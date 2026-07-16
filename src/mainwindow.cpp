#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "StatusBarManager.h"
#include "networkautomation.h"

// Standard and Qt includes
#include <QtConcurrent>
#include <QFuture>
#include <QFutureWatcher>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTimer>
#include <QDir>
#include <QSettings>

// Project specific includes
#include "MediaButtonsWidget.h"
#include "qledindicator.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    setWindowTitle("EMC control application by Zdenek Zechovsky");
    setWindowIcon(QIcon(":/img/electromagnetic.png"));
    ui->setupUi(this);
    ui->tabWidget->setCurrentIndex(0);

    // Dynamic allocation of hardware interfaces and managers
    ftdi = new FtdiBitBang();
    pa = new PowerAmplifier(ftdi);
    gpibDevice = new GpibDevice(this);
    measurementManager = new EmcMeasurementManager(gpibDevice, this);
    m_automaton = new NetworkAutomaton(gpibDevice, &EMC, this);

    setupWidgets();
    setupConnections();
    setupPlots();
    setupGroups();

    statusTimer = new QTimer(this);
    setWindowIcon(QIcon(":/img/electromagnetic.png"));
    setupVariables();
}

MainWindow::~MainWindow()
{
    storeVariables();

    if (statusTimer->isActive()) statusTimer->stop();

    // Free allocated memory
    delete ui;
    delete pa;
    delete ftdi;
    // Note: gpibDevice and measurementManager are children of MainWindow (passed 'this' in constructor),
    // so Qt's parent-child system will delete them automatically, but manual deletion is also fine.
}

void MainWindow::testpushButton_clicked()
{
    if (!m_automaton->isActive()) {
        // Načtení hodnot z UI do konfigurační struktury
        NetworkAutomaton::Config config;
        config.activePeriodBase = ui->spinBox_1->value();
        config.activePeriodRand = ui->spinBox_2->value();
        config.udpDelay         = ui->spinBox_3->value();
        config.udpPeriod        = ui->spinBox_4->value();
        config.inactiveBase     = ui->spinBox_5->value();
        config.inactiveRand     = ui->spinBox_6->value();

        // Spuštění a změna textu
        m_automaton->start(config);
        ui->testpushButton->setText("Stop");
    } else {
        // Zastavení a reset textu
        m_automaton->stop();
        ui->testpushButton->setText("Start");
    }
}

void MainWindow::setupGroups()
{
    // Default UI state
    ui->RE102_1->setEnabled(false);    
    ui->attgroup->setEnabled(false);
    ui->gengroup->setEnabled(false);
    ui->pwrgroup->setEnabled(false);
    ui->CS114group->setEnabled(false);
    ui->PAgroup->setEnabled(false);
    ui->PAgroup_2->setEnabled(false);    

    ui->S21group->setEnabled(false);
    //ui->comboBox->clear(); // Vymaže případné testovací položky z Designeru
    ui->comboBox->addItem("Sine", 0);   // Text, Data (QVariant)
    ui->comboBox->addItem("Square", 1);
    ui->comboBox->addItem("Ramp", 2);
    ui->comboBox->addItem("Pulse", 3);
    ui->comboBox->addItem("Noise", 4);

    ui->comboBox_1->clear(); // Vymaže případné testovací položky z Designeru
    ui->comboBox_1->addItem("HFH2_Z6 Active rod antenna ", 0);   // Text, Data (QVariant)
    ui->comboBox_1->addItem("HK116   Biconical antenna", 1);
    ui->comboBox_1->addItem("HL222   Log-periodic antenna", 2);
    ui->comboBox_1->addItem("HF906   Horn antenna", 3);
    ui->comboBox_1->addItem("CE102   50uH power network", 4);

    ui->comboBox_2->clear(); // Vymaže případné testovací položky z Designeru
    ui->comboBox_2->addItem("1%", 0);   // Text, Data (QVariant)
    ui->comboBox_2->addItem("5%", 1);
}

