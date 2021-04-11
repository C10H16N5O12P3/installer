#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QtWidgets/QMessageBox>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QFile>
#include <QDir>
#include <QProcess>
#include "3rdParty/zip.h"

#define RELEASES_URL    "https://api.github.com/repos/C10H16N5O12P3/BeyondStyx-release/releases"
#ifdef Q_OS_WINDOWS
#define EXEC_FILE   "BeyondStyx.exe"
#else
#define EXEC_FILE   "BeyondStyx"
#endif

ReleaseInfo::ReleaseInfo(QJsonObject const& obj)
{
    tagName = obj["tag_name"].toString();
    isPrerelease = obj["prerelease"].toBool();
    releaseName = obj["name"].toString();
    publishTime = obj["published_at"].toString();
    body = obj["body"].toString();

    QJsonArray assets = obj["assets"].toArray();

    foreach (QJsonValue const& asset, assets)
    {
        QJsonObject assetObj = asset.toObject();
        QString assetName = assetObj["name"].toString();
        QString assetUrl = assetObj["browser_download_url"].toString();

        if (assetName.startsWith("linux-x86_64"))
            linux64Url = assetUrl;
        else if (assetName.startsWith("windows-x86_64"))
            windows64Url = assetUrl;
        else if (assetName.startsWith("windows-x86"))
            windows32Url = assetUrl;
        else if (assetName.startsWith("webgl"))
            webglUrl = assetUrl;
    }
}

QString getInstallationPath(QString tag)
{
    return QStandardPaths::writableLocation(QStandardPaths::StandardLocation::DataLocation) + "/" + tag;
}

QString getExecPath(QString tag)
{
    return getInstallationPath(tag) + "/" EXEC_FILE;
}

bool isVersionInstalled(QString tag)
{
    QFile file(getExecPath(tag));
    return file.exists();
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_NetworkMgr(new QNetworkAccessManager),
    m_Releases(),
    m_ReleaseFiltered()
{
    ui->setupUi(this);

    ui->progressBar->setVisible(false);
    ui->progressBar->setValue(0);

    ui->playBtn->setVisible(false);


    fetchVersions();
}

MainWindow::~MainWindow()
{
    delete ui;
    delete m_NetworkMgr;
}

void MainWindow::setLabelImage(QLabel* label, QString path)
{
    QPixmap image(path);
    image = image.scaled(label->size().width(), label->size().height(), Qt::AspectRatioMode::KeepAspectRatio);
    label->setPixmap(image);
}

void MainWindow::startDownload(QUrl const& url, std::function<void(QNetworkReply*)> callback)
{
    ui->progressBar->setVisible(true);
    this->setEnabled(false);

    QNetworkRequest req;
    req.setUrl(url);
    req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);

    QObject *context = new QObject(this);
    connect(m_NetworkMgr, &QNetworkAccessManager::finished, context, [context, callback, this] (QNetworkReply* reply) {

        callback(reply);

        ui->progressBar->setVisible(false);
        this->setEnabled(true);

        delete context;
    });


    QNetworkReply* reply = m_NetworkMgr->get(req);
    connect(reply, &QNetworkReply::downloadProgress, this, &MainWindow::onDownloadProgress, Qt::ConnectionType::UniqueConnection);
}

void MainWindow::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    ui->progressBar->setMaximum(bytesTotal);
    ui->progressBar->setValue(bytesReceived);
}

void MainWindow::fetchVersions()
{
    startDownload(QUrl(RELEASES_URL), [this] (QNetworkReply* reply)
    {
        m_Releases.clear();

        QString replyStr = (QString)reply->readAll();
        QJsonDocument json = QJsonDocument::fromJson(replyStr.toUtf8());

        QJsonArray array = json.array();
        foreach (QJsonValue const& value, array)
            m_Releases.push_back(ReleaseInfo(value.toObject()));

        applyFilter();
    });
}

