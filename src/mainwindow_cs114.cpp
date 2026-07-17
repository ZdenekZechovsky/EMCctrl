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

#include "GpibDevice.h"
#include "EmcMeasurementManager.h"

void MainWindow::prepareNewGraph() {
    m_currentGraph = ui->qcustomplotWidget_2->addGraph();

    // stepNumber je počet grafů (1 pro první, 2 pro druhý...)
    int stepNumber = ui->qcustomplotWidget_2->graphCount();
    m_currentGraph->setName(QString("Trace %1").arg(stepNumber));

    QColor color = m_graphColors[m_colorIndex % m_graphColors.size()];
    m_colorIndex++;

    QPen pen;
    pen.setColor(color);
    pen.setWidth(2);
    m_currentGraph->setPen(pen);

    // --- NOVÁ ČÁST PRO SPINBOX ---
    // Povolíme výběr nového čísla grafu
    ui->tracerSpinBox->setMaximum(stepNumber);

    // Automaticky přepneme SpinBox na tento nový graf.
    // (Tím se automaticky zavolá i náš slot on_tracerSpinBox_valueChanged
    // a tracer se korektně přepne).
    ui->tracerSpinBox->setValue(stepNumber);
}

// CS114 frequencies generator
QVector<double> generateCS114Frequencies() {
    QVector<double> frequencies;
    double currentFreq = 10e3;
    const double stopFreq = 10e9;
    const double band1Limit = 1e6;
    const double band2Limit = 200e6;

    while (currentFreq <= stopFreq) {
        frequencies.append(currentFreq);
        //double multiplier = (currentFreq < band1Limit) ? 1.05 : ((currentFreq < band2Limit) ? 1.01 : 1.001);
        double multiplier = 1.05;
        double nextFreq = currentFreq * multiplier;

        if (nextFreq > stopFreq && currentFreq < stopFreq) {
            frequencies.append(stopFreq);
            break;
        }
        currentFreq = nextFreq;
    }
    return frequencies;
}

#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QMessageBox>
#include <QVector>

