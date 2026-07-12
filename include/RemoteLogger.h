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

public slots:
    void init();
    void start();
    void stop();
    void clear();
    void append(const QString &text);
    void setInterval(int ms);
    void setMaxLines(int lines);

private slots:    
    void replyFinished(QNetworkReply *reply);    

private:

    void upload();
    QNetworkAccessManager *m_manager = nullptr; // Změna na ukazatel
    QTimer *m_timer = nullptr;                  // Změna na ukazatel

    QStringList m_lines;

    QString m_token;
    QString m_gistId;
    QString m_fileName = "log.txt";

    int m_maxLines = 500;
    int m_pendingInterval = 1000; // Pomocná proměnná pro uchování intervalu před init()

    bool m_dirty = false;
    bool m_uploadInProgress = false;
};

#endif // REMOTELOGGER_H
