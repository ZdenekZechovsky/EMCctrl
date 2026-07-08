#include "networkautomation.h"
#include "StatusBarManager.h"
#include <QRandomGenerator>
#include <QDebug>
#include <QtConcurrent>
#include <QFileDialog>

#include "RemoteLogger.h"

NetworkAutomaton::NetworkAutomaton(GpibDevice* device, tEMC_measurement *emc, QObject *parent)
    : QObject(parent), hwDevice(device), m_emc(emc),
    m_state(State::Idle),
    m_isRunning(false),
    m_targetAddress("129.99.200.80"),
    m_targetPort(1000),
    m_listenPort(1001)
{
    // Absolutní cesta k INI souboru
    QString iniPath = QCoreApplication::applicationDirPath() + "/secret.ini";

    // Pomocná kontrola, jestli soubor na disku vůbec existuje
    if (!QFileInfo::exists(iniPath)) {
        StatusBarManager::instance().showMessage(QString("SECRET file failed  - %1").arg(iniPath));
        return;
    }

    QSettings settings(iniPath, QSettings::IniFormat);

    // Otevřeme sekci [Metadata]
    settings.beginGroup("GistData");

    // Načtení hodnot. Druhý parametr ("") je výchozí hodnota,
    // která se použije, pokud v INI souboru klíč ještě neexistuje.
    QString token = settings.value("Token", "Unknown").toString();
    QString id   = settings.value("GistId", "Unknown").toString();
    QString filename   = settings.value("FileName", "Unknown").toString();

    settings.endGroup();

    m_logger = new RemoteLogger(this);

    //qDebug() << token << id << filename;

    m_logger->setToken(token);
    m_logger->setGistId(id);
    m_logger->setFileName(filename);

    m_logger->setInterval(60000);
    m_logger->setMaxLines(1000);

    // Inicializace časovačů
    m_stateTimer = new QTimer(this);
    m_stateTimer->setSingleShot(true); // Vždy běží jen na jeden cyklus, pak přepočítáme random složku

    m_udpDelayTimer = new QTimer(this);
    m_udpDelayTimer->setSingleShot(true);

    m_udpSendTimer = new QTimer(this);
    // Inicializace socketu
    m_udpSocket = new QUdpSocket(this);

    // Propojení signálů a slotů
    connect(m_udpDelayTimer, &QTimer::timeout, this, &NetworkAutomaton::onUdpDelayTimeout);
    connect(m_udpSendTimer, &QTimer::timeout, this, &NetworkAutomaton::onUdpSendTimeout);
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &NetworkAutomaton::onReadyRead);
}

NetworkAutomaton::~NetworkAutomaton()
{
    stop();
}

void NetworkAutomaton::start(const Config &config)
{
    if (m_isRunning) return;

    m_logger->clear();
    m_logger->start();

    QDateTime now = QDateTime::currentDateTime();
    m_logFileName = now.toString("hhmmss_ddMMyy") + ".log";

    emit countersUpdated(m_sentPacketsCount, m_receivedPacketsCount);

    m_config = config;
    m_isRunning = true;

    // Bindování portu pro příjem odpovědí (1001)
    m_udpSocket->bind(QHostAddress::Any, m_listenPort);

    // Nastartování aktivní periody
    m_state = State::Active;
    disconnect(m_stateTimer, &QTimer::timeout, nullptr, nullptr);
    connect(m_stateTimer, &QTimer::timeout, this, &NetworkAutomaton::onActivePeriodTimeout);

    double activeDuration = getRandomValue(m_config.activePeriodBase, m_config.activePeriodRand);
    m_stateTimer->start(static_cast<int>(activeDuration * 1000));

    // Spuštění časovače pro zpoždění UDP paketu
    m_udpDelayTimer->start(static_cast<int>(m_config.udpDelay * 1000));

    emit stateChanged(true);


    hwDevice->enablePowerSupply(m_emc->PWR_addr, 1);

    QTime t(0,0);
    t = t.addSecs(qRound(activeDuration));

    QString str = t.toString("hh:mm:ss");

    // Logování startu
    logToFile(QString("--- Automat spuštěn ---"));
    logToFile(QString("%1 -- aktivní perioda %2, UDP zpoždění:%3 s").arg(now.toString("hh:mm:ss"))
                  .arg(str)
                  .arg(m_config.udpDelay));    
}

void NetworkAutomaton::stop()
{
    if (!m_isRunning) return;

    m_isRunning = false;
    m_state = State::Idle;

    // Zastavení všech časovačů
    m_stateTimer->stop();
    m_udpDelayTimer->stop();
    m_udpSendTimer->stop();

    // Odpojení socketu
    m_udpSocket->close();

    //qDebug() << "Automat zastaven a resetován do výchozího stavu.";
    m_receivedPacketsCount = m_sentPacketsCount = 0;
    emit stateChanged(false);

    hwDevice->enablePowerSupply(m_emc->PWR_addr, 0);

    logToFile(QString("--- Automat zastaven uživatelem ---"));
    m_logger->stop();
}

void NetworkAutomaton::onActivePeriodTimeout()
{
    // Aktivní perioda skončila -> Přechod do Neaktivní
    m_udpSendTimer->stop();
    m_udpDelayTimer->stop();

    m_state = State::Inactive;
    emit stateChanged(false);
    disconnect(m_stateTimer, &QTimer::timeout, nullptr, nullptr);
    connect(m_stateTimer, &QTimer::timeout, this, &NetworkAutomaton::onInactivePeriodTimeout);

    double inactiveDuration = getRandomValue(m_config.inactiveBase, m_config.inactiveRand);
    m_stateTimer->start(static_cast<int>(inactiveDuration * 1000));

    QTime t(0,0);
    t = t.addSecs(qRound(inactiveDuration));

    QString str = t.toString("hh:mm:ss");

    QDateTime now = QDateTime::currentDateTime();
    logToFile(QString("%1 -- neaktivní perioda %2").arg(now.toString("hh:mm:ss"))
                  .arg(str));

    hwDevice->enablePowerSupply(m_emc->PWR_addr, 0);
}

void NetworkAutomaton::onInactivePeriodTimeout()
{
    // Neaktivní perioda skončila -> Návrat do Aktivní
    m_state = State::Active;
    emit stateChanged(true);

    disconnect(m_stateTimer, &QTimer::timeout, nullptr, nullptr);
    connect(m_stateTimer, &QTimer::timeout, this, &NetworkAutomaton::onActivePeriodTimeout);

    double activeDuration = getRandomValue(m_config.activePeriodBase, m_config.activePeriodRand);
    m_stateTimer->start(static_cast<int>(activeDuration * 1000));

    // Znovu nastartujeme zpoždění UDP pro novou aktivní fázi
    m_udpDelayTimer->start(static_cast<int>(m_config.udpDelay * 1000));

    QTime t(0,0);
    t = t.addSecs(qRound(activeDuration));

    QString str = t.toString("hh:mm:ss");

    QDateTime now = QDateTime::currentDateTime();
    logToFile(QString("%1 -- aktivní perioda %2").arg(now.toString("hh:mm:ss"))
                  .arg(str));
    hwDevice->enablePowerSupply(m_emc->PWR_addr, 1);
}

void NetworkAutomaton::onUdpDelayTimeout()
{
    if (m_state != State::Active) return;

    // Zpoždění vypršelo, pošleme první paket a spustíme periodické odesílání
    sendUdpPacket();
    m_udpSendTimer->start(static_cast<int>(m_config.udpPeriod * 1000));
}

void NetworkAutomaton::onUdpSendTimeout()
{
    if (m_state == State::Active) {
        sendUdpPacket();
    }
}

void NetworkAutomaton::sendUdpPacket()
{
    QByteArray datagram(128, 0); // Vytvoří 128B paket naplněný nulami (lze upravit)

    // Zde případně naplňte datagram daty...

    m_udpSocket->writeDatagram(datagram, m_targetAddress, m_targetPort);    
    emit packetSent();

    // Inkrementace a vyslání nového stavu počítadel
    m_sentPacketsCount++;
    emit countersUpdated(m_sentPacketsCount, m_receivedPacketsCount);
}

void NetworkAutomaton::onReadyRead()
{
    while (m_udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(static_cast<int>(m_udpSocket->pendingDatagramSize()));

        QHostAddress sender;
        quint16 senderPort;

        m_udpSocket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");

        // Kontrola délky odpovědi (640B)
        if (datagram.size() == 640) {            
            // Inkrementace a vyslání nového stavu počítadel
            m_receivedPacketsCount++;
            emit countersUpdated(m_sentPacketsCount, m_receivedPacketsCount);
            // Zde zpracovat data z odpovědi

            QString msg = QString("[%1] PŘIJATO: 640B z %2:%3")
                              .arg(timestamp).arg(sender.toString()).arg(senderPort);
            logToFile(msg);
        } else {

            QString msg = QString("[%1] CHYBA: Přijat paket nesprávné délky (%2B) z %3:%4")
                              .arg(timestamp).arg(datagram.size()).arg(sender.toString()).arg(senderPort);

            logToFile(msg);
        }
    }
}

double NetworkAutomaton::getRandomValue(double base, double maxRand)
{
    if (maxRand <= 0.0) return base;
    // Generuje náhodné číslo od 0.0 do maxRand a přičte base
    double randVal = QRandomGenerator::global()->generateDouble() * maxRand;
    return base + randVal;
}

// Pomocná metoda pro zápis do souboru a vyslání signálu do UI
void NetworkAutomaton::logToFile(const QString &message)
{
    // Zápis do souboru (otevře, zapíše, zavře - bezpečné proti pádům)
    QFile file(m_logFileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << message << "\n";
        file.close();
    }

    m_logger->append(message);

    // Poslání zprávy do MainWindow pro zobrazení v QPlainTextEdit
    emit logMessage(message);
}