#ifndef APPUPDATER_H
#define APPUPDATER_H

#include <QObject>
#include <QString>
#include <QStringList>

class QNetworkAccessManager;
class QNetworkReply;

class AppUpdater : public QObject
{
    Q_OBJECT

public:
    explicit AppUpdater(QObject *parent = nullptr);

    void setUpdateSource(const QString &source);
    void setCurrentVersion(const QString &currentVersion);
    void setDefaultInstallerArguments(const QStringList &arguments);
    void setSignaturePolicy(bool requireValidSignature, const QString &expectedPublisher);

    void checkForUpdates(bool userInitiated);
    void downloadAndInstall(const QString &installerUrl,
                            const QString &expectedSha256,
                            const QString &expectedPublisher,
                            bool userInitiated);

private:
    static QString normalizedVersion(const QString &raw);
    static int compareVersions(const QString &left, const QString &right);
    QString buildDownloadPath(const QString &installerUrl) const;

    QString m_updateSource;
    QString m_currentVersion;
    QStringList m_defaultInstallerArguments;
    bool m_requireValidSignature;
    QString m_expectedPublisher;
    QNetworkAccessManager *m_network;
    bool m_busy;

signals:
    void updateAvailable(const QString &latestVersion,
                         const QString &installerUrl,
                         const QString &releaseNotes,
                         const QString &sha256,
                         const QString &publisher,
                         const QString &sourceType,
                         bool userInitiated);
    void noUpdateAvailable(const QString &currentVersion, bool userInitiated);
    void checkFailed(const QString &message, bool userInitiated);
    void installStarted(const QString &installerPath, bool userInitiated);
    void installFailed(const QString &message, bool userInitiated);
};

#endif // APPUPDATER_H
