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
    connect(&m_timer,
            &QTimer::timeout,
            this,
            &RemoteLogger::upload);

    connect(&m_manager,
            &QNetworkAccessManager::finished,
            this,
            &RemoteLogger::replyFinished);
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
    m_timer.setInterval(ms);
}

void RemoteLogger::setMaxLines(int lines)
{
    m_maxLines = lines;
}

void RemoteLogger::start()
{
    if (m_timer.interval() == 0)
        m_timer.setInterval(1000);

    m_timer.start();
}

void RemoteLogger::stop()
{
    m_timer.stop();
}

void RemoteLogger::append(const QString &text)
{
    /*
    QString line =
        QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz ")
        + text;
    */

    m_lines.append(text);

    while (m_lines.size() > m_maxLines)
        m_lines.removeFirst();

    m_dirty = true;
}

void RemoteLogger::upload()
{
    if (!m_dirty)
        return;

    if (m_uploadInProgress)
        return;

    if (m_token.isEmpty() || m_gistId.isEmpty())
        return;

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

    m_uploadInProgress = true;

    m_manager.sendCustomRequest(req, "PATCH", data);
}

void RemoteLogger::replyFinished(QNetworkReply *reply)
{
    m_uploadInProgress = false;

    int status =
        reply->attribute(
                 QNetworkRequest::HttpStatusCodeAttribute)
            .toInt();
/*
    if (status == 403)
    {
        qDebug() << "GitHub rate limit - disabling uploads";

        m_timer.stop();
    }
*/
    if (reply->error() == QNetworkReply::NoError) {
        m_dirty = false;
    }
    else {
        qDebug() << "Gist upload error, status: "
                 << status << ", "
                 << reply->errorString();

        qDebug() << reply->readAll();
    }

    reply->deleteLater();
}

void RemoteLogger::clear()
{
    m_lines.clear();
    m_dirty = true;
}