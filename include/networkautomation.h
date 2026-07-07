#ifndef NETWORKAUTOMATON_H
#define NETWORKAUTOMATON_H

#include <QObject>
#include <QTimer>
#include <QUdpSocket>
#include <QHostAddress>
#include <QDateTime>
#include <QFile>
#include <QTextStream>

#include "RemoteLogger.h"

class NetworkAutomaton : public QObject
{
    Q_OBJECT

public:
    explicit NetworkAutomaton(QObject *parent = nullptr);
    ~NetworkAutomaton();

    // Struktura pro předání konfigurace z UI
    struct Config {
        double activePeriodBase; // spinBox_1 (s)
        double activePeriodRand; // spinBox_2 (s)
        double udpDelay;         // spinBox_3 (s)
        double udpPeriod;        // spinBox_4 (s)
        double inactiveBase;     // spinBox_5 (s)
        double inactiveRand;     // spinBox_6 (s)
    };

    void start(const Config &config);
    void stop();
    bool isActive() const { return m_isRunning; }

private slots:
    void onActivePeriodTimeout();
    void onInactivePeriodTimeout();
    void onUdpDelayTimeout();
    void onUdpSendTimeout();
    void onReadyRead();

private:
    enum class State {
        Idle,
        Active,
        Inactive
    };

    double getRandomValue(double base, double maxRand);
    void sendUdpPacket();

    // Stav a konfigurace
    State m_state;
    bool m_isRunning;
    Config m_config;

    // Časovače
    QTimer *m_stateTimer;    // Pro řízení aktivní/neaktivní periody
    QTimer *m_udpDelayTimer; // Pro zpoždění prvního UDP paketu
    QTimer *m_udpSendTimer;  // Pro periodické posílání UDP paketů

    // Síť
    QUdpSocket *m_udpSocket;
    const QHostAddress m_targetAddress;
    const quint16 m_targetPort;
    const quint16 m_listenPort;
    int m_sentPacketsCount = 0;     // Počítadlo odeslaných
    int m_receivedPacketsCount = 0; // Počítadlo přijatých

    void logToFile(const QString &message);
    QString m_logFileName; // Uloží název souboru vygenerovaný při startu

    RemoteLogger *m_logger;
signals:
    // Nové signály pro komunikaci s UI
    void stateChanged(bool isActiveMode); // true = Aktivní, false = Neaktivní/Idle
    void packetSent();
    void countersUpdated(int sent, int received);
    void logMessage(const QString &message); // Signál pro předání textu do QPlainTextEdit
};

#endif // NETWORKAUTOMATON_H