void MainWindow::setupWidgets()
{
    // Add MediaButtonsWidget to the designated UI layouts
    MediaButtonsWidget *mediaButtons_1 = new MediaButtonsWidget(this);
    ui->verticalLayout_1->addWidget(mediaButtons_1);    

    // Connect play buttons to corresponding spectrum measurement routines
    connect(mediaButtons_1, &MediaButtonsWidget::playClicked, this, &MainWindow::onSpectrum1Clicked);
    connect(mediaButtons_1, &MediaButtonsWidget::pauseClicked, this, &MainWindow::pauseMeasurement);    
    connect(mediaButtons_1, &MediaButtonsWidget::stopClicked, this, &MainWindow::stopMeasurement);
}

void MainWindow::setupConnections()
{
    //Apllication action - quit
    connect(ui->action_Quit, &QAction::triggered, qApp, &QApplication::quit);
    connect(ui->action_LoadCEGraph, &QAction::triggered, this, &MainWindow::LoadCEGraph);
    connect(ui->action_LoadCS114graph, &QAction::triggered, this, &MainWindow::LoadCSGraph);

    // Connection handling
    connect(ui->pushButton, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(ui->pushButton_2, &QPushButton::clicked, this, &MainWindow::onDisConnectClicked);


    // Instrument controls
    connect(ui->rfstepattenuator, &QSlider::valueChanged, this, &MainWindow::onAttenuatorValueChanged);
    connect(ui->rfstepattenuator, &QSlider::sliderReleased, this, &MainWindow::onAttenuatorReleased);
    connect(ui->setpwrButton, &QPushButton::clicked, this, &MainWindow::setpwrClicked);
    connect(ui->onpwrButton, &QPushButton::clicked, this, &MainWindow::onpwrClicked);
    connect(ui->getpwrButton, &QPushButton::clicked, this, &MainWindow::getpwrClicked);
    connect(ui->genpushButton, &QPushButton::clicked, this, &MainWindow::setgenClicked);

    // PA Controls
    connect(ui->PARemotepushButton, &QPushButton::clicked, this, &MainWindow::onPARemoteClicked);
    connect(ui->PAAmpOnpushButton, &QPushButton::clicked, this, &MainWindow::onPAAmpOnClicked);
    connect(ui->PAHVOnpushButton, &QPushButton::clicked, this, &MainWindow::onPAHVOnClicked);
    connect(ui->PASeqOnpushButton, &QPushButton::clicked, this, &MainWindow::onPARun);
    connect(ui->PASeqOffpushButton, &QPushButton::clicked, this, &MainWindow::onPAStop);

    //networkautomation
    connect(ui->testpushButton, &QPushButton::clicked, this, &MainWindow::testpushButton_clicked);


    // Vertical / Horizontal buttons
    connect(ui->verticalButton_1, &QPushButton::clicked, this, &MainWindow::vertical1Clicked);        
    connect(ui->comboBox_1, &QComboBox::currentIndexChanged, this, &MainWindow::TransducerChanged);
    connect(ui->cleanButton_1, &QPushButton::clicked, this, &MainWindow::cleanSpectrum);

    // CS114 buttons
    connect(ui->calibrateCS, &QPushButton::clicked, this, &MainWindow::prepareAndStartCalibrationCS114);
    connect(ui->measureCS, &QPushButton::clicked, this, &MainWindow::prepareAndStartMeasurementCS114);
    connect(ui->stopCS, &QPushButton::clicked, this, &MainWindow::emergencyStop);
    connect(ui->pauseCS, &QPushButton::clicked, this, &MainWindow::pauseMeasurement);
    connect(ui->stopCS_2, &QPushButton::clicked, this, &MainWindow::emergencyStop);
    connect(ui->pauseCS_2, &QPushButton::clicked, this, &MainWindow::pauseMeasurement);
    connect(ui->sweepS21, &QPushButton::clicked, this, &MainWindow::sweepS21Measurement);

    // Explicitní propojení checkboxů v Qt 6
    connect(ui->disp1checkBox, &QCheckBox::checkStateChanged, this, &MainWindow::disp1checkBox_stateChanged);
    connect(ui->disp2checkBox, &QCheckBox::checkStateChanged, this, &MainWindow::disp2checkBox_stateChanged);
    connect(ui->disp3checkBox, &QCheckBox::checkStateChanged, this, &MainWindow::disp3checkBox_stateChanged);

    // Measurement progress and data updates from Manager
    // Nadeklarování palety barev, které se budou střídat
    m_graphColors << Qt::blue << Qt::red << Qt::green << Qt::darkMagenta << Qt::darkCyan << Qt::black;

    // Nezapomeňte propojit nový signál! (předpokládám, že už podobně propojujete newDataPoint)
    connect(measurementManager, &EmcMeasurementManager::measurementStarted, this, &MainWindow::prepareNewGraph);
    connect(measurementManager, &EmcMeasurementManager::spectrummeasurementStarted, this, &MainWindow::prepareNewGraphSpectrum);
    connect(measurementManager, &EmcMeasurementManager::newMultiDataPoint, this, &MainWindow::updateMultiPlot);

    connect(measurementManager, &EmcMeasurementManager::measurementProgress, ui->progressBar, &QProgressBar::setValue);    
    connect(measurementManager, &EmcMeasurementManager::newDataPoint, this, &MainWindow::updatePlot);
    connect(measurementManager, &EmcMeasurementManager::newDataPointMeasure, this, &MainWindow::updatePlotMeasure);
    connect(measurementManager, &EmcMeasurementManager::spectrumChunkReady, this, &MainWindow::addSpectrumChunk);    

    connect(&paTimer, &QTimer::timeout, this, &MainWindow::processPaRun);

    connect(&StatusBarManager::instance(), &StatusBarManager::messageRequested,
            ui->statusbar, &QStatusBar::showMessage);

    // Seznam vašich grafů (můžete jich přidat kolik chcete)
    QList<QCustomPlot*> plots = { ui->qcustomplotWidget, ui->qcustomplotWidget_2 };

    for (QCustomPlot* plot : plots) {
        connect(plot, &QCustomPlot::mouseDoubleClick, this, [plot](QMouseEvent *event) {
            Q_UNUSED(event);
            // "plot" zde ukazuje přesně na ten graf, na který se zrovna kliklo
            plot->rescaleAxes();
            plot->replot();
        });
    }

    ui->plainTextEdit->setStyleSheet(
        "background-color: #121212; "  // Tmavě šedé/černé pozadí
        "color: #00FF00; "             // Jasně zelené písmo (Matrix styl)
        "font-family: 'Courier New'; " // Volitelně: neproporcionální písmo pro logy
        "font-size: 11pt;"
        );

    // 1. ŘÍZENÍ RADIO BUTTONŮ PODLE REŽIMU
    // Nastavení výchozího textu tlačítka
    ui->testpushButton->setText("Start");

    connect(m_automaton, &NetworkAutomaton::stateChanged, this, [this](bool isActiveMode){
        if (m_automaton->isActive()) { // Pokud automat vůbec běží
            ui->radioButton->setEnabled(isActiveMode);
            ui->radioButton_2->setEnabled(!isActiveMode);

            // Volitelně: automaticky je i zaškrtnout (setChecked), pokud chcete
            ui->radioButton->setChecked(isActiveMode);
            ui->radioButton_2->setChecked(!isActiveMode);
            // POŽADAVEK: Pokud aktivní režim skončil, zhasneme LED
            if (!isActiveMode) {
                m_ledState = false;
                ui->testLed->setOn(false);
            }
        } else {
            // Pokud je automat vypnutý tlačítkem Stop, raději obě zhasneme/zakážeme
            ui->radioButton->setEnabled(false);
            ui->radioButton_2->setEnabled(false);
            ui->radioButton->setChecked(false);
            ui->radioButton_2->setChecked(false);

            // Zhasnutí LED při stopu
            m_ledState = false;
            ui->testLed->setOn(false);

            // Volitelně: Můžete do logu na obrazovce přidat oddělovač, že byl automat zastaven
            //ui->plainTextEdit->appendPlainText("--- Automat zastaven uživatelem ---");
        }
    });

    // 2. TOGGLOVÁNÍ LED PŘI ODEOSLÁNÍ PAKETU
    connect(m_automaton, &NetworkAutomaton::packetSent, this, [this](){
        m_ledState = !m_ledState; // Negace stavu
        ui->testLed->setOn(m_ledState);
    });

    // Výchozí stav UI při zapnutí aplikace
    ui->radioButton->setEnabled(false);
    ui->radioButton_2->setEnabled(false);
    ui->testLed->setOn(false);

    connect(m_automaton, &NetworkAutomaton::countersUpdated, this, [this](int sent, int received){
        // Formátování textu na "odeslané/přijaté" (např. 100/100)
        ui->counterLabel->setText(QString("%1/%2").arg(sent).arg(received));
    });

    // Nastavení limitu řádků pro vysoký výkon
    ui->plainTextEdit->setMaximumBlockCount(5000);
    ui->plainTextEdit->setReadOnly(true); // Uživatel by neměl logy přepisovat

    // Propojení signálu logování s prvkem QPlainTextEdit
    connect(m_automaton, &NetworkAutomaton::logMessage, ui->plainTextEdit, &QPlainTextEdit::appendPlainText);
}

void MainWindow::setupPlots()
{
    // Initial plot configuration
    setupPlot(ui->qcustomplotWidget, 2e6, 1e9, 0);

    QVector<double> testFreqs = getFilteredCS114Frequencies(10e3, 200e6);
    setupPlotCS(ui->qcustomplotWidget_2, 10e3, 200e6, testFreqs);
}

// --- Hardware Actions ---

void MainWindow::onConnectClicked()
{
    QLedIndicator* leds[] = { ui->ledAtt, ui->ledGen, ui->ledSmt, ui->ledEsi, ui->ledPwr, ui->ledPA };
    const int numLEDs = sizeof(leds) / sizeof(leds[0]);

    for (int i = 0; i < numLEDs; ++i) {
        leds[i]->setOn(true);
        leds[i]->setColor(Qt::red);
        leds[i]->setBlink(true, 500);
    }

    // Try initializing FTDI for Power Amplifier
    if (pa->initDevice() == 0) {
        connect(statusTimer, &QTimer::timeout, this, &MainWindow::updatePaStatus);
        statusTimer->start(500);
        ui->PAgroup->setEnabled(true);
        ui->ledPA->setColor(Qt::green);  ui->ledPA->setBlink(false, 0);
    }

    // Initialize GPIB Instruments
    gpibDevice->init(&EMC);

    // UI state update based on found devices
    updateInterfaceState();
}

void MainWindow::updateInterfaceState()
{
    QLedIndicator* leds[] = { ui->ledAtt, ui->ledGen, ui->ledSmt, ui->ledEsi, ui->ledPwr };

    if (EMC.ATT_addr) {
        leds[0]->setColor(Qt::green); leds[0]->setBlink(false, 0);
        ui->attgroup->setEnabled(true);
    }
    if (EMC.GEN_addr) {
        leds[1]->setColor(Qt::green); leds[1]->setBlink(false, 0);
        ui->gengroup->setEnabled(true);
    }
    if (EMC.SMT_addr) {
        leds[2]->setColor(Qt::green); leds[2]->setBlink(false, 0);
    }
    if (EMC.ESI_addr) {
        leds[3]->setColor(Qt::green); leds[3]->setBlink(false, 0);
        ui->RE102_1->setEnabled(true);

        if (EMC.SMT_addr) {
            ui->CS114group->setEnabled(true);
            ui->S21group->setEnabled(true);
        }
    }
    if (EMC.PWR_addr) {
        leds[4]->setColor(Qt::green); leds[4]->setBlink(false, 0);
        ui->pwrgroup->setEnabled(true);
    }
}

void MainWindow::onDisConnectClicked()
{
    QLedIndicator* leds[] = { ui->ledAtt, ui->ledGen, ui->ledSmt, ui->ledEsi, ui->ledPwr };
    const int numLEDs = sizeof(leds) / sizeof(leds[0]);

    statusTimer->stop();
    ftdi->closeDevice();
    gpibDevice->close(&EMC);

    for (int i = 0; i < numLEDs; ++i) {
        leds[i]->setOn(true);
        leds[i]->setColor(Qt::gray);
        leds[i]->setBlink(false, 500);
    }    
    setupGroups();
}

void MainWindow::stopMeasurement() {
    if (stopFlag) {
        *stopFlag = true; // Změníme hodnotu, vlákno si ji při dalším cyklu přečte a ukončí se
    }
}

void MainWindow::pauseMeasurement() {
    if (pauseFlag) {
        // Přepínání stavu pauzy (toggle)
        bool isPaused = pauseFlag->load();
        *pauseFlag = !isPaused;
    }
}

void MainWindow::setupVariables()
{
    // Absolutní cesta k INI souboru
    QString iniPath = QCoreApplication::applicationDirPath() + "/config.ini";

    // Pomocná kontrola, jestli soubor na disku vůbec existuje
    if (!QFileInfo::exists(iniPath)) {
        StatusBarManager::instance().showMessage(QString("INI file failed  - %1").arg(iniPath));
        return;
    }

    QSettings settings(iniPath, QSettings::IniFormat);

    // Otevřeme sekci [Metadata]
    settings.beginGroup("Metadata");

    // Načtení hodnot. Druhý parametr ("") je výchozí hodnota,
    // která se použije, pokud v INI souboru klíč ještě neexistuje.
    QString unit = settings.value("Unit", "Unknown").toString();
    QString op   = settings.value("Operator", "Unknown").toString();

    settings.endGroup();

    // Nastavení textů do UI prvků
    ui->unitEdit->setText(unit);
    ui->operatorEdit->setText(op);

    settings.beginGroup("Generator");

    // Načtení hodnot s výchozími (default) hodnotami, pokud v INI ještě nejsou
    double freq   = settings.value("Frequency", 10.0).toDouble();
    double amp    = settings.value("Amplitude", 1.0).toDouble();
    double offset = settings.value("Offset", 0.0).toDouble();
    int comboid   = settings.value("SelectedType", 0).toInt();

    settings.endGroup();

    // Nastevení hodnot do SpinBoxů
    ui->genFreqSpinBox->setValue(freq);
    ui->genAmpSpinBox->setValue(amp);
    ui->genOffsetSpinBox->setValue(offset);

    // Nastavení správné položky v ComboBoxu podle uložených dat (ID)
    int index = ui->comboBox->findData(comboid);
    if (index != -1) {
        ui->comboBox->setCurrentIndex(index);
    } else {
        // Pokud se ID v comboboxu nenašlo (např. chyba v INI), nastaví se první položka
        ui->comboBox->setCurrentIndex(1);
    }

    settings.beginGroup("PowerSupply");

    // Načtení napětí (výchozí hodnota je zde např. 0.0)
    double p6_v  = settings.value("P6_Voltage", 0.0).toDouble();
    double p25_v = settings.value("P25_Voltage", 0.0).toDouble();
    double m25_v = settings.value("M25_Voltage", 0.0).toDouble();

    // Načtení proudů (výchozí hodnota je zde např. 0.0)
    double p6_i  = settings.value("P6_Current", 0.0).toDouble();
    double p25_i = settings.value("P25_Current", 0.0).toDouble();
    double m25_i = settings.value("M25_Current", 0.0).toDouble();

    settings.endGroup();

    // Nastavení načtených hodnot zpět do SpinBoxů v UI
    ui->p6VSpinBox->setValue(p6_v);
    ui->p25VSpinBox->setValue(p25_v);
    ui->m25VSpinBox->setValue(m25_v);

    ui->p6ISpinBox->setValue(p6_i);
    ui->p25ISpinBox->setValue(p25_i);
    ui->m25ISpinBox->setValue(m25_i);

    // Začátek čtení pole ze skupiny "ScanRanges"
    int size = settings.beginReadArray("ScanRanges");

    if (size == 0) {
        StatusBarManager::instance().showMessage(QString("Scan ranges restored - %1").arg(iniPath));
        settings.endArray();
    }
    else
    {
        // Projdeme pole a načteme hodnoty (max 4)
        for (int i = 0; i < size && i < 4; ++i) {
            settings.setArrayIndex(i);

            ranges[i].Start_MHz     = settings.value("Start_MHz").toDouble();
            ranges[i].Stop_MHz      = settings.value("Stop_MHz").toDouble();
            ranges[i].Step_size_kHz = settings.value("Step_size_kHz").toDouble();
            ranges[i].Res_BW_kHz    = settings.value("Res_BW_kHz").toDouble();

            ranges[i].Meas_Time_ms  = settings.value("Meas_Time_ms").toInt();
            ranges[i].Auto_Ranging  = settings.value("Auto_Ranging").toInt();
            ranges[i].RF_Attn_dB    = settings.value("RF_Attn_dB").toInt();
            ranges[i].Preamp        = settings.value("Preamp").toInt();
            ranges[i].Auto_Preamp   = settings.value("Auto_Preamp").toInt();
            ranges[i].Input         = settings.value("Input").toInt();

            // Výpis úspěšně načteného indexu (Qt v INI indexuje od 1, my v poli od 0)
            /*
            qDebug() << QString("--> Rozsah %1/%2 uspesne nacten: %3 MHz az %4 MHz (Step: %5 kHz, BW: %6 kHz, Input: %7)")
                            .arg(i + 1)
                            .arg(qMin(size, 4))
                            .arg(ranges[i].Start_MHz)
                            .arg(ranges[i].Stop_MHz)
                            .arg(ranges[i].Step_size_kHz)
                            .arg(ranges[i].Res_BW_kHz)
                            .arg(ranges[i].Input);
            */
        }

        // Ukončení čtení pole
        settings.endArray();
        StatusBarManager::instance().showMessage(QString("INI file %1").arg(iniPath), 3000);
    }
}

void MainWindow::storeVariables()
{
    QString iniPath = QCoreApplication::applicationDirPath() + "/config.ini";
    QSettings settings(iniPath, QSettings::IniFormat);

    // Otevřeme sekci [Metadata]
    settings.beginGroup("Metadata");

    // Zápis hodnot z lineEditů
    settings.setValue("Unit", ui->unitEdit->text());
    settings.setValue("Operator", ui->operatorEdit->text());

    // Zavření sekce
    settings.endGroup();

    settings.beginGroup("Generator");

    // Uložení číselných hodnot ze spinboxů (value() vrací double nebo int podle typu spinboxu)
    settings.setValue("Frequency", ui->genFreqSpinBox->value());
    settings.setValue("Amplitude", ui->genAmpSpinBox->value());
    settings.setValue("Offset", ui->genOffsetSpinBox->value());

    // Uložení uživatelských dat z ComboBoxu (.toInt() už máš v dotazu)
    settings.setValue("SelectedType", ui->comboBox->currentData().toInt());

    settings.endGroup();

    settings.beginGroup("PowerSupply");

    // Uložení napětí (V)
    settings.setValue("P6_Voltage",  ui->p6VSpinBox->value());
    settings.setValue("P25_Voltage", ui->p25VSpinBox->value());
    settings.setValue("M25_Voltage", ui->m25VSpinBox->value());

    // Uložení proudů (I)
    settings.setValue("P6_Current",  ui->p6ISpinBox->value());
    settings.setValue("P25_Current", ui->p25ISpinBox->value());
    settings.setValue("M25_Current", ui->m25ISpinBox->value());

    settings.endGroup();

    settings.remove("ScanRanges");

    settings.beginWriteArray("ScanRanges");

    for (int i = 0; i < 4; ++i) {
        settings.setArrayIndex(i);

        settings.setValue("Start_MHz",     ranges[i].Start_MHz);
        settings.setValue("Stop_MHz",      ranges[i].Stop_MHz);
        settings.setValue("Step_size_kHz", ranges[i].Step_size_kHz);
        settings.setValue("Res_BW_kHz",    ranges[i].Res_BW_kHz);

        settings.setValue("Meas_Time_ms",  ranges[i].Meas_Time_ms);
        settings.setValue("Auto_Ranging",  ranges[i].Auto_Ranging);
        settings.setValue("RF_Attn_dB",    ranges[i].RF_Attn_dB);
        settings.setValue("Preamp",        ranges[i].Preamp);
        settings.setValue("Auto_Preamp",   ranges[i].Auto_Preamp);
        settings.setValue("Input",         ranges[i].Input);
    }

    settings.endArray();
    settings.sync();

    // Volitelné: Vynucení zápisu na disk hned teď
    settings.sync();
}
