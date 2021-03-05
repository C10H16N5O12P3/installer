#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QtWidgets/QMessageBox>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QFile>
#include <QDir>
#include <QProcess>
#include <zip.h>

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

QString getInstallationPath(QString tag, bool createDir)
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::StandardLocation::DataLocation) + "/" + tag;

    if (createDir)
    {
        QDir dir(path);
        if (!dir.exists())
            dir.mkdir(path);
    }

    return path;
}

QString getExecPath(QString tag)
{
    return getInstallationPath(tag, false) + "/" EXEC_FILE;
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
        auto instPath = getInstallationPath(release.tagName, false);

        QDir dir(instPath);
        dir.removeRecursively();

        updateVersionCombobox();
    }
    else
    { 
        startDownload(release.getCurrentOsUrl(), [this, release] (QNetworkReply* reply)
        {
            QString instPath = getInstallationPath(release.tagName, true);
            QString execPath = getExecPath(release.tagName);

            // create persisent directory
            auto data = reply->readAll();
            QDir dir(instPath);
            if (!dir.exists())
                dir.mkdir(instPath);


            // create zip file
            zip_error_t err;

            zip_source_t* src = zip_source_buffer_create(data.data(), data.size(), 0, &err);
            zip_t* zipFile = zip_open_from_source(src, ZIP_RDONLY, &err);

            struct zip_stat stat;
            zip_int64_t count = zip_get_num_entries(zipFile, 0);

            ui->progressBar->setMaximum(count);

            for (zip_int64_t i = 0; i < count; i++)
            {
                ui->progressBar->setValue(i);

                // get file info
                zip_stat_init(&stat);
                zip_stat_index(zipFile, i, 0, &stat);

                // create directory
                ssize_t pathLen = strlen(stat.name);
                for (ssize_t i = pathLen - 1; i >= 0; i--)
                    if (stat.name[i] == '/'){
                        dir.mkpath(QString(stat.name).left(i));
                        break;
                    }

                // check if directory
                if (stat.name[pathLen-1] == '/')
                    continue;


                char* data = new char[stat.size];

                zip_file_t* entry = zip_fopen_index(zipFile, i, 0);
                zip_fread(entry, data, stat.size);

                // write the decompressed file
                QFile file(instPath + "/" + stat.name);
                file.open(QIODevice::WriteOnly);
                file.write(data, stat.size);
                file.close();

                zip_fclose(entry);

                delete[] data;
            }

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