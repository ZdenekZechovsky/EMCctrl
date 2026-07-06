#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "StatusBarManager.h"

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

void MainWindow::vertical1Clicked() {
    if(ui->verticalButton_1->text() == "Vertical") {
        ui->verticalButton_1->setText("Horizontal");
    }
    else if(ui->verticalButton_1->text() == "Horizontal") {
        ui->verticalButton_1->setText("Vertical");
    }
    else if(ui->verticalButton_1->text() == "Plus") {
        ui->verticalButton_1->setText("Minus");
    }
    else if(ui->verticalButton_1->text() == "Minus") {
        ui->verticalButton_1->setText("Plus");
    }
}

void MainWindow::cleanSpectrum()
{
    int ord = ui->comboBox_1->currentData().toInt();
    setupPlot(ui->qcustomplotWidget, ui->fstartSpinBox_1->value() * 1e6, ui->fstopSpinBox_1->value() * 1e6, ord);
}

void MainWindow::prepareNewGraphSpectrum()
{
    m_currentGraph = ui->qcustomplotWidget->addGraph();

    // stepNumber je počet grafů (1 pro první, 2 pro druhý...)
    int stepNumber = ui->qcustomplotWidget->graphCount();

    int ord = ui->comboBox_1->currentData().toInt();
    if(ord < 4)
    {
        if((stepNumber - 1) <= 3) {
            m_colorIndex = 0;
        }
        m_currentGraph->setName(QString("Trace %1 %2").arg(stepNumber - 3).arg(ui->lineEdit_1->text()));
    }
    else {
        if((stepNumber - 1) <= 1) {
            m_colorIndex = 0;
        }
        m_currentGraph->setName(QString("Trace %1 %2").arg(stepNumber - 1).arg(ui->lineEdit_1->text()));
    }

    QColor color = m_graphColors[m_colorIndex % m_graphColors.size()];
    m_colorIndex++;

    QPen pen;
    pen.setColor(color);
    pen.setWidth(1);
    m_currentGraph->setPen(pen);

    // --- NOVÁ ČÁST PRO SPINBOX ---
    // Povolíme výběr nového čísla grafu
    ui->tracerSpinBox_1->setMaximum(stepNumber);

    // Automaticky přepneme SpinBox na tento nový graf.
    // (Tím se automaticky zavolá i náš slot on_tracerSpinBox_valueChanged
    // a tracer se korektně přepne).
    ui->tracerSpinBox_1->setValue(stepNumber);
}

void MainWindow::TransducerChanged()
{
    switch(ui->comboBox_1->currentData().toInt())
    {
    case 0:
        ui->fstartSpinBox_1->setMinimum(2.0);
        ui->fstartSpinBox_1->setMaximum(30.0);

        ui->fstopSpinBox_1->setMinimum(2.0);
        ui->fstopSpinBox_1->setMaximum(30.0);

        ui->verticalButton_1->setText("Horizontal");
        break;

    case 1:
        ui->fstartSpinBox_1->setMinimum(30.0);
        ui->fstartSpinBox_1->setMaximum(300.0);

        ui->fstopSpinBox_1->setMinimum(30.0);
        ui->fstopSpinBox_1->setMaximum(300.0);

        ui->verticalButton_1->setText("Horizontal");
        break;

    case 2:
        ui->fstartSpinBox_1->setMinimum(300.0);
        ui->fstartSpinBox_1->setMaximum(1000.0);

        ui->fstopSpinBox_1->setMinimum(300.0);
        ui->fstopSpinBox_1->setMaximum(1000.0);

        ui->verticalButton_1->setText("Horizontal");
        break;

    case 3:
        ui->fstartSpinBox_1->setMinimum(1000.0);
        ui->fstartSpinBox_1->setMaximum(6000.0);

        ui->fstopSpinBox_1->setMinimum(1000.0);
        ui->fstopSpinBox_1->setMaximum(6000.0);

        ui->verticalButton_1->setText("Horizontal");
        break;

    case 4:
        ui->fstartSpinBox_1->setMinimum(0.01);
        ui->fstartSpinBox_1->setMaximum(10.0);

        ui->fstopSpinBox_1->setMinimum(0.01);
        ui->fstopSpinBox_1->setMaximum(10.0);

        ui->verticalButton_1->setText("Plus");
        break;
    }
    ui->fstartSpinBox_1->setValue(ui->fstartSpinBox_1->minimum());
    ui->fstopSpinBox_1->setValue(ui->fstopSpinBox_1->maximum());

    cleanSpectrum();
    //prepareNewGraphSpectrum();
    ui->tracerSpinBox_1->setMaximum(1);
    ui->tracerSpinBox_1->setValue(1);
}

// --- Measurement Logic Wrappers ---
void MainWindow::onSpectrum1Clicked() {
    int ord = ui->comboBox_1->currentData().toInt();
    spectrumProcess(ui->fstartSpinBox_1->value() * 1e6, ui->fstopSpinBox_1->value() * 1e6, ord);
}

QString MainWindow::SetFilename(QString devicename,
                                QString measurement,
                                QString note,
                                double fstart,
                                double fstop)
{
    // 1. Získání cesty do složky Dokumenty uživatele
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);

    // 2. Název zařízení ze stejného pole, které používáte v názvu souboru
    QString deviceName = ui->unitEdit->text().trimmed();
    if (deviceName.isEmpty()) {
        deviceName = "Default_Unit"; // Pojistka pro prázdný text
    }

    // 3. Příprava objektu adresáře a jeho vytvoření
    QDir dir(baseDir + "/" + deviceName);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");

    // 4. Finální sestavení cesty k souboru (včetně složky)
    QString filename = dir.filePath(
        QString("%1_%2_from_%3_to_%4_MHz_%5.pdf")
            .arg(measurement,
                 note,
                 QString::number(fstart / 1e6),
                 QString::number(fstop / 1e6),
                 timestamp)
        );
    return  filename;
}

void exportAllGraphsToCsv(QCustomPlot *customPlot, const QString &pdfFilename)
{
    // 1. Automatická změna přípony z jakékoliv na .csv
    QFileInfo fileInfo(pdfFilename);
    // completeBaseName() vrátí název bez cesty a bez poslední přípony (až po poslední tečku)
    QString csvFilename = fileInfo.absolutePath() + "/" + fileInfo.completeBaseName() + ".csv";

    // 2. Otevření souboru pro zápis
    QFile file(csvFilename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Nepodařilo se otevřít soubor pro zápis:" << csvFilename;
        return;
    }

    QTextStream out(&file);
    const QString separator = ";"; // Středník je ideální pro český Excel

    int graphCount = customPlot->graphCount();
    if (graphCount == 0) {
        qWarning() << "QCustomPlot neobsahuje žádné grafy k exportu.";
        file.close();
        return;
    }

    // 3. Zápis hlavičky tabulky (pro všechny grafy vedle sebe)
    // Výsledek bude: Graf 1 (X);Graf 1 (Y);Graf 2 (X);Graf 2 (Y);...
    for (int i = 0; i < graphCount; ++i) {
        QString graphName = customPlot->graph(i)->name();
        if (graphName.isEmpty()) {
            graphName = QString("Graf %1").arg(i + 1);
        }

        out << graphName << " (X)" << separator << graphName << " (Y)";
        if (i < graphCount - 1) out << separator;
    }
    out << "\n";

    // 4. Příprava iterátorů pro všechny grafy
    QVector<QCPGraphDataContainer::const_iterator> iterators(graphCount);
    QVector<QCPGraphDataContainer::const_iterator> ends(graphCount);

    for (int i = 0; i < graphCount; ++i) {
        auto dataContainer = customPlot->graph(i)->data();
        iterators[i] = dataContainer->begin();
        ends[i] = dataContainer->end();
    }

    // 5. Hlavní cyklus: zapisujeme řádky, dokud má alespoň jeden graf data
    bool dataAvailable = true;
    while (dataAvailable) {
        dataAvailable = false; // Předpokládáme, že už nikde data nejsou

        for (int i = 0; i < graphCount; ++i) {
            // Pokud tento konkrétní graf ještě má data k zápisu
            if (iterators[i] != ends[i]) {
                // Formátování čísel s nahrazením tečky za čárku pro český Excel
                QString xStr = QString::number(iterators[i]->key, 'f', 4).replace('.', ',');
                QString yStr = QString::number(iterators[i]->value, 'f', 4).replace('.', ',');

                out << xStr << separator << yStr;

                ++iterators[i]; // Posun na další bod v tomto grafu
                dataAvailable = true; // Našli jsme data, budeme pokračovat dalším řádkem
            } else {
                // Pokud tento graf už data nemá, ale ostatní ano, necháme prázdné buňky
                out << separator;
            }

            // Pokud to není poslední graf v řádku, přidáme oddělovač mezi grafy
            if (i < graphCount - 1) {
                out << separator;
            }
        }
        out << "\n";
    }

    file.close();
    //qDebug() << "Data byla úspěšně exportována do:" << csvFilename;
}

void MainWindow::spectrumProcess(double fstart, double fstop, int ord)
{
    stopFlag = std::make_shared<std::atomic<bool>>(false);
    pauseFlag = std::make_shared<std::atomic<bool>>(false);

    ui->progressBar->setValue(0);
    int stepNumber = ui->qcustomplotWidget->graphCount();

    static double fstart_old = 0, fstop_old = 0;

    if( fstart != fstart_old ||
        fstop != fstop_old ||
        (ord < 4 && stepNumber == 3) ||
        (ord >= 4 && stepNumber == 1))  {
        setupPlot(ui->qcustomplotWidget, fstart, fstop, ord);
        fstart_old = fstart;
        fstop_old = fstop;
    }

    // Initialize ESI through hardware layer
    QString measurement_name_str;

    int data_chunk_time = ui->tracerSpinBox_2->value();

    if(ord <= 3) {
        gpibDevice->initSpectrumRead(EMC.ESI_addr, fstart, fstop, ord, data_chunk_time);
        measurement_name_str = "RE-102";
    }
    else {
        // Initialize ESI through hardware layer
        gpibDevice->initSpectrumReadCE102(EMC.ESI_addr, fstart, fstop, data_chunk_time);
        measurement_name_str = "CE-102";
    }

    QString polarizationstr = ui->verticalButton_1->text();

    auto *watcher = new QFutureWatcher<void>(this);

    QString filename = SetFilename(ui->unitEdit->text(),
                                   measurement_name_str,
                                   polarizationstr,
                                   fstart,
                                   fstop);

    connect(watcher, &QFutureWatcher<void>::finished, this, [this, watcher, filename]() {
        ui->qcustomplotWidget->savePdf(filename);
        // Uloží surová data do C:/Projekty/mereni_vysledek.csv
        exportAllGraphsToCsv(ui->qcustomplotWidget, filename);
        watcher->deleteLater();
    });

    // Run measurement in background thread via Manager
    QFuture<void> future = QtConcurrent::run([=, this]() {
        measurementManager->runSpectrumRead(EMC.ESI_addr, fstart, fstop, stopFlag, pauseFlag, data_chunk_time);
        StatusBarManager::instance().showMessage(QString("Graph saved to %1").arg(filename));
    });
    watcher->setFuture(future);
}

void MainWindow::on_tracerSpinBox_1_valueChanged(int arg1)
{
    if (!m_tracer) return;

    // Hodnota ve SpinBoxu je od 1 výše (Step 1, Step 2...)
    // QCustomPlot ale indexuje grafy od 0 (0, 1, 2...)
    int graphIndex = arg1 - 1;

    // Zkontrolujeme, zda takový graf skutečně existuje
    if (graphIndex >= 0 && graphIndex < ui->qcustomplotWidget->graphCount()) {

        // Získáme ukazatel na vybraný graf
        QCPGraph *selectedGraph = ui->qcustomplotWidget->graph(graphIndex);

        // Přepneme tracer na tento graf
        m_tracer->setGraph(selectedGraph);

        // --- TADY JE TA OPRAVA ---
        // updatePosition() zavoláme POUZE tehdy, pokud graf už obsahuje reálná data.
        // Pokud je graf prázdný (protože jsme ho zrovna vyrobili), updatePosition vynecháme.
        if (selectedGraph && !selectedGraph->data()->isEmpty()) {
            m_tracer->setVisible(true);
            m_tracerLabel->setVisible(true);
            m_tracer->updatePosition();
        } else {
            // Pokud graf data nemá, tracer prozatím schováme, aby nestrašil na špatných souřadnicích
            m_tracer->setVisible(false);
            m_tracerLabel->setVisible(false);
        }
        ui->qcustomplotWidget->replot();
    }
}
