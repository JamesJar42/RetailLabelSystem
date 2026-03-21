#include "../include/AppUpdater.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>
#include <QVersionNumber>

namespace {

const QString kAllowedRepoOwner = QStringLiteral("JamesJar42");
const QString kAllowedRepoName = QStringLiteral("RetailLabelSystem");
const QString kAllowedRepoFull = QStringLiteral("JamesJar42/RetailLabelSystem");
const QString kAllowedLatestApiUrl = QStringLiteral("https://api.github.com/repos/JamesJar42/RetailLabelSystem/releases/latest");

bool matchesAllowedRepo(const QString &owner, const QString &repo)
{
    return owner.compare(kAllowedRepoOwner, Qt::CaseInsensitive) == 0
        && repo.compare(kAllowedRepoName, Qt::CaseInsensitive) == 0;
}

struct ParsedReleaseInfo {
    bool ok = false;
    QString error;
    QString version;
    QString installerUrl;
    QString notes;
    QString sha256;
    QString publisher;
    QString sourceType;
    QString checksumAssetUrl;
};

QString toGithubLatestUrl(const QString &raw)
{
    QString input = raw.trimmed();
    if (input.isEmpty()) {
        return QString();
    }

    if (input.startsWith("github:", Qt::CaseInsensitive)) {
        QString repo = input.mid(QStringLiteral("github:").size()).trimmed();
        if (repo.startsWith('/')) {
            repo = repo.mid(1);
        }
        const QStringList parts = repo.split('/', Qt::SkipEmptyParts);
        if (parts.size() == 2 && matchesAllowedRepo(parts[0], parts[1])) {
            return kAllowedLatestApiUrl;
        }
        return QString();
    }

    if (input.startsWith("https://api.github.com/repos/", Qt::CaseInsensitive)) {
        const QUrl apiUrl(input);
        const QStringList parts = apiUrl.path().split('/', Qt::SkipEmptyParts);
        if (parts.size() >= 5
            && parts[0].compare("repos", Qt::CaseInsensitive) == 0
            && matchesAllowedRepo(parts[1], parts[2])
            && parts[3].compare("releases", Qt::CaseInsensitive) == 0
            && parts[4].compare("latest", Qt::CaseInsensitive) == 0) {
            return kAllowedLatestApiUrl;
        }
        return QString();
    }

    if (input.startsWith("https://github.com/", Qt::CaseInsensitive)) {
        const QUrl webUrl(input);
        const QStringList parts = webUrl.path().split('/', Qt::SkipEmptyParts);
        if (parts.size() >= 4
            && matchesAllowedRepo(parts[0], parts[1])
            && parts[2].compare("releases", Qt::CaseInsensitive) == 0
            && parts[3].compare("latest", Qt::CaseInsensitive) == 0) {
            return kAllowedLatestApiUrl;
        }
        return QString();
    }

    return QString();
}

QString pickInstallerFromAssets(const QJsonArray &assets)
{
    int bestScore = -1;
    QString bestUrl;

    for (const QJsonValue &assetValue : assets) {
        if (!assetValue.isObject()) continue;
        const QJsonObject asset = assetValue.toObject();
        const QString url = asset.value(QStringLiteral("browser_download_url")).toString().trimmed();
        const QString name = asset.value(QStringLiteral("name")).toString().trimmed().toLower();
        if (url.isEmpty() || name.isEmpty()) continue;

        int score = 0;
        if (name.endsWith(QStringLiteral(".exe")) || name.endsWith(QStringLiteral(".msi"))) score += 100;
        if (name.contains(QStringLiteral("win")) || name.contains(QStringLiteral("windows"))) score += 20;
        if (name.contains(QStringLiteral("x64")) || name.contains(QStringLiteral("amd64"))) score += 10;
        if (name.contains(QStringLiteral("setup")) || name.contains(QStringLiteral("installer"))) score += 10;
        if (score > bestScore) {
            bestScore = score;
            bestUrl = url;
        }
    }

    return bestUrl;
}

QString pickChecksumAssetUrl(const QJsonArray &assets)
{
    int bestScore = -1;
    QString bestUrl;

    for (const QJsonValue &assetValue : assets) {
        if (!assetValue.isObject()) continue;
        const QJsonObject asset = assetValue.toObject();
        const QString url = asset.value(QStringLiteral("browser_download_url")).toString().trimmed();
        const QString name = asset.value(QStringLiteral("name")).toString().trimmed().toLower();
        if (url.isEmpty() || name.isEmpty()) continue;

        int score = -1;
        if (name.endsWith(QStringLiteral(".sha256"))) score = 120;
        else if (name.endsWith(QStringLiteral(".sha256.txt"))) score = 110;
        else if (name.contains(QStringLiteral("sha256")) && name.endsWith(QStringLiteral(".txt"))) score = 100;
        else if (name.contains(QStringLiteral("checksum")) && name.endsWith(QStringLiteral(".txt"))) score = 90;

        if (score > bestScore) {
            bestScore = score;
            bestUrl = url;
        }
    }

    return bestUrl;
}

QString extractSha256FromText(const QByteArray &textData, const QString &installerUrl)
{
    const QString text = QString::fromUtf8(textData);
    const QString installerName = QFileInfo(QUrl(installerUrl).path()).fileName().toLower();
    const QStringList lines = text.split(QRegularExpression("[\\r\\n]+"), Qt::SkipEmptyParts);
    QRegularExpression hashRegex(QStringLiteral("([0-9a-fA-F]{64})"));

    QString fallbackHash;
    for (const QString &line : lines) {
        const QRegularExpressionMatch match = hashRegex.match(line);
        if (!match.hasMatch()) continue;
        const QString hash = match.captured(1).toLower();
        if (fallbackHash.isEmpty()) {
            fallbackHash = hash;
        }

        if (!installerName.isEmpty() && line.toLower().contains(installerName)) {
            return hash;
        }
    }

    return fallbackHash;
}

ParsedReleaseInfo parseReleaseInfo(const QJsonObject &obj)
{
    ParsedReleaseInfo result;

    const QString githubTag = obj.value(QStringLiteral("tag_name")).toString().trimmed();
    if (!githubTag.isEmpty() && obj.value(QStringLiteral("assets")).isArray()) {
        const QJsonArray assets = obj.value(QStringLiteral("assets")).toArray();
        result.version = githubTag;
        result.installerUrl = pickInstallerFromAssets(assets);
        result.notes = obj.value(QStringLiteral("body")).toString();
        result.sha256 = obj.value(QStringLiteral("sha256")).toString().trimmed().toLower();
        result.publisher = obj.value(QStringLiteral("publisher")).toString().trimmed();
        result.checksumAssetUrl = pickChecksumAssetUrl(assets);
        result.sourceType = QStringLiteral("GitHub Releases");
        if (result.installerUrl.isEmpty()) {
            result.error = QStringLiteral("GitHub release does not include a Windows installer asset (.exe or .msi).");
            return result;
        }
        result.ok = true;
        return result;
    }

    result.error = QStringLiteral("Update source response is not a GitHub releases/latest payload (missing tag_name/assets).");
    return result;
}

#ifdef Q_OS_WIN
bool verifyInstallerSignature(const QString &installerPath,
                              const QString &expectedPublisher,
                              QString &errorOut)
{
    QString escapedPath = installerPath;
    escapedPath.replace('\'', "''");

    QProcess statusProcess;
    const QString statusScript =
        QStringLiteral("(Get-AuthenticodeSignature -LiteralPath '%1').Status")
            .arg(escapedPath);
    statusProcess.start(QStringLiteral("powershell.exe"),
                        {QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"), QStringLiteral("-Command"), statusScript});
    if (!statusProcess.waitForFinished(20000)) {
        errorOut = QStringLiteral("Timed out while verifying installer signature.");
        return false;
    }

    const QString status = QString::fromLocal8Bit(statusProcess.readAllStandardOutput()).trimmed();
    if (status.compare(QStringLiteral("Valid"), Qt::CaseInsensitive) != 0) {
        const QString stdErr = QString::fromLocal8Bit(statusProcess.readAllStandardError()).trimmed();
        errorOut = stdErr.isEmpty()
            ? QStringLiteral("Installer signature is not valid (status: %1)." ).arg(status.isEmpty() ? QStringLiteral("Unknown") : status)
            : QStringLiteral("Installer signature verification failed: %1").arg(stdErr);
        return false;
    }

    const QString expected = expectedPublisher.trimmed();
    if (expected.isEmpty()) {
        return true;
    }

    QProcess subjectProcess;
    const QString subjectScript =
        QStringLiteral("(Get-AuthenticodeSignature -LiteralPath '%1').SignerCertificate.Subject")
            .arg(escapedPath);
    subjectProcess.start(QStringLiteral("powershell.exe"),
                         {QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"), QStringLiteral("-Command"), subjectScript});
    if (!subjectProcess.waitForFinished(20000)) {
        errorOut = QStringLiteral("Timed out while verifying installer publisher.");
        return false;
    }

    const QString subject = QString::fromLocal8Bit(subjectProcess.readAllStandardOutput()).trimmed();
    if (!subject.contains(expected, Qt::CaseInsensitive)) {
        errorOut = QStringLiteral("Installer signer does not match expected publisher. Expected contains '%1', got '%2'.")
                       .arg(expected, subject.isEmpty() ? QStringLiteral("<none>") : subject);
        return false;
    }

    return true;
}
#else
bool verifyInstallerSignature(const QString &, const QString &, QString &)
{
    return true;
}
#endif

} // namespace

AppUpdater::AppUpdater(QObject *parent)
    : QObject(parent),
      m_requireValidSignature(true),
      m_network(new QNetworkAccessManager(this)),
      m_busy(false)
{
}

void AppUpdater::setUpdateSource(const QString &source)
{
    m_updateSource = toGithubLatestUrl(source);
}

void AppUpdater::setCurrentVersion(const QString &currentVersion)
{
    m_currentVersion = normalizedVersion(currentVersion);
}

void AppUpdater::setDefaultInstallerArguments(const QStringList &arguments)
{
    m_defaultInstallerArguments = arguments;
}

void AppUpdater::setSignaturePolicy(bool requireValidSignature, const QString &expectedPublisher)
{
    m_requireValidSignature = requireValidSignature;
    m_expectedPublisher = expectedPublisher.trimmed();
}

QString AppUpdater::normalizedVersion(const QString &raw)
{
    QString cleaned = raw.trimmed();
    if (cleaned.startsWith('v', Qt::CaseInsensitive)) {
        cleaned = cleaned.mid(1);
    }
    return cleaned;
}

int AppUpdater::compareVersions(const QString &left, const QString &right)
{
    const QVersionNumber lhs = QVersionNumber::fromString(normalizedVersion(left));
    const QVersionNumber rhs = QVersionNumber::fromString(normalizedVersion(right));

    if (!lhs.isNull() && !rhs.isNull()) {
        return QVersionNumber::compare(lhs, rhs);
    }

    return QString::compare(normalizedVersion(left), normalizedVersion(right), Qt::CaseInsensitive);
}

QString AppUpdater::buildDownloadPath(const QString &installerUrl) const
{
    const QUrl url(installerUrl);
    QString fileName = QFileInfo(url.path()).fileName();
    if (fileName.isEmpty()) {
        fileName = QStringLiteral("RetailLabeler-Update.exe");
    }

    QString tempRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (tempRoot.isEmpty()) {
        tempRoot = QDir::tempPath();
    }

    QDir dir(tempRoot);
    dir.mkpath(QStringLiteral("RetailLabelerUpdater"));
    return dir.filePath(QStringLiteral("RetailLabelerUpdater/%1").arg(fileName));
}

void AppUpdater::checkForUpdates(bool userInitiated)
{
    if (m_busy) {
        if (userInitiated) {
            emit checkFailed(QStringLiteral("Update check already in progress."), true);
        }
        return;
    }

    if (m_updateSource.isEmpty()) {
        if (userInitiated) {
            emit checkFailed(QStringLiteral("Updater is restricted to github:%1.").arg(kAllowedRepoFull), true);
        }
        return;
    }

    const QUrl sourceUrl(m_updateSource);
    if (!sourceUrl.isValid()) {
        emit checkFailed(QStringLiteral("Updater URL is invalid or not allowed. Only github:%1 is supported.").arg(kAllowedRepoFull), userInitiated);
        return;
    }

    m_busy = true;
    QNetworkRequest request(sourceUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("RetailLabeler-Updater"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, userInitiated]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            m_busy = false;
            emit checkFailed(QStringLiteral("Update check failed: %1").arg(reply->errorString()), userInitiated);
            return;
        }

        QJsonParseError parseError;
        const QByteArray payload = reply->readAll();
        const QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            m_busy = false;
            emit checkFailed(QStringLiteral("Update source response is not valid JSON."), userInitiated);
            return;
        }

        const QJsonObject obj = doc.object();
        const ParsedReleaseInfo parsed = parseReleaseInfo(obj);
        if (!parsed.ok) {
            m_busy = false;
            emit checkFailed(parsed.error, userInitiated);
            return;
        }

        const QString latestVersion = normalizedVersion(parsed.version);
        const QString installerUrl = parsed.installerUrl;

        if (compareVersions(latestVersion, m_currentVersion) <= 0) {
            m_busy = false;
            emit noUpdateAvailable(m_currentVersion, userInitiated);
            return;
        }

        if (parsed.sha256.isEmpty() && !parsed.checksumAssetUrl.isEmpty()) {
            QNetworkRequest checksumReq(QUrl(parsed.checksumAssetUrl));
            checksumReq.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("RetailLabeler-Updater"));
            checksumReq.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

            QNetworkReply *checksumReply = m_network->get(checksumReq);
            connect(checksumReply, &QNetworkReply::finished, this,
                    [this, checksumReply, parsed, latestVersion, installerUrl, userInitiated]() {
                checksumReply->deleteLater();
                QString resolvedSha = parsed.sha256;

                if (checksumReply->error() == QNetworkReply::NoError) {
                    resolvedSha = extractSha256FromText(checksumReply->readAll(), installerUrl);
                }

                m_busy = false;
                emit updateAvailable(latestVersion,
                                     installerUrl,
                                     parsed.notes,
                                     resolvedSha,
                                     parsed.publisher,
                                     parsed.sourceType,
                                     userInitiated);
            });
            return;
        }

        m_busy = false;

        emit updateAvailable(latestVersion,
                             installerUrl,
                             parsed.notes,
                             parsed.sha256,
                             parsed.publisher,
                             parsed.sourceType,
                             userInitiated);
    });
}