void MainWindow::LoadCSGraph()
{
    // 1. Otevření QFileDialog pro výběr CSV souboru pro CS114
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Otevřít CSV soubor s měřením CS114"),
        "",
        tr("CSV soubory (*.csv);;Všechny soubory (*.*)")
        );

    if (filePath.isEmpty()) {
        return; // Uživatel zrušil výběr
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Chyba"), tr("Nelze otevřít vybraný soubor."));
        return;
    }

    QTextStream in(&file);

    // 2. Načtení záhlaví (hlavičky)
    if (in.atEnd()) {
        QMessageBox::warning(this, tr("Varování"), tr("Soubor je prázdný."));
        return;
    }

    QString headerLine = in.readLine();
    QStringList headers = headerLine.split(';');
    int columnCount = headers.size();

    // První sloupec je frekvence, ostatní sloupce jsou datové křivky (Y)
    if (columnCount < 2) {
        QMessageBox::warning(this, tr("Chyba struktury"), tr("Soubor musí mít alespoň dva sloupce (Frekvence a Data)."));
        return;
    }

    int curveCount = columnCount - 1; // Počet datových křivek k vykreslení

    // Společný vektor pro frekvence (1. sloupec) a vektory pro jednotlivé křivky
    QVector<double> freqs;
    QVector<QVector<double>> allCurves(curveCount);

    // 3. Načtení dat ze souboru řádek po řádku
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.trimmed().isEmpty()) {
            continue;
        }

        QStringList tokens = line.split(';');
        if (tokens.isEmpty()) {
            continue;
        }

        // Načtení frekvence z 1. sloupce
        QString freqStr = tokens.at(0).trimmed();
        freqStr.replace(',', '.'); // Ošetření pro případ čárky i tečky

        bool okFreq;
        double freqVal = freqStr.toDouble(&okFreq);
        if (!okFreq) {
            continue; // Přeskočit neplatný řádek
        }

        freqs.append(freqVal);

        // Načtení datových hodnot pro jednotlivé křivky
        int tokensToProcess = qMin(tokens.size(), columnCount);
        for (int i = 1; i < tokensToProcess; ++i) {
            int curveIdx = i - 1;
            QString valStr = tokens.at(i).trimmed();
            valStr.replace(',', '.');

            bool okVal;
            double val = valStr.toDouble(&okVal);
            if (okVal) {
                allCurves[curveIdx].append(val);
            } else {
                // Pokud chybí hodnota, vloží se nula nebo předchozí hodnota (případně QCP umožňuje vynechat)
                allCurves[curveIdx].append(0.0);
            }
        }
    }
    file.close();

    if (freqs.isEmpty()) {
        QMessageBox::warning(this, tr("Chyba"), tr("V souboru nebyla nalezena žádná platná data."));
        return;
    }

    // 4. Určení frekvenčního rozsahu z načtených dat
    double fstart = freqs.first();
    double fstop = freqs.last();

    // 5. Příprava filtrovaných frekvencí a nastavení grafu (volání vašich metod)
    QVector<double> testFreqs = getFilteredCS114Frequencies(fstart, fstop);
    setupPlotCSMeasure(ui->qcustomplotWidget_2, fstart, fstop, testFreqs);

    // 6. Vykreslení dat do ui->qcustomplotWidget_2
   // ui->qcustomplotWidget_2->clearGraphs();

    // Barevná paleta pro grafy bez varování clazy (využívá hexadecimální zápis)
    QVector<QColor> m_graphColors;
    m_graphColors << QColor(0x1f77b4)  // Modrá
                  << QColor(0xd62728)  // Červená
                  << QColor(0x2ca02c)  // Zelená
                  << QColor(0x9467bd)  // Fialová
                  << QColor(0xff7f0e)  // Oranžová
                  << QColor(0x7f7f7f); // Šedá

    int colorIndex = 0;

    for (int i = 0; i < curveCount; ++i) {
        // Kontrola, aby počet prvků v křivce odpovídal počtu frekvencí
        if (allCurves[i].size() != freqs.size()) {
            // Zarovnáme velikost vektoru, pokud by na konci souboru chyběly hodnoty
            allCurves[i].resize(freqs.size());
        }

        // Název grafu z hlavičky
        QString graphName = headers.at(i + 1).trimmed();

        QCPGraph *graph = ui->qcustomplotWidget_2->addGraph();
        graph->setName(graphName);
        graph->setData(freqs, allCurves[i]);

        // Nastavení barvy z palety (střídání pomocí modulo)
        QColor color = m_graphColors.at(colorIndex % m_graphColors.size());
        QPen pen(color, 1, Qt::SolidLine);
        graph->setPen(pen);

        colorIndex++;
    }

    // Překreslení grafu 2
    ui->qcustomplotWidget_2->replot();
}

QVector<double> MainWindow::getFilteredCS114Frequencies(double fstart, double fstop) {
    QVector<double> allFreqs = generateCS114Frequencies();
    QVector<double> filteredFreqs;

    // std::as_const prevents detachment/copying of the Qt container
    for (double f : std::as_const(allFreqs)) {
        if (f >= fstart && f <= fstop) filteredFreqs.append(f);
    }
    if (!filteredFreqs.isEmpty() && filteredFreqs.last() < fstop) filteredFreqs.append(fstop);

    return filteredFreqs;
}

