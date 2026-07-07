#ifndef REMOTELOGGER_H
#define REMOTELOGGER_H

//#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QStringList>

class QNetworkReply;

class RemoteLogger : public QObject
{
    Q_OBJECT

public:
    explicit RemoteLogger(QObject *parent = nullptr);

    void setToken(const QString &token);
    void setGistId(const QString &gistId);
    void setFileName(const QString &fileName);

    void setInterval(int ms);
    void setMaxLines(int lines);

    void start();
    void stop();
    void clear();

    void append(const QString &text);

private slots:
    void upload();
    void replyFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager m_manager;
    QTimer m_timer;

    QStringList m_lines;

    QString m_token;
    QString m_gistId;
    QString m_fileName = "log.txt";

    int m_maxLines = 500;

    bool m_dirty = false;
    bool m_uploadInProgress = false;
};

#endif // REMOTELOGGER_H
