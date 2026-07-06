#include "mainwindow.h"
#include "ui_mainwindow.h"

// Standard and Qt includes
#include <QtConcurrent>
#include <QFuture>
#include <QFutureWatcher>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTimer>
#include <QDir>

void MainWindow::updatePaStatus()
{
    const auto status = pa->getStatus();

    // Struktura pro párování statusu se skupinou kontrolek
    struct LedGroup {
        std::vector<QLedIndicator*> widgets;
        uint32_t bit;
    };

    const std::vector<LedGroup> groups = {
        {{ui->ledPARemote,      ui->ledPARemote_4},      PaConfig::STATUS_REMOTE},
        {{ui->ledPAStanby,      ui->ledPAStanby_4},      PaConfig::STATUS_STANDBY},
        {{ui->ledPAHighVoltage, ui->ledPAHighVoltage_4}, PaConfig::STATUS_HIGH_VOLTAGE_ON},
        {{ui->ledPAAmpOn,       ui->ledPAAmpOn_4},       PaConfig::STATUS_AMPLIFIER_ON}
    };

    // 1. Aktualizace standardních stavů
    for (const auto& group : groups) {
        const bool on = (status & group.bit) == group.bit;
        const QColor color = on ? Qt::green : Qt::red;

        for (QLedIndicator* led : group.widgets) {
            led->setOn(true);
            led->setColor(color);
        }
    }

    // 2. Aktualizace alarmu (pro oba groupboxy)
    const bool alarm = (status & PaConfig::STATUS_SUMMARY_ALARM) == PaConfig::STATUS_SUMMARY_ALARM;
    const std::array alarmLeds = { ui->ledPAAlarm, ui->ledPAAlarm_4 };

    for (QLedIndicator* led : alarmLeds) {
        led->setOn(true); // Alarm bývá "aktivní" (viditelný) pořád, mění se barva/blikání
        led->setColor(alarm ? Qt::yellow : Qt::red);
        led->setBlink(alarm, 300);
    }
}

// --- PA Controls ---
void MainWindow::onPARemoteClicked() {
    pa->setStatus(PaConfig::OUTPUT_SET & (~PaConfig::COMMAND_REMOTE));
    QThread::msleep(50);
    pa->setStatus(PaConfig::OUTPUT_SET);
}

void MainWindow::onPAAmpOnClicked() {
    pa->setStatus(PaConfig::OUTPUT_SET & (~PaConfig::COMMAND_AMPLIFIER_ON_OFF));
    QThread::msleep(50);
    pa->setStatus(PaConfig::OUTPUT_SET);
}

void MainWindow::onPAHVOnClicked() {
    pa->setStatus(PaConfig::OUTPUT_SET & (~PaConfig::COMMAND_HIGH_VOLTAGE_ON_OFF));
    QThread::msleep(50);
    pa->setStatus(PaConfig::OUTPUT_SET);
}

void MainWindow::onPARun()
{
    // Resetujeme čítač pokusů při spuštění sekvence
    retryCount = 0;

    // 2. Připojíme časovač na zpracování stavového automatu
    connect(&paTimer, &QTimer::timeout, this, &MainWindow::processPaRun, Qt::UniqueConnection);

    // 3. Nastavíme počáteční stav a spustíme automat
    paState = PaRunState::SetRemote;
    paTimer.start(200); // Rychlejší reakce (100 ms)
}