void AppUpdater::downloadAndInstall(const QString &installerUrl,
                                    const QString &expectedSha256,
                                    const QString &expectedPublisher,
                                    bool userInitiated)
{
    if (m_busy) {
        emit installFailed(QStringLiteral("Another updater operation is already in progress."), userInitiated);
        return;
    }

    if (installerUrl.trimmed().isEmpty()) {
        emit installFailed(QStringLiteral("Installer URL is empty."), userInitiated);
        return;
    }

    const QUrl url(installerUrl);
    if (!url.isValid()) {
        emit installFailed(QStringLiteral("Installer URL is invalid."), userInitiated);
        return;
    }

    m_busy = true;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("RetailLabeler-Updater"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, expectedSha256, expectedPublisher, userInitiated, installerUrl]() {
        reply->deleteLater();
        m_busy = false;

        if (reply->error() != QNetworkReply::NoError) {
            emit installFailed(QStringLiteral("Update download failed: %1").arg(reply->errorString()), userInitiated);
            return;
        }

        const QByteArray content = reply->readAll();
        if (content.isEmpty()) {
            emit installFailed(QStringLiteral("Downloaded installer is empty."), userInitiated);
            return;
        }

        const QString downloadPath = buildDownloadPath(installerUrl);
        QSaveFile file(downloadPath);
        if (!file.open(QIODevice::WriteOnly)) {
            emit installFailed(QStringLiteral("Cannot write installer: %1").arg(file.errorString()), userInitiated);
            return;
        }

        if (file.write(content) != content.size()) {
            emit installFailed(QStringLiteral("Failed while writing installer to disk."), userInitiated);
            return;
        }

        if (!file.commit()) {
            emit installFailed(QStringLiteral("Failed to finalize installer file."), userInitiated);
            return;
        }

        const QString normalizedHash = expectedSha256.trimmed().toLower();
        if (!normalizedHash.isEmpty()) {
            QFile verifyFile(downloadPath);
            if (!verifyFile.open(QIODevice::ReadOnly)) {
                emit installFailed(QStringLiteral("Unable to verify installer hash."), userInitiated);
                return;
            }
            const QByteArray hash = QCryptographicHash::hash(verifyFile.readAll(), QCryptographicHash::Sha256).toHex().toLower();
            if (QString::fromLatin1(hash) != normalizedHash) {
                emit installFailed(QStringLiteral("Installer checksum mismatch."), userInitiated);
                return;
            }
        }

        if (m_requireValidSignature) {
            const QString publisher = expectedPublisher.trimmed().isEmpty() ? m_expectedPublisher : expectedPublisher.trimmed();
            QString signatureError;
            if (!verifyInstallerSignature(downloadPath, publisher, signatureError)) {
                emit installFailed(signatureError, userInitiated);
                return;
            }
        }

        QFileInfo installerInfo(downloadPath);
        QString program = installerInfo.absoluteFilePath();
        QStringList arguments = m_defaultInstallerArguments;

        if (installerInfo.suffix().compare(QStringLiteral("msi"), Qt::CaseInsensitive) == 0) {
            program = QStringLiteral("msiexec.exe");
            if (arguments.isEmpty()) {
                arguments << QStringLiteral("/i") << installerInfo.absoluteFilePath();
            } else {
                arguments.prepend(installerInfo.absoluteFilePath());
                arguments.prepend(QStringLiteral("/i"));
            }
        }

        if (!QProcess::startDetached(program, arguments, installerInfo.absolutePath())) {
            emit installFailed(QStringLiteral("Could not launch installer."), userInitiated);
            return;
        }

        emit installStarted(downloadPath, userInitiated);
    });
}
