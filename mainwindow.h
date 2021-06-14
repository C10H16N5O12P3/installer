#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class QLabel;

struct ReleaseInfo
{
    QString tagName;
    bool isPrerelease;
    QString releaseName;
    QString publishTime;
    QString body;
    QUrl linux64Url;
    QUrl windows32Url;
    QUrl windows64Url;
    QUrl webglUrl;

    ReleaseInfo(QJsonObject const& obj);

    inline QUrl const& getCurrentOsUrl()
    {
#if defined(Q_OS_WIN32)
        return windows32Url;
#elif defined(Q_OS_WIN64)
        return windows64Url;
#elif defined(Q_OS_LINUX)
        return linux64Url;
#endif
    }
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    QNetworkAccessManager* m_NetworkMgr;
    std::vector<ReleaseInfo> m_Releases;
    std::vector<int> m_ReleaseFiltered;

private:
    void startDownload(QUrl const& url, std::function<void(QNetworkReply*)> callback);
    void setLabelImage(QLabel* label, QString path);
    QJsonDocument getReleasesJson();
    void updateReleaseList();
    void fetchVersions();
    void applyFilter();
    void updateVersionCombobox();
    ReleaseInfo const& getSelectedRelease();

public slots:
    void sslError(QNetworkReply* reply, const QList<QSslError> &errors);
    void onDownloadBtnClick();
    void onRefreshBtnClick();
    void onPlayBtnClick();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onVersionIndexChanged(int idx);
};
#endif // MAINWINDOW_H