void MainWindow::sweepS21Measurement()
{
    stopFlag = std::make_shared<std::atomic<bool>>(false);
    pauseFlag = std::make_shared<std::atomic<bool>>(false);

    // 1. Získání aktuálního času pro formát ddmmyy_hhmmss
    QDateTime now = QDateTime::currentDateTime();
    QString timestamp = now.toString("ddMMyy_hhmmss");

    // 2. Sestavení názvu souboru
    QString defaultFileName = QString("Sweep_measurement_%1.csv").arg(timestamp);

    // 3. Spojení cesty k dokumentům a názvu souboru
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                          + "/" + defaultFileName;

    // 4. Vyvolání dialogu (getSaveFileName automaticky předvyplní název z cesty)
    filePath = QFileDialog::getSaveFileName(this,
                                            tr("Save Measurement"),
                                            defaultPath,
                                            tr("CSV Files (*.csv)"));
    if (filePath.isEmpty()) return;

    ui->calibrateCS->setEnabled(false);
    EMC.fstart = ui->fstartCSSpinBox_2->value() * 1e6;
    EMC.fstop = ui->fstopCSSpinBox_2->value() * 1e6;
    EMC.sweepVoltagedBuV = ui->SweepVoltageSpinBox->value();
    EMC.Curentenable = ui->CurrentcheckBox->isChecked();
    bool correction_enable = ui->CorrcheckBox->isChecked();

    QVector<double> testFreqs = getFilteredCS114Frequencies(EMC.fstart, EMC.fstop);

    setupPlotS21(ui->qcustomplotWidget_2, EMC.fstart, EMC.fstop, testFreqs);
    //setupPlotClearLimit(ui->qcustomplotWidget_2, testFreqs);

    auto *watcher = new QFutureWatcher<void>(this);

    QString filename = SetFilename(ui->unitEdit->text(),
                                   "S21",
                                   "measurement",
                                   ui->fstartCSSpinBox->value(),
                                   ui->fstopCSSpinBox->value());

    connect(watcher, &QFutureWatcher<void>::finished, this, [this, watcher, filename]() {
        ui->qcustomplotWidget_2->savePdf(filename);
        ui->calibrateCS->setEnabled(true);
        watcher->deleteLater();
    });

    // Delegate long measurement to the Manager
    QFuture<void> future = QtConcurrent::run([=, this]() {
        measurementManager->runS21Measure(&EMC, testFreqs, filePath, stopFlag, pauseFlag, correction_enable);
    });
    watcher->setFuture(future);
}

void MainWindow::onCS114Clicked()
{
    stopFlag = std::make_shared<std::atomic<bool>>(false);
    pauseFlag = std::make_shared<std::atomic<bool>>(false);

    ui->calibrateCS->setEnabled(false);
    EMC.fstart = ui->fstartCSSpinBox->value() * 1e6;
    EMC.fstop = ui->fstopCSSpinBox->value() * 1e6;
    QVector<double> testFreqs = getFilteredCS114Frequencies(EMC.fstart, EMC.fstop);

    setupPlotCS(ui->qcustomplotWidget_2, EMC.fstart, EMC.fstop, testFreqs);

    auto *watcher = new QFutureWatcher<void>(this);

    QString filename = SetFilename(ui->unitEdit->text(),
                                   "CS114",
                                   "calibration",
                                   ui->fstartCSSpinBox->value(),
                                   ui->fstopCSSpinBox->value());

    connect(watcher, &QFutureWatcher<void>::finished, this, [this, watcher, filename]() {
        onPAStop();
        ui->qcustomplotWidget_2->savePdf(filename);
        ui->calibrateCS->setEnabled(true);
        watcher->deleteLater();
    });

    // Delegate long measurement to the Manager
    QFuture<void> future = QtConcurrent::run([=, this]() {
        measurementManager->runEsiCalibration(&EMC, testFreqs, filePath, stopFlag, pauseFlag);
    });
    watcher->setFuture(future);
}