void MainWindow::processPaRun()
{
    const auto status = pa->getStatus();

    auto isSet = [&](quint8 flag) {
        return (status & flag) == flag;
    };

    switch (paState)
    {
    case PaRunState::SetRemote:
        if (!isSet(PaConfig::STATUS_REMOTE)) {
            onPARemoteClicked();
        }
        stepTimer.restart();
        paState = PaRunState::WaitRemote;
        break;

    case PaRunState::WaitRemote:
        if (isSet(PaConfig::STATUS_REMOTE)) {
            retryCount = 0; // Úspěch, vynulujeme čítač pokusů
            paState = PaRunState::SetAmpOn;
        } else if (stepTimer.elapsed() > 2000) {
            if (retryCount < 3) {
                retryCount++;
                qDebug() << "WaitRemote timeout, opakuji pokus č.:" << retryCount;
                paState = PaRunState::SetRemote; // Zkusíme znovu poslat příkaz
            } else {
                paState = PaRunState::Error;
            }
        }
        break;

    case PaRunState::SetAmpOn:
        if (!isSet(PaConfig::STATUS_AMPLIFIER_ON)) {
            onPAAmpOnClicked();
        }
        stepTimer.restart();
        paState = PaRunState::WaitAmpOn;
        break;

    case PaRunState::WaitAmpOn:
        if (isSet(PaConfig::STATUS_AMPLIFIER_ON)) {
            retryCount = 0; // Úspěch, vynulujeme čítač pokusů
            paState = PaRunState::WaitStandby;
            stepTimer.restart();
        } else if (stepTimer.elapsed() > 2000) {
            if (retryCount < 3) {
                retryCount++;
                qDebug() << "WaitAmpOn timeout, opakuji pokus č.:" << retryCount;
                paState = PaRunState::SetAmpOn; // Zkusíme znovu poslat příkaz
            } else {
                paState = PaRunState::Error;
            }
        }
        break;

    case PaRunState::WaitStandby:
        // Poznámka: Zde neopakujeme "kliknutí", protože čekáme na interní stav HW (např. nahřátí),
        // ale necháváme velký timeout 60s, který jste zde měl nastavený.
        if (isSet(PaConfig::STATUS_STANDBY)) {
            retryCount = 0;
            paState = PaRunState::SetHV;
        } else if (stepTimer.elapsed() > 60000) {
            paState = PaRunState::Error;
        }
        break;

    case PaRunState::SetHV:
        if (!isSet(PaConfig::STATUS_HIGH_VOLTAGE_ON)) {
            onPAHVOnClicked();
        }
        stepTimer.restart();
        paState = PaRunState::WaitHV;
        break;

    case PaRunState::WaitHV:
        if (isSet(PaConfig::STATUS_HIGH_VOLTAGE_ON)) {
            retryCount = 0; // Úspěch
            paState = PaRunState::Done;
        } else if (stepTimer.elapsed() > 2000) {
            if (retryCount < 3) {
                retryCount++;
                qDebug() << "WaitHV timeout, opakuji pokus č.:" << retryCount;
                paState = PaRunState::SetHV; // Zkusíme znovu poslat příkaz
            } else {
                paState = PaRunState::Error;
            }
        }
        break;

        // --- Vypínací sekvence ---

    case PaRunState::ResetHV:
        if (isSet(PaConfig::STATUS_HIGH_VOLTAGE_ON)) {
            onPAHVOnClicked();
        }
        stepTimer.restart();
        paState = PaRunState::WaitHVOff;
        break;

    case PaRunState::WaitHVOff:
        if (!isSet(PaConfig::STATUS_HIGH_VOLTAGE_ON)) {
            retryCount = 0; // Úspěch
            paState = PaRunState::ResetAmp;
        } else if (stepTimer.elapsed() > 2000) {
            if (retryCount < 3) {
                retryCount++;
                qDebug() << "WaitHVOff timeout, opakuji pokus č.:" << retryCount;
                paState = PaRunState::ResetHV; // Zkusíme znovu poslat vypínací příkaz
            } else {
                paState = PaRunState::Error;
            }
        }
        break;

    case PaRunState::ResetAmp:
        if (isSet(PaConfig::STATUS_AMPLIFIER_ON)) {
            onPAAmpOnClicked();
        }
        stepTimer.restart();
        paState = PaRunState::WaitAmpOff;
        break;

    case PaRunState::WaitAmpOff:
        if (!isSet(PaConfig::STATUS_AMPLIFIER_ON)) {
            retryCount = 0; // Úspěch
            paState = PaRunState::Done;
        } else if (stepTimer.elapsed() > 2000) {
            if (retryCount < 3) {
                retryCount++;
                qDebug() << "WaitAmpOff timeout, opakuji pokus č.:" << retryCount;
                paState = PaRunState::ResetAmp; // Zkusíme znovu poslat vypínací příkaz
            } else {
                paState = PaRunState::Error;
            }
        }
        break;

    case PaRunState::Done:
        paTimer.stop();
        disconnect(&paTimer, nullptr, this, nullptr);
        emit paSequenceFinished(true);
        break;

    case PaRunState::Error:
        paTimer.stop();
        disconnect(&paTimer, nullptr, this, nullptr);
        qDebug() << "PA sequence ERROR at state:" << (int)paState << " after retries.";
        emit paSequenceFinished(false);
        break;

    default:
        break;
    }

    updatePaStatus();
}

void MainWindow::onPAStop()
{
    // Resetujeme čítač pokusů i pro vypínací sekvenci
    retryCount = 0;

    paState = PaRunState::ResetHV;
    stepTimer.restart();

    connect(&paTimer, &QTimer::timeout, this, &MainWindow::processPaRun, Qt::UniqueConnection);
    paTimer.start(200);
}

void MainWindow::emergencyStop()
{
    stopMeasurement();
    // zastav sekvencer
    paTimer.stop();
    disconnect(&paTimer, nullptr, this, nullptr);

    paState = PaRunState::Error; // nebo Idle, podle filozofie

    // okamžité vypnutí – bez čekání
    const auto status = pa->getStatus();

    auto isSet = [&](quint8 flag) {
        return (status & flag) == flag;
    };

    // pořadí je důležité
    if (isSet(PaConfig::STATUS_HIGH_VOLTAGE_ON)) {
        onPAHVOnClicked();   // nebo onPAHVOffClicked()
    }

    if (isSet(PaConfig::STATUS_AMPLIFIER_ON)) {
        onPAAmpOnClicked();  // nebo onPAAmpOffClicked()
    }

    // REMOTE neřešíme

    updatePaStatus();

    qDebug() << "!!! EMERGENCY STOP ACTIVATED !!!";
}
