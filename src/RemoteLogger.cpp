#include "RemoteLogger.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

RemoteLogger::RemoteLogger(QObject *parent)
    : QObject(parent)
{

}

void RemoteLogger::init()
{
    m_timer = new QTimer(this);
    m_manager = new QNetworkAccessManager(this);

    // Aplikujeme interval, který byl nastaven ještě před startem vlákna
    m_timer->setInterval(m_pendingInterval);

    connect(m_timer, &QTimer::timeout, this, &RemoteLogger::upload);
    connect(m_manager, &QNetworkAccessManager::finished, this, &RemoteLogger::replyFinished);
}

void RemoteLogger::setToken(const QString &token)
{
    m_token = token;
}

void RemoteLogger::setGistId(const QString &gistId)
{
    m_gistId = gistId;
}

void RemoteLogger::setFileName(const QString &fileName)
{
    m_fileName = fileName;
}

void RemoteLogger::setInterval(int ms)
{
    m_pendingInterval = ms;
    if (m_timer) {
        m_timer->setInterval(ms);
    }
}

void RemoteLogger::setMaxLines(int lines)
{
    m_maxLines = lines;
}

void RemoteLogger::start()
{
    if (!m_timer) return;
    if (m_timer->interval() == 0) m_timer->setInterval(1000);
    m_timer->start();
}

void RemoteLogger::stop()
{
    if (m_timer) m_timer->stop();
    upload();

    if (m_manager) {
        m_manager->clearConnectionCache();
    }
}

void RemoteLogger::append(const QString &text)
{
    m_lines.append(text);

    while (m_lines.size() > m_maxLines)
        m_lines.removeFirst();

    m_dirty = true;
}

// Vrátili jsme na typ void - nepotřebujeme už vracet ukazatel
void RemoteLogger::upload()
{
    if (!m_dirty || m_uploadInProgress || !m_manager) return;
    if (m_token.isEmpty() || m_gistId.isEmpty()) return;

    QString log = m_lines.join('\n');

    QJsonObject file;
    file["content"] = log;

    QJsonObject files;
    files[m_fileName] = file;

    QJsonObject root;
    root["files"] = files;

    QByteArray data = QJsonDocument(root).toJson();

    QNetworkRequest req(
        QUrl("https://api.github.com/gists/" + m_gistId));

    req.setRawHeader("Authorization",
                     ("Bearer " + m_token).toUtf8());

    req.setRawHeader("Accept",
                     "application/vnd.github+json");

    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  "application/json");

    // Říká serveru i Qt: po této odpovědi socket hned zavřete, nečekejte 30s Keep-Alive
    req.setRawHeader("Connection", "close");

    m_uploadInProgress = true;

    m_manager->sendCustomRequest(req, "PATCH", data);
}

void RemoteLogger::replyFinished(QNetworkReply *reply)
{
    m_uploadInProgress = false;

    // BEZPEČNOSTNÍ KONTROLA: Pokud reply už neexistuje nebo nastal tvrdý rozpad spojení
    if (!reply)
        return;

    int status =
        reply->attribute(
                 QNetworkRequest::HttpStatusCodeAttribute)
            .toInt();

    if (reply->error() == QNetworkReply::NoError) {
        m_dirty = false;
    }
    else {
        qDebug() << "Gist upload error, status: "
                 << status << ", "
                 << reply->errorString();

        // Čteme, jen pokud je to bezpečné a socket nepadl dřív, než se otevřel
        if (reply->isOpen() && reply->isReadable()) {
            qDebug() << reply->readAll();
        }
    }

    reply->deleteLater();
}

void RemoteLogger::clear()
{
    m_lines.clear();
    m_dirty = true;
}