void MainWindow::onCS114MeasureClicked() {
    stopFlag = std::make_shared<std::atomic<bool>>(false);
    pauseFlag = std::make_shared<std::atomic<bool>>(false);

    ui->measureCS->setEnabled(false);
    EMC.fstart = ui->fstartCSSpinBox->value() * 1e6;
    EMC.fstop = ui->fstopCSSpinBox->value() * 1e6;
    EMC.AMenable = ui->AMcheckBox->isChecked();
    EMC.safetyDropdB = ui->SafetyDropSpinBox->value();

    QVector<double> testFreqs = getFilteredCS114Frequencies(EMC.fstart, EMC.fstop);

    setupPlotCSMeasure(ui->qcustomplotWidget_2, EMC.fstart, EMC.fstop, testFreqs);

    QVector<double> dummyFreq = { EMC.fstart };
    QVector<double> dummyValue = { 0.0 };
    QCustomPlot *customPlot = ui->qcustomplotWidget_2;

    customPlot->addGraph();
    customPlot->graph(1)->setPen(QPen(QColor(0x1f77b4)));
    customPlot->graph(1)->setName("Calibrated level");
    customPlot->graph(1)->setData(dummyFreq, dummyValue); // Dočasný bod

    customPlot->addGraph();
    customPlot->graph(2)->setPen(QPen(QColor(0xff7f0e)));
    customPlot->graph(2)->setName("Measured current");
    customPlot->graph(2)->setData(dummyFreq, dummyValue); // Dočasný bod

    customPlot->addGraph();
    customPlot->graph(3)->setPen(QPen(QColor(0x2ca02c)));
    customPlot->graph(3)->setName("Generator level");
    customPlot->graph(3)->setData(dummyFreq, dummyValue); // Dočasný bod

    customPlot->addGraph();
    customPlot->graph(4)->setPen(QPen(QColor(0x9467bd)));
    customPlot->graph(4)->setName("Limit Imax");
    customPlot->graph(4)->setData(dummyFreq, dummyValue); // Dočasný bod

    auto *watcher = new QFutureWatcher<void>(this);

    QString filename = SetFilename(ui->unitEdit->text(),
                                   "CS114",
                                   "measurement",
                                   ui->fstartCSSpinBox->value(),
                                   ui->fstopCSSpinBox->value());

    connect(watcher, &QFutureWatcher<void>::finished, this, [this, watcher, filename]() {
        onPAStop();
        ui->qcustomplotWidget_2->savePdf(filename);
        ui->measureCS->setEnabled(true);
        watcher->deleteLater();
    });

    // Delegate long measurement to the Manager
    QFuture<void> future = QtConcurrent::run([=, this]() {
        measurementManager->runEsiTest(&EMC, filePath,filePath_2, stopFlag, pauseFlag);
    });
    watcher->setFuture(future);
}

void MainWindow::on_tracerSpinBox_valueChanged(int arg1) {
    // Ujistíme se, že tracer existuje
    if (!m_tracer) return;

    // Hodnota ve SpinBoxu je od 1 výše (Step 1, Step 2...)
    // QCustomPlot ale indexuje grafy od 0 (0, 1, 2...)
    int graphIndex = arg1 - 1;

    // Zkontrolujeme, zda takový graf skutečně existuje
    if (graphIndex >= 0 && graphIndex < ui->qcustomplotWidget_2->graphCount()) {

        // Získáme ukazatel na vybraný graf
        QCPGraph *selectedGraph = ui->qcustomplotWidget_2->graph(graphIndex);

        // Přepneme tracer na tento graf
        m_tracer->setGraph(selectedGraph);

        // Aby se změna hned projevila, aktualizujeme pozici traceru a překreslíme
        m_tracer->updatePosition();
        ui->qcustomplotWidget_2->replot();
    }
}

void MainWindow::prepareAndStartCalibrationCS114()
{
    // 1. Získání aktuálního času pro formát ddmmyy_hhmmss
    QDateTime now = QDateTime::currentDateTime();
    QString timestamp = now.toString("ddMMyy_hhmmss");

    // 2. Sestavení názvu souboru
    QString defaultFileName = QString("CS114_3_calibration_%1.csv").arg(timestamp);

    // 3. Spojení cesty k dokumentům a názvu souboru
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                          + "/" + defaultFileName;

    // 4. Vyvolání dialogu (getSaveFileName automaticky předvyplní název z cesty)
    filePath = QFileDialog::getSaveFileName(this,
                                            tr("Save Measurement"),
                                            defaultPath,
                                            tr("CSV Files (*.csv)"));
    if (filePath.isEmpty()) return;

    //onCS114Clicked();
    //return;

    // 1. Připojíme signál na slot, který spustí samotné měření
    // Používáme Qt::UniqueConnection, aby se nám slot nepřipojil vícekrát
    connect(this, &MainWindow::paSequenceFinished, this, &MainWindow::onPaReadyForCalibrationCS114, Qt::UniqueConnection);

    // 2. Připojíme časovač na zpracování stavového automatu
    connect(&paTimer, &QTimer::timeout, this, &MainWindow::processPaRun, Qt::UniqueConnection);

    // 3. Nastavíme počáteční stav a spustíme automat
    paState = PaRunState::SetRemote;
    paTimer.start(200); // Rychlejší reakce (100 ms)
}

void MainWindow::prepareAndStartMeasurementCS114() {
    filePath = QFileDialog::getOpenFileName(this, tr("Open Measurement"),
                                            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
                                            tr("CSV Files (*.csv)"));

    if (filePath.isEmpty()) return;

    // 1. Získání aktuálního času pro formát ddmmyy_hhmmss
    QDateTime now = QDateTime::currentDateTime();
    QString timestamp = now.toString("ddMMyy_hhmmss");

    // 2. Sestavení názvu souboru
    QString defaultFileName = QString("CS114_3_measurement_%1.csv").arg(timestamp);

    // 3. Spojení cesty k dokumentům a názvu souboru
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                          + "/" + defaultFileName;

    // 4. Vyvolání dialogu (getSaveFileName automaticky předvyplní název z cesty)
    filePath_2 = QFileDialog::getSaveFileName(this,
                                              tr("Save Measurement"),
                                              defaultPath,
                                              tr("CSV Files (*.csv)"));

    //onCS114MeasureClicked();
    //return;

    // 1. Připojíme signál na slot, který spustí samotné měření
    // Používáme Qt::UniqueConnection, aby se nám slot nepřipojil vícekrát
    connect(this, &MainWindow::paSequenceFinished, this, &MainWindow::onPaReadyForMeasurementCS114, Qt::UniqueConnection);

    // 2. Připojíme časovač na zpracování stavového automatu
    connect(&paTimer, &QTimer::timeout, this, &MainWindow::processPaRun, Qt::UniqueConnection);

    // 3. Nastavíme počáteční stav a spustíme automat
    paState = PaRunState::SetRemote;
    paTimer.start(200); // Rychlejší reakce (100 ms)
}

void MainWindow::onPaReadyForCalibrationCS114(bool success)
{
    // Odpojíme se, abychom při příštím měření nevolali slot znovu (pokud nepoužíváte UniqueConnection)
    disconnect(this, &MainWindow::paSequenceFinished, this, &MainWindow::onPaReadyForCalibrationCS114);

    if (success) {
        qDebug() << "PA je připraven, spouštím měření...";
        onCS114Clicked();
    } else {
        qDebug() << "Chyba při přípravě PA. Měření zrušeno.";
        // Zobrazit uživateli varování
    }
}

void MainWindow::onPaReadyForMeasurementCS114(bool success) {
    // Odpojíme se, abychom při příštím měření nevolali slot znovu (pokud nepoužíváte UniqueConnection)
    disconnect(this, &MainWindow::paSequenceFinished, this, &MainWindow::onPaReadyForMeasurementCS114);

    if (success) {
        qDebug() << "PA je připraven, spouštím měření...";
        onCS114MeasureClicked();
    } else {
        qDebug() << "Chyba při přípravě PA. Měření zrušeno.";
        // Zobrazit uživateli varování
    }
}

void MainWindow::on_corectSMT_clicked() {
    filePath = QFileDialog::getOpenFileName(this, tr("Open Measurement"),
                                            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
                                            tr("CSV Files (*.csv)"));

    if (filePath.isEmpty()) return;
    bool inc_atten_enable = ui->CorrcheckBox_2->isChecked();
    int inc_attenuator = 0;
    if(inc_atten_enable)
    {
        inc_attenuator = ui->PAAttenuaterSpinBox->value();
    }

    measurementManager->SmtUserCorrection(&EMC, filePath, inc_attenuator);
}