void MainWindow::applyFilter()
{
    m_ReleaseFiltered.clear();
    
    for (size_t i = 0; i < m_Releases.size(); i++)
    {
        // todo: add condition
        m_ReleaseFiltered.push_back(i);
    }

    updateReleaseList();
}

void MainWindow::updateVersionCombobox()
{
    int versionIdx = ui->versionComboBox->currentIndex();
    updateReleaseList();
    ui->versionComboBox->setCurrentIndex(versionIdx);
    //onVersionIndexChanged(ui->versionComboBox->currentIndex());
}

void MainWindow::updateReleaseList()
{
    ui->versionComboBox->clear();
 
    for (auto const& relIdx : m_ReleaseFiltered)
    {
        ReleaseInfo const& rel = m_Releases[relIdx];
        ui->versionComboBox->addItem(rel.tagName + (isVersionInstalled(rel.tagName)
            ? " [installed]"
            : ""));
    }
}

ReleaseInfo const& MainWindow::getSelectedRelease()
{
    return m_Releases[m_ReleaseFiltered[ui->versionComboBox->currentIndex()]];
}


void MainWindow::onDownloadBtnClick()
{
    auto release = getSelectedRelease();

    if (isVersionInstalled(release.tagName))
    {
        auto instPath = getInstallationPath(release.tagName);

        QDir dir(instPath);
        dir.removeRecursively();

        updateVersionCombobox();
    }
    else
    { 
        startDownload(release.getCurrentOsUrl(), [this, release] (QNetworkReply* reply)
        {
            QString instPath = getInstallationPath(release.tagName);
            QString execPath = getExecPath(release.tagName);

            // create persisent directory
            QDir dir(instPath);
            if (!dir.exists())
                dir.mkpath(instPath);

            auto data = reply->readAll();


            zip_t* zip = zip_stream_open(data.data(), data.size(), ZIP_DEFAULT_COMPRESSION_LEVEL, 'r');

            int count = zip_entries_total(zip);
            ui->progressBar->setMaximum(count);
            
            char* buf = NULL;
            size_t bufsize = 0;

            for (int i = 0; i < count; i++)
            {
                zip_entry_openbyindex(zip, i);

                ui->progressBar->setValue(i);

                if (!zip_entry_isdir(zip))
                {
                    const char* fileName = zip_entry_name(zip);

                    // create directory
                    ssize_t pathLen = strlen(fileName);
                    for (ssize_t i = pathLen - 1; i >= 0; i--)
                        if (fileName[i] == '/'){
                            dir.mkpath(QString(fileName).left(i));
                            break;
                        }

                    
                    zip_entry_read(zip, reinterpret_cast<void**>(&buf), &bufsize);

                    // write the decompressed file
                    QFile file(instPath + "/" + fileName);
                    file.open(QIODevice::WriteOnly);
                    file.write(buf, bufsize);
                    file.close();
                }

                zip_entry_close(zip);
            }
            
            zip_close(zip);
            
            // set exec permission
            auto oldPerm = QFile::permissions(execPath);
            QFile::setPermissions(execPath, oldPerm | QFileDevice::Permission::ExeUser);

            updateVersionCombobox();
        });
    }
}

void MainWindow::onRefreshBtnClick()
{
    fetchVersions();
}

void MainWindow::onVersionIndexChanged(int idx)
{
    bool valid = idx >= 0 && m_Releases.size() > 0;
    if (valid)
    {
        auto release = getSelectedRelease();
        bool isInstalled = isVersionInstalled(release.tagName);

        ui->downloadBtn->setText(isInstalled
            ? "Uninstall"
            : "Install");
        
        ui->playBtn->setVisible(isInstalled);
    }

    ui->playBtn->setEnabled(valid);
    ui->downloadBtn->setEnabled(valid);
}

void MainWindow::onPlayBtnClick()
{
    auto release = getSelectedRelease();

    QProcess* proc = new QProcess(this);
    proc->start(getExecPath(release.tagName), QStringList());
}