#include "networkautomation.h"
#include "StatusBarManager.h"
#include <QRandomGenerator>
#include <QDebug>
#include <QtConcurrent>
#include <QFileDialog>

#include "RemoteLogger.h"
#include "diag_ipu_159_01.h"

NetworkAutomaton::NetworkAutomaton(GpibDevice* device, tEMC_measurement *emc, QObject *parent)
    : QObject(parent), hwDevice(device), m_emc(emc),
    m_state(State::Idle),
    m_isRunning(false),
    m_targetAddress("129.200.99.81"),
    m_targetPort(1030),
    m_listenPort(1030),
    m_logger(nullptr)
{
    // Absolutní cesta k INI souboru
    QString iniPath = QCoreApplication::applicationDirPath() + "/secret.ini";

    // Pomocná kontrola, jestli soubor na disku vůbec existuje
    if (!QFileInfo::exists(iniPath)) {
        StatusBarManager::instance().showMessage(QString("SECRET file failed  - %1").arg(iniPath));
        //return;
    }

    QSettings settings(iniPath, QSettings::IniFormat);

    // Otevřeme sekci [Metadata]
    settings.beginGroup("GistData");

    // Načtení hodnot. Druhý parametr ("") je výchozí hodnota,
    // která se použije, pokud v INI souboru klíč ještě neexistuje.
    QString token = settings.value("Token", "Unknown").toString();
    QString id   = settings.value("GistId", "4020ea114bec9c3a929204238444be1c").toString();
    QString filename   = settings.value("FileName", "emc_log.txt").toString();

    settings.endGroup();

    m_logger = new RemoteLogger();

    //qDebug() << token << id << filename;

    m_logger->setToken(token);
    m_logger->setGistId(id);
    m_logger->setFileName(filename);


    // 4. Vytvoření vlákna a přesun loggeru do něj
    QThread *loggerThread = new QThread(this);
    m_logger->moveToThread(loggerThread);

    // KLÍČOVÝ KROK: Jakmile vlákno odstartuje, zavolá se init() přímo uvnitř loggerThread!
    connect(loggerThread, &QThread::started, m_logger, &RemoteLogger::init);

    // Zajištění správné likvidace objektu při ukončení vlákna
    connect(loggerThread, &QThread::finished, m_logger, &QObject::deleteLater);

    // Spuštění vlákna (od této chvíle běží event loop loggeru vedle)
    loggerThread->start();

    // BEZPEČNÉ VOLÁNÍ PŘES VLÁKNA:
    QMetaObject::invokeMethod(m_logger, "setInterval", Qt::QueuedConnection, Q_ARG(int, 30 * 60 * 1000));
    QMetaObject::invokeMethod(m_logger, "setMaxLines", Qt::QueuedConnection, Q_ARG(int, 2000));

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
    // Korektně ukončíme vlákno loggeru před smazáním automatu
    if (m_logger) {
        QThread *thread = m_logger->thread();
        thread->quit();
        thread->wait(); // Počkáme na bezpečné ukončení vlákna
    }
}

void NetworkAutomaton::start(const Config &config)
{
    if (m_isRunning) return;

    QMetaObject::invokeMethod(m_logger, "clear", Qt::QueuedConnection);
    QMetaObject::invokeMethod(m_logger, "start", Qt::QueuedConnection);

    QDateTime now = QDateTime::currentDateTime();
    m_logFileName = now.toString("hhmmss_ddMMyy") + ".log";

    emit countersUpdated(m_sentPacketsCount, m_receivedPacketsCount);

    m_config = config;
    m_isRunning = true;

    // Bindování portu pro příjem odpovědí (1001)
    // Původní: m_udpSocket->bind(QHostAddress::Any, m_listenPort);
    // Upravené s příznakem ShareAddress a ReuseAddressHint:
    if(!m_udpSocket->bind(QHostAddress::Any, m_listenPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
    //if (!m_udpSocket->bind(QHostAddress::Any, m_listenPort))
    {
        qWarning() << "UDP bind failed:" << m_udpSocket->errorString();
        //return;
    }

    // Nastartování aktivní periody
    m_state = State::Active;
    disconnect(m_stateTimer, &QTimer::timeout, nullptr, nullptr);
    connect(m_stateTimer, &QTimer::timeout, this, &NetworkAutomaton::onActivePeriodTimeout);

    double activeDuration = getRandomValue(m_config.activePeriodBase, m_config.activePeriodRand);
    m_stateTimer->start(static_cast<int>(activeDuration * 1000));

    // Spuštění časovače pro zpoždění UDP paketu
    m_udpDelayTimer->start(static_cast<int>(m_config.udpDelay * 1000));

    emit stateChanged(true);

    hwDevice->setPowerSupply(m_emc->PWR_addr,5.0,25.0,-3.0,1.0,1.0,1.0);

    hwDevice->enablePowerSupply(m_emc->PWR_addr, 1);
    hwDevice->setDisplay(m_emc->PWR_addr, 1);

    QTime t(0,0);
    t = t.addSecs(qRound(activeDuration));

    QString str = t.toString("hh:mm:ss");

    // Logování startu
    logToFile(QString("--- Automat spuštěn ---"));
    logToFile(QString("%1 -- aktivní perioda po dobu %2, vysílání diagnostiky za %3 s").arg(now.toString("hh:mm:ss"), str)
                  .arg(m_config.udpDelay));    
}

void NetworkAutomaton::stop()
{
    if (!m_isRunning) return;

    m_isRunning = false;
    m_state = State::Idle;

    // 1. Zastavíme časovače automatu
    m_stateTimer->stop();
    m_udpDelayTimer->stop();
    m_udpSendTimer->stop();

    // 2. Zapíšeme finální zprávu do logu a SPUSTÍME ODESÍLÁNÍ (dokud je vše nahoře)
    logToFile(QString("--- Automat zastaven uživatelem ---"));
    QMetaObject::invokeMethod(m_logger, "stop", Qt::QueuedConnection);

    // 3. Odpojení lokálního UDP socketu automatu
    m_udpSocket->close();

        m_receivedPacketsCount = m_sentPacketsCount = 0;
        emit countersUpdated(0, 0); // Vylepšení pro UI
    emit stateChanged(false);

        // 4. AŽ JAKO POSLEDNÍ odstavíme napájení hardwaru
        hwDevice->enablePowerSupply(m_emc->PWR_addr, 0);
        hwDevice->setDisplay(m_emc->PWR_addr, 1);
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
    logToFile(QString("%1 -- neaktivní perioda po dobu %2, vysláno/přijato %3/%4 UDP paketů")
                  .arg(now.toString("hh:mm:ss"), str)
                  .arg(m_sentPacketsCount)
                  .arg(m_receivedPacketsCount));

    hwDevice->enablePowerSupply(m_emc->PWR_addr, 0);
    hwDevice->setDisplay(m_emc->PWR_addr, 1);
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
    logToFile(QString("%1 -- aktivní perioda po dobu %2").arg(now.toString("hh:mm:ss"), str));
    hwDevice->enablePowerSupply(m_emc->PWR_addr, 1);
    hwDevice->setDisplay(m_emc->PWR_addr, 1);
}

void NetworkAutomaton::onUdpDelayTimeout()
{
    if (m_state != State::Active) return;

    m_receivedPacketsCount = m_sentPacketsCount = 0;
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
    sys_diag_request_t request{};
    request.packetType = SYS_DIAG_PACKET_TYPE_DIAG;
    //request.responseDataFormat = SYS_DIAG_RESPONSE_DATA_FORMAT_STD;
    request.responseDataFormat = SYS_DIAG_RESPONSE_DATA_FORMAT_EXT;

    m_udpSocket->writeDatagram(
        reinterpret_cast<const char*>(&request),
        sizeof(request),
        m_targetAddress,
        m_targetPort);

    emit packetSent();

    // Inkrementace a vyslání nového stavu počítadel
    m_sentPacketsCount++;
    emit countersUpdated(m_sentPacketsCount, m_receivedPacketsCount);
}

void NetworkAutomaton::onReadyRead()
{
    //SYS_DIAG_RESPONSE_STD_DATA_FORMAT response{};
    //static SYS_DIAG_RESPONSE_STD_DATA_FORMAT response_old{};

    SYS_DIAG_RESPONSE_EXT_DATA_FORMAT response{};
    static SYS_DIAG_RESPONSE_EXT_DATA_FORMAT response_old{};

    if(m_receivedPacketsCount == 0) {
        memset(&response_old,0xff,sizeof(response_old));
    }

    while (m_udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(static_cast<int>(m_udpSocket->pendingDatagramSize()));

        QHostAddress sender;
        quint16 senderPort;

        m_udpSocket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
        QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");

        if (datagram.size() == static_cast<int>(sizeof(response))) {

            memcpy(&response, datagram.constData(), sizeof(response));

            // Inkrementace a vyslání nového stavu počítadel
            m_receivedPacketsCount++;
            emit countersUpdated(m_sentPacketsCount, m_receivedPacketsCount);
            // Zde zpracovat data z odpovědi

            response.hwStatus = qFromBigEndian(response.hwStatus);
            response.swStatus = qFromBigEndian(response.swStatus);
            response.temperatureCPU = qFromBigEndian(response.temperatureCPU);
            response.Diag_ZZ.GS12170_input_lock = qFromBigEndian(response.Diag_ZZ.GS12170_input_lock);
            response.GS12170_CRC_error_CH0_counter = qFromBigEndian(response.GS12170_CRC_error_CH0_counter);
            response.Diag_ZZ.GS12170_raster_struc_1 = qFromBigEndian(response.Diag_ZZ.GS12170_raster_struc_1);
            response.Diag_ZZ.GS12170_raster_struc_2 = qFromBigEndian(response.Diag_ZZ.GS12170_raster_struc_2);
            response.Diag_ZZ.GS12170_raster_struc_3 = qFromBigEndian(response.Diag_ZZ.GS12170_raster_struc_3);
            response.Diag_ZZ.GS12170_raster_struc_4 = qFromBigEndian(response.Diag_ZZ.GS12170_raster_struc_4);
            response.Diag_ZZ.temperature = qFromBigEndian(response.Diag_ZZ.temperature);
            response.Diag_ZZ.firmwareID = qFromBigEndian(response.Diag_ZZ.firmwareID);

            QString msg;

            if(m_receivedPacketsCount == 1)
            {
                quint16 fw = response.Diag_ZZ.firmwareID;

                QString firmwareVersion =
                    QString ("%1.%2").arg((fw >> 4) & 0x0F, fw & 0x0F);

                msg = QString("%1 fw CPU = %2, fw DSP = %3")
                          .arg(timestamp,
                               QStringView{QString::fromUtf8(response.firmwareName)},
                               firmwareVersion);
                logToFile(msg);
            }

            if(response.hwStatus != response_old.hwStatus ||
                response.swStatus != response_old.swStatus ||
                response.Diag_ZZ.GS12170_input_lock != response_old.Diag_ZZ.GS12170_input_lock ||
                response.GS12170_CRC_error_CH0_counter!= response_old.GS12170_CRC_error_CH0_counter
                ) {

                double p6i, p25i, m25i;
                hwDevice->getPowerCurrent(m_emc->PWR_addr, &p6i, &p25i, &m25i);

                double p6u, p25u, m25u;
                hwDevice->getPowerVoltage(m_emc->PWR_addr, &p6u, &p25u, &m25u);

                hwDevice->setDisplay(m_emc->PWR_addr, 1);

                msg = QString("%1 U = %2V, I = %3A, HW = %4, SW = %5, t = %6/%7°C, SDI_lock = %8, SDI_CRC_error = %9, WxH = %10x%11")
                          .arg(timestamp)
                          .arg(p25u - m25u, 0, 'f', 2)
                          .arg(p25i, 0, 'f', 2)
                          .arg(QString("0x%1")
                                   .arg(response.hwStatus, 2, 16, QLatin1Char('0'))
                                   .toUpper(), QString("0x%1")
                                   .arg(response.swStatus, 2, 16, QLatin1Char('0'))
                                   .toUpper())
                          .arg(static_cast<double>(response.temperatureCPU) / 1000.0, 0, 'f', 2)
                          .arg(static_cast<double>(response.Diag_ZZ.temperature) / 256.0, 0, 'f', 2)
                          .arg(response.Diag_ZZ.GS12170_input_lock)
                          .arg(response.GS12170_CRC_error_CH0_counter)
                          .arg(response.Diag_ZZ.GS12170_raster_struc_4)
                          .arg(response.Diag_ZZ.GS12170_raster_struc_1);

                logToFile(msg);
            }

            memcpy(&response_old, &response, sizeof(response));
        } else {

            QString msg = QString("[%1] CHYBA: Přijat paket nesprávné délky (%2B) z %3:%4")
                              .arg(timestamp).arg(datagram.size()).arg(sender.toString()).arg(senderPort);

            logToFile(msg);
            memset(&response_old,0xff,sizeof(response_old));
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

    QMetaObject::invokeMethod(m_logger, "append", Qt::QueuedConnection, Q_ARG(QString, message));

    // Poslání zprávy do MainWindow pro zobrazení v QPlainTextEdit
    emit logMessage(message);
}