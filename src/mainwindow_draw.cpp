#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "rf_limits.h"
#include "qcustomplot.h"
#include "PowerAmplifier.h"
#include "EmcMeasurementManager.h"

extern tESI_SCAN_RANGES ranges[4];

class CISPR16Ticker : public QCPAxisTickerText
{
protected:
    QString format(double hz) const
    {
        double v = hz;
        QString u;

        if (v >= 1e9) { v /= 1e9; u = "G"; }
        else if (v >= 1e6) { v /= 1e6; u = "M"; }
        else if (v >= 1e3) { v /= 1e3; u = "k"; }
        else u = " ";

        return QString::number(v, 'g', 3) + u;
    }

public:
    void generate(double lower, double upper)
    {
        clear();
        int d1 = floor(log10(lower));
        int d2 = ceil(log10(upper));

        for (int d = d1; d <= d2; ++d)
        {
            double decade = pow(10.0, d);
            double major = 1.0 * decade;
            if (major >= lower && major <= upper)
                addTick(major, format(major));

            for (int i = 2; i <= 9; ++i)
            {
                double val = i * decade;
                if (val >= lower && val <= upper)
                    addTick(val, "");
            }
        }
    }
};

class MhzTicker : public QCPAxisTicker {
protected:
    virtual QString getTickLabel(double tick, const QLocale &locale, QChar format, int precision) override {
        return QCPAxisTicker::getTickLabel(tick / 1e6, locale, format, precision);
    }
};

void MainWindow::setupPlotCS(QCustomPlot *customPlot, double fstart, double fstop, const QVector<double>& testFreqs)
{
    QCPTextElement *titleElement;
    QCPTextElement *subTitleElement;

    disconnect(customPlot, &QCustomPlot::mouseMove, this, &MainWindow::onMouseMove);

    if (customPlot->plotLayout()->rowCount() <= 1) {
        customPlot->plotLayout()->insertRow(0);
        customPlot->plotLayout()->insertRow(1);
        titleElement = new QCPTextElement(customPlot);
        subTitleElement = new QCPTextElement(customPlot);
        customPlot->plotLayout()->addElement(0, 0, titleElement);
        customPlot->plotLayout()->addElement(1, 0, subTitleElement);
    } else {
        titleElement = qobject_cast<QCPTextElement*>(customPlot->plotLayout()->element(0, 0));
    }

    bool useLog = (fstop / fstart) >= 5.0;
    if (useLog) {
        QSharedPointer<CISPR16Ticker> ticker(new CISPR16Ticker);
        customPlot->xAxis->setScaleType(QCPAxis::stLogarithmic);
        customPlot->xAxis->setTicker(ticker);
        ticker->generate(1e4, 2e8);
    } else {
        customPlot->xAxis->setScaleType(QCPAxis::stLinear);
        QSharedPointer<QCPAxisTicker> linTicker(new QCPAxisTicker);
        linTicker->setTickStepStrategy(QCPAxisTicker::tssReadability);
        customPlot->xAxis->setTicker(linTicker);
        customPlot->xAxis->setNumberFormat("f");
        customPlot->xAxis->setNumberPrecision(3);
        customPlot->xAxis->setTicker(QSharedPointer<MhzTicker>(new MhzTicker));
    }

    titleElement->setText(QString("%1 CS-114 from %2 MHz to %3 MHz, operator %4")
                              .arg(ui->unitEdit->text()).arg(fstart / 1e6).arg(fstop / 1e6).arg(ui->operatorEdit->text()));
    titleElement->setFont(QFont("sans", 12, QFont::Bold));

    customPlot->clearGraphs();
    customPlot->clearItems();

    customPlot->xAxis->setLabel("Frequency (MHz)");
    customPlot->xAxis->grid()->setSubGridVisible(true);
    customPlot->xAxis->setRange(fstart, fstop);
    customPlot->yAxis->setRange(0, 120);
    customPlot->yAxis->setLabel("Level [dBµV]");

    // Graf limitu
    customPlot->addGraph();
    customPlot->graph(0)->setPen(QPen(Qt::red, 2));
    customPlot->graph(0)->setName("CS114 limit");

    QVector<double> limitVals;
    for (double f : testFreqs) {
        limitVals.append(CS114_limit(f));
    }
    customPlot->graph(0)->setData(testFreqs, limitVals);

    // Graf měření
    customPlot->addGraph();
    customPlot->graph(1)->setPen(QPen(Qt::blue));
    customPlot->graph(1)->setName("Measurement");
    // Vložíme dočasný "dummy" bod, aby se měl tracer čeho chytit a neřval
    customPlot->graph(1)->addData(fstart, CS114_limit(fstart));

    customPlot->legend->setVisible(true);
    customPlot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop | Qt::AlignLeft);
    customPlot->legend->setBrush(QBrush(QColor(255, 255, 255, 180)));
    customPlot->setAntialiasedElements(QCP::aeAll);

    m_tracer = new QCPItemTracer(customPlot);
    m_tracer->setGraph(customPlot->graph(1));
    // ŘÁDEK ODSTRANĚN ODSUD: m_tracer->setGraphKey(...)
    m_tracer->setInterpolating(true);
    m_tracer->setStyle(QCPItemTracer::tsCrosshair);
    m_tracer->setPen(QPen(Qt::darkGray, 1, Qt::DashLine));
    m_tracer->setBrush(Qt::darkGray);
    m_tracer->setSize(10);

    m_tracerLabel = new QCPItemText(customPlot);
    m_tracerLabel->setLayer("overlay");
    m_tracerLabel->setPositionAlignment(Qt::AlignBottom | Qt::AlignLeft);
    m_tracerLabel->position->setParentAnchor(m_tracer->position);
    m_tracerLabel->setFont(QFont("sans", 9));
    m_tracerLabel->setPadding(QMargins(8, 0, 0, 0));

    // Bezpečnostní kontrola
    if (m_tracer->graph() && m_tracer->graph()->data() && !m_tracer->graph()->data()->isEmpty()) {
        // Až ZDE nastavíme pozici na X-ové ose, když víme, že máme data
        m_tracer->setGraphKey((fstop - fstart) / 2);

        m_tracer->updatePosition();
        double x = m_tracer->position->key();
        double y = m_tracer->position->value();
        m_tracerLabel->setText(QString("%1 MHz\n%2 dBµV").arg(x / 1e6, 0, 'f', 2).arg(y, 0, 'f', 1));

        m_tracer->setVisible(true);
        m_tracerLabel->setVisible(true);
    } else {
        // Pokud graf data nemá, tracer schováme a nastavíme výchozí text
        m_tracer->setVisible(false);
        m_tracerLabel->setVisible(false);
        m_tracerLabel->setText("No data");
    }

    connect(customPlot, &QCustomPlot::mouseMove, this, &MainWindow::onMouseMove);
    customPlot->replot();
}

void MainWindow::setupPlotCSMeasure(QCustomPlot *customPlot, double fstart, double fstop, const QVector<double>& testFreqs)
{
    QCPTextElement *titleElement;
    QCPTextElement *subTitleElement;

    disconnect(customPlot, &QCustomPlot::mouseMove, this, &MainWindow::onMouseMove);

    if (customPlot->plotLayout()->rowCount() <= 1) {
        customPlot->plotLayout()->insertRow(0);
        customPlot->plotLayout()->insertRow(1);
        titleElement = new QCPTextElement(customPlot);
        subTitleElement = new QCPTextElement(customPlot);
        customPlot->plotLayout()->addElement(0, 0, titleElement);
        customPlot->plotLayout()->addElement(1, 0, subTitleElement);
    } else {
        titleElement = qobject_cast<QCPTextElement*>(customPlot->plotLayout()->element(0, 0));
    }

    bool useLog = (fstop / fstart) >= 5.0;
    if (useLog) {
        QSharedPointer<CISPR16Ticker> ticker(new CISPR16Ticker);
        customPlot->xAxis->setScaleType(QCPAxis::stLogarithmic);
        customPlot->xAxis->setTicker(ticker);
        ticker->generate(1e4, 2e8);
    } else {
        customPlot->xAxis->setScaleType(QCPAxis::stLinear);
        QSharedPointer<QCPAxisTicker> linTicker(new QCPAxisTicker);
        linTicker->setTickStepStrategy(QCPAxisTicker::tssReadability);
        customPlot->xAxis->setTicker(linTicker);
        customPlot->xAxis->setNumberFormat("f");
        customPlot->xAxis->setNumberPrecision(3);
        customPlot->xAxis->setTicker(QSharedPointer<MhzTicker>(new MhzTicker));
    }

    titleElement->setText(QString("%1 CS-114 from %2 MHz to %3 MHz, operator %4")
                              .arg(ui->unitEdit->text()).arg(fstart / 1e6).arg(fstop / 1e6).arg(ui->operatorEdit->text()));
    titleElement->setFont(QFont("sans", 12, QFont::Bold));

    customPlot->clearGraphs();
    customPlot->clearItems();

    customPlot->xAxis->setLabel("Frequency (MHz)");
    customPlot->xAxis->grid()->setSubGridVisible(true);
    customPlot->xAxis->setRange(fstart, fstop);
    customPlot->yAxis->setRange(0, 120);
    customPlot->yAxis->setLabel("Level [dBµV]");

    // Graf limitu
    customPlot->addGraph();
    customPlot->graph(0)->setPen(QPen(Qt::red, 2));
    customPlot->graph(0)->setName("CS114 limit");

    QVector<double> limitVals;
    for (double f : testFreqs) {
        limitVals.append(CS114_limit(f));
    }
    customPlot->graph(0)->setData(testFreqs, limitVals);
    /*
    // Graf měření
    customPlot->addGraph();
    customPlot->graph(1)->setPen(QPen(Qt::blue));
    customPlot->graph(1)->setName("Measured current");
    customPlot->graph(1)->setData(dummyFreq, dummyValue); // Dočasný bod

    customPlot->addGraph();
    customPlot->graph(2)->setPen(QPen(Qt::green));
    customPlot->graph(2)->setName("Generator voltage");
    customPlot->graph(2)->setData(dummyFreq, dummyValue); // Dočasný bod

    customPlot->addGraph();
    customPlot->graph(3)->setPen(QPen(Qt::cyan));
    customPlot->graph(3)->setName("Limit Imax");
    customPlot->graph(3)->setData(dummyFreq, dummyValue); // Dočasný bod
    */
    customPlot->legend->setVisible(true);
    customPlot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop | Qt::AlignLeft);
    customPlot->legend->setBrush(QBrush(QColor(255, 255, 255, 180)));
    customPlot->setAntialiasedElements(QCP::aeAll);

    m_tracer = new QCPItemTracer(customPlot);
    m_tracer->setGraph(customPlot->graph(0));
    m_tracer->setInterpolating(true);
    m_tracer->setStyle(QCPItemTracer::tsCrosshair);
    m_tracer->setPen(QPen(Qt::darkGray, 1, Qt::DashLine));
    m_tracer->setBrush(Qt::darkGray);
    m_tracer->setSize(10);

    m_tracerLabel = new QCPItemText(customPlot);
    m_tracerLabel->setLayer("overlay");
    m_tracerLabel->setPositionAlignment(Qt::AlignBottom | Qt::AlignLeft);
    m_tracerLabel->position->setParentAnchor(m_tracer->position);
    m_tracerLabel->setFont(QFont("sans", 9));
    m_tracerLabel->setPadding(QMargins(8, 0, 0, 0));

    if (m_tracer->graph() && !m_tracer->graph()->data()->isEmpty()) {
        m_tracer->updatePosition();
        m_tracer->setGraphKey((fstop - fstart) / 2);
        double x = m_tracer->position->key();
        double y = m_tracer->position->value();
        m_tracerLabel->setText(QString("%1 MHz\n%2 dBµV").arg(x / 1e6, 0, 'f', 2).arg(y, 0, 'f', 1));
    } else {
        // Pokud graf data nemá, tracer schováme a nastavíme výchozí text
        m_tracer->setVisible(false);
        m_tracerLabel->setVisible(false);
        m_tracerLabel->setText("No data");
    }

    connect(customPlot, &QCustomPlot::mouseMove, this, &MainWindow::onMouseMove);
    customPlot->replot();
}

void MainWindow::setupPlotS21(QCustomPlot *customPlot, double fstart, double fstop, const QVector<double>& testFreqs)
{
    QCPTextElement *titleElement;
    QCPTextElement *subTitleElement;

    disconnect(customPlot, &QCustomPlot::mouseMove, this, &MainWindow::onMouseMove);

    customPlot->clearGraphs();
    customPlot->clearItems();

    if (customPlot->plotLayout()->rowCount() <= 1) {
        customPlot->plotLayout()->insertRow(0);
        customPlot->plotLayout()->insertRow(1);
        titleElement = new QCPTextElement(customPlot);
        subTitleElement = new QCPTextElement(customPlot);
        customPlot->plotLayout()->addElement(0, 0, titleElement);
        customPlot->plotLayout()->addElement(1, 0, subTitleElement);
    } else {
        titleElement = qobject_cast<QCPTextElement*>(customPlot->plotLayout()->element(0, 0));
    }

    if ((fstop / fstart) >= 3.0 && (fstop / fstart) < 75.0) {
        customPlot->xAxis->setScaleType(QCPAxis::stLogarithmic);
        QSharedPointer<QCPAxisTickerText> textTicker(new QCPAxisTickerText);

        // Zjistíme řádový rozsah pro exponenty (např. pro 1 GHz je to 9, pro 30 MHz to začne na 7)
        int startExp = static_cast<int>(std::floor(std::log10(fstart)));
        int stopExp = static_cast<int>(std::ceil(std::log10(fstop)));

        // Procházíme dekády pomocí celočíselného exponentu (např. exp = 6, 7, 8, 9)        
        for (int exp = startExp; exp <= stopExp; ++exp) {

            // Spočítáme aktuální dekádu (1e6, 1e7, 1e8, 1e9, atd.)
            double decade = std::pow(10.0, exp);

            for (int i = 1; i <= 9; ++i) {
                double tickValue = decade * i;

                // Zkontrolujeme, zda bod leží v našem zobrazovaném rozsahu (s malou rezervou kvůli float)
                if (tickValue >= (fstart * 0.99) && tickValue <= (fstop * 1.01)) {

                    // Zformátujeme popisek podle vzoru ze zkušebny (1G, 2G, 50M...)
                    QString label;
                    if (tickValue >= 1e9) {
                        label = QString("%1G").arg(tickValue / 1e9, 0, 'g', 3);
                    } else if (tickValue >= 1e6) {
                        label = QString("%1M").arg(tickValue / 1e6, 0, 'g', 3);
                    } else if (tickValue >= 1e3) {
                        label = QString("%1k").arg(tickValue / 1e3, 0, 'g', 3);
                    } else {
                        label = QString("%1").arg(tickValue, 0, 'g', 3);
                    }

                    // Přidáme hodnotu a její textový popisek na osu
                    textTicker->addTick(tickValue, label);
                }
            }
        }
        customPlot->xAxis->setTicker(textTicker);
    } else if ((fstop / fstart) < 3.0) {
        customPlot->xAxis->setScaleType(QCPAxis::stLinear);
        QSharedPointer<QCPAxisTicker> linTicker(new QCPAxisTicker);
        linTicker->setTickStepStrategy(QCPAxisTicker::tssReadability);
        customPlot->xAxis->setTicker(linTicker);
        customPlot->xAxis->setNumberFormat("f");
        customPlot->xAxis->setNumberPrecision(1);
        customPlot->xAxis->setTicker(QSharedPointer<MhzTicker>(new MhzTicker));
    }
    else if((fstop / fstart) >= 75.0)
    {

        QSharedPointer<CISPR16Ticker> ticker(new CISPR16Ticker);
        customPlot->xAxis->setScaleType(QCPAxis::stLogarithmic);
        customPlot->xAxis->setTicker(ticker);
        ticker->generate(1e4, 1500e9);
    }

    titleElement->setText(QString("%1 S21 parameter %2 MHz to %3 MHz, operator %4")
                              .arg(ui->unitEdit->text()).arg(fstart / 1e6).arg(fstop / 1e6).arg(ui->operatorEdit->text()));
    titleElement->setFont(QFont("sans", 12, QFont::Bold));

    customPlot->clearGraphs();
    customPlot->clearItems();

    customPlot->xAxis->setLabel("Frequency (MHz)");
    customPlot->xAxis->grid()->setSubGridVisible(true);
    customPlot->xAxis->setRange(fstart, fstop);
    customPlot->yAxis->setRange(-100, 10);
    //customPlot->yAxis->setRange(30, 70);

    customPlot->yAxis->setLabel("Level [dBµV]");
    customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

    // Graf limitu
    customPlot->addGraph();
    customPlot->graph(0)->setPen(QPen(Qt::red, 1));
    customPlot->graph(0)->setName("0dB limit");

    QVector<double> limitVals(testFreqs.size(), 0.0);
    customPlot->graph(0)->setData(testFreqs, limitVals);

    // Graf měření
    //customPlot->addGraph();

    customPlot->legend->setVisible(true);
    customPlot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop | Qt::AlignLeft);
    // Zapneme manuální řízení okrajů pro axisRect
    customPlot->axisRect()->setAutoMargins(QCP::msLeft | QCP::msTop | QCP::msBottom); // Pravý (msRight) vynecháme z automatiky
    customPlot->axisRect()->setMargins(QMargins(
        customPlot->axisRect()->margins().left(),
        customPlot->axisRect()->margins().top(),
        40, // <-- TADY: Natvrdo nastavíme pravý okraj na 40 pixelů (výchozí bývá cca 15-20)
        customPlot->axisRect()->margins().bottom()
        ));
    customPlot->legend->setBrush(QBrush(QColor(255, 255, 255, 180)));
    customPlot->setAntialiasedElements(QCP::aeAll);

    m_tracer = new QCPItemTracer(customPlot);
    m_tracer->setGraph(customPlot->graph(0));    
    m_tracer->setInterpolating(true);
    m_tracer->setStyle(QCPItemTracer::tsCrosshair);
    m_tracer->setPen(QPen(Qt::darkGray, 1, Qt::DashLine));
    m_tracer->setBrush(Qt::darkGray);
    m_tracer->setSize(10);

    m_tracerLabel = new QCPItemText(customPlot);
    m_tracerLabel->setLayer("overlay");
    m_tracerLabel->setPositionAlignment(Qt::AlignBottom | Qt::AlignLeft);
    m_tracerLabel->position->setParentAnchor(m_tracer->position);
    m_tracerLabel->setFont(QFont("sans", 9));
    m_tracerLabel->setPadding(QMargins(8, 0, 0, 0));

    if (m_tracer->graph() && !m_tracer->graph()->data()->isEmpty()) {
        m_tracer->setGraphKey((fstop - fstart) / 2);
        m_tracer->updatePosition();        
        double x = m_tracer->position->key();
        double y = m_tracer->position->value();
        m_tracerLabel->setText(QString("%1 MHz\n%2 dBµV").arg(x / 1e6, 0, 'f', 2).arg(y, 0, 'f', 1));
    } else {
        // Pokud graf data nemá, tracer schováme a nastavíme výchozí text
        m_tracer->setVisible(false);
        m_tracerLabel->setVisible(false);
        m_tracerLabel->setText("No data");
    }

    connect(customPlot, &QCustomPlot::mouseMove, this, &MainWindow::onMouseMove);
    customPlot->replot();
}

void MainWindow::setupPlot(QCustomPlot *customPlot, double fstart, double fstop, int order)
{
    QCPTextElement *titleElement;
    QCPTextElement *subTitleElement;

    disconnect(customPlot, &QCustomPlot::mouseMove, this, &MainWindow::onMouseMove);
    customPlot->clearGraphs();
    customPlot->clearItems();

    if (customPlot->plotLayout()->rowCount() <= 1) {
        customPlot->plotLayout()->insertRow(0);
        customPlot->plotLayout()->insertRow(1);
        titleElement = new QCPTextElement(customPlot);
        subTitleElement = new QCPTextElement(customPlot);
        customPlot->plotLayout()->addElement(0, 0, titleElement);
        customPlot->plotLayout()->addElement(1, 0, subTitleElement);
    } else {
        titleElement = qobject_cast<QCPTextElement*>(customPlot->plotLayout()->element(0, 0));
        subTitleElement = qobject_cast<QCPTextElement*>(customPlot->plotLayout()->element(1, 0));
    }

    QString notestr = ui->lineEdit_1->text();
    QString polarizationstr = ui->verticalButton_1->text();

    double rbw = ranges[0].Res_BW_kHz;
    double step = ranges[0].Step_size_kHz;
    unsigned int att = ranges[0].RF_Attn_dB;


    notestr = ui->lineEdit_1->text();
    polarizationstr = ui->verticalButton_1->text();

    switch(order) {
    case 0:

        rbw = ranges[1].Res_BW_kHz; step = ranges[1].Step_size_kHz; att = ranges[1].RF_Attn_dB;
        break;
    case 1:
        rbw = ranges[2].Res_BW_kHz; step = ranges[2].Step_size_kHz; att = ranges[2].RF_Attn_dB;
        break;
    case 2:
        rbw = ranges[2].Res_BW_kHz; step = ranges[2].Step_size_kHz; att = ranges[2].RF_Attn_dB;
        break;
    case 3:
        rbw = ranges[3].Res_BW_kHz; step = ranges[3].Step_size_kHz; att = ranges[3].RF_Attn_dB;
        break;
    case 4:
        rbw = ranges[0].Res_BW_kHz; step = ranges[0].Step_size_kHz; att = ranges[0].RF_Attn_dB;
        break;
    }

    if(order < 4) {
        titleElement->setText(QString("%1 RE-102 from %2 MHz to %3 MHz, operator %4")
                                  .arg(ui->unitEdit->text(), QString::number(fstart / 1e6), QString::number(fstop / 1e6), ui->operatorEdit->text()));
    }
    else {
        titleElement->setText(QString("%1 CE-102 from %2 MHz to %3 MHz, operator %4")
                                  .arg(ui->unitEdit->text(), QString::number(fstart / 1e6), QString::number(fstop / 1e6), ui->operatorEdit->text()));
    }
    titleElement->setFont(QFont("sans", 12, QFont::Bold));

    subTitleElement->setText(QString::asprintf("Note: %s | Polarization: %s \n RBW: %.1f kHz | Step: %.1f kHz | Att: %d dB",
                                               notestr.toUtf8().constData(), polarizationstr.toUtf8().constData(), rbw, step, att));
    subTitleElement->setFont(QFont("sans", 9));
    subTitleElement->setTextColor(Qt::darkGray);

    bool useLog;
    if(order < 4)
    {
        useLog = (fstop / fstart) >= 3.0;
        if (useLog) {
            customPlot->xAxis->setScaleType(QCPAxis::stLogarithmic);
            QSharedPointer<QCPAxisTickerText> textTicker(new QCPAxisTickerText);

            // Zjistíme řádový rozsah pro exponenty (např. pro 1 GHz je to 9, pro 30 MHz to začne na 7)
            int startExp = static_cast<int>(std::floor(std::log10(fstart)));
            int stopExp = static_cast<int>(std::ceil(std::log10(fstop)));

            // Procházíme dekády pomocí celočíselného exponentu (např. exp = 6, 7, 8, 9)
            for (int exp = startExp; exp <= stopExp; ++exp) {

                // Spočítáme aktuální dekádu (1e6, 1e7, 1e8, 1e9, atd.)
                double decade = std::pow(10.0, exp);

                for (int i = 1; i <= 9; ++i) {
                    double tickValue = decade * i;

                    // Zkontrolujeme, zda bod leží v našem zobrazovaném rozsahu (s malou rezervou kvůli float)
                    if (tickValue >= (fstart * 0.99) && tickValue <= (fstop * 1.01)) {

                        // Zformátujeme popisek podle vzoru ze zkušebny (1G, 2G, 50M...)
                        QString label;
                        if (tickValue >= 1e9) {
                            label = QString("%1G").arg(tickValue / 1e9, 0, 'g', 3);
                        } else if (tickValue >= 1e6) {
                            label = QString("%1M").arg(tickValue / 1e6, 0, 'g', 3);
                        } else if (tickValue >= 1e3) {
                            label = QString("%1k").arg(tickValue / 1e3, 0, 'g', 3);
                        } else {
                            label = QString("%1").arg(tickValue, 0, 'g', 3);
                        }

                        // Přidáme hodnotu a její textový popisek na osu
                        textTicker->addTick(tickValue, label);
                    }
                }
            }
            customPlot->xAxis->setTicker(textTicker);
        } else {
            customPlot->xAxis->setScaleType(QCPAxis::stLinear);
            QSharedPointer<QCPAxisTicker> linTicker(new QCPAxisTicker);
            linTicker->setTickStepStrategy(QCPAxisTicker::tssReadability);
            customPlot->xAxis->setTicker(linTicker);
            customPlot->xAxis->setNumberFormat("f");
            customPlot->xAxis->setNumberPrecision(1);
            customPlot->xAxis->setTicker(QSharedPointer<MhzTicker>(new MhzTicker));
        }

        customPlot->xAxis->setLabel("Frequency (Hz)");
        customPlot->yAxis->setLabel("Level [dBµV/m]");
        switch(order)
        {
            case 0:
                customPlot->yAxis->setRange(0, 60);
                break;
            case 1:
                customPlot->yAxis->setRange(0, 60);
                break;
            case 2:
                customPlot->yAxis->setRange(10, 70);
                break;
            case 3:
                customPlot->yAxis->setRange(20, 80);
                break;
        }
    }
    else
    {
        /*
        useLog = (fstop / fstart) >= 5.0;
        if (useLog) {
            QSharedPointer<CISPR16Ticker> ticker(new CISPR16Ticker);
            customPlot->xAxis->setScaleType(QCPAxis::stLogarithmic);
            customPlot->xAxis->setTicker(ticker);
            ticker->generate(1e4, 10e6);
        } else {
            customPlot->xAxis->setScaleType(QCPAxis::stLinear);
            QSharedPointer<QCPAxisTicker> linTicker(new QCPAxisTicker);
            linTicker->setTickStepStrategy(QCPAxisTicker::tssReadability);
            customPlot->xAxis->setTicker(linTicker);
            customPlot->xAxis->setNumberFormat("f");
            customPlot->xAxis->setNumberPrecision(3);
            customPlot->xAxis->setTicker(QSharedPointer<MhzTicker>(new MhzTicker));
        }
        customPlot->xAxis->setLabel("Frequency [Hz]");
        customPlot->yAxis->setLabel("Level [dBµV]");
        customPlot->yAxis->setRange(-30, 100);
*/
        useLog = (fstop / fstart) >= 5.0;
        if (useLog) {
            customPlot->xAxis->setScaleType(QCPAxis::stLogarithmic);
            QSharedPointer<QCPAxisTickerText> textTicker(new QCPAxisTickerText);

            // Zjistíme řádový rozsah pro exponenty (např. pro 1 GHz je to 9, pro 30 MHz to začne na 7)
            int startExp = static_cast<int>(std::floor(std::log10(fstart)));
            int stopExp = static_cast<int>(std::ceil(std::log10(fstop)));

            // Procházíme dekády pomocí celočíselného exponentu (např. exp = 6, 7, 8, 9)
            for (int exp = startExp; exp <= stopExp; ++exp) {

                // Spočítáme aktuální dekádu (1e6, 1e7, 1e8, 1e9, atd.)
                double decade = std::pow(10.0, exp);

                for (int i = 1; i <= 9; ++i) {
                    double tickValue = decade * i;

                    // Zkontrolujeme, zda bod leží v našem zobrazovaném rozsahu (s malou rezervou kvůli float)
                    if (tickValue >= (fstart * 0.99) && tickValue <= (fstop * 1.01)) {

                        // Zformátujeme popisek podle vzoru ze zkušebny (1G, 2G, 50M...)
                        QString label;
                        if (tickValue >= 1e9) {
                            label = QString("%1G").arg(tickValue / 1e9, 0, 'g', 3);
                        } else if (tickValue >= 1e6) {
                            label = QString("%1M").arg(tickValue / 1e6, 0, 'g', 3);
                        } else if (tickValue >= 1e3) {
                            label = QString("%1k").arg(tickValue / 1e3, 0, 'g', 3);
                        } else {
                            label = QString("%1").arg(tickValue, 0, 'g', 3);
                        }

                        // Přidáme hodnotu a její textový popisek na osu
                        textTicker->addTick(tickValue, label);
                    }
                }
            }
            customPlot->xAxis->setTicker(textTicker);
        } else {
            customPlot->xAxis->setScaleType(QCPAxis::stLinear);
            QSharedPointer<QCPAxisTicker> linTicker(new QCPAxisTicker);
            linTicker->setTickStepStrategy(QCPAxisTicker::tssReadability);
            customPlot->xAxis->setTicker(linTicker);
            customPlot->xAxis->setNumberFormat("f");
            customPlot->xAxis->setNumberPrecision(3);
            customPlot->xAxis->setTicker(QSharedPointer<MhzTicker>(new MhzTicker));
        }
        customPlot->xAxis->setLabel("Frequency [Hz]");
        customPlot->yAxis->setLabel("Level [dBµV]");
        customPlot->yAxis->setRange(-10, 100); // Upraven rozsah od -10 dBµV podle obrázku ze zkušebny
    }
    customPlot->xAxis->grid()->setSubGridVisible(true);
    customPlot->xAxis->setRange(fstart, fstop);
    // Povolí posun myší (panning) a zoomování kolečkem pro hlavní osy
    customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

    customPlot->axisRect()->setRangeDragAxes(customPlot->xAxis, customPlot->yAxis);
    customPlot->axisRect()->setRangeZoomAxes(customPlot->xAxis, customPlot->yAxis);

    QVector<double> freqs(256), limit1(256), limit2(256), limit3(256), limit4(256);

    if (useLog) {
        double logStart = std::log10(fstart);
        double logStop = std::log10(fstop);
        double logStep = (logStop - logStart) / 255.0;
        for(int i = 0; i < 256; ++i) {
            freqs[i] = std::pow(10, logStart + i * logStep);
            limit1[i] = RE102_limit(freqs[i], RE102_ORIGINAL);
            limit2[i] = RE102_limit(freqs[i], RE102_HELICOPTER);
            limit3[i] = RE102_limit(freqs[i], RE102_LARGE_AIRCRAFT);
            limit4[i] = CE102_limit(freqs[i]);            
        }
    } else {
        double linStep = (fstop - fstart) / 255.0;
        for(int i = 0; i < 256; ++i) {
            freqs[i] = fstart + i * linStep;
            limit1[i] = RE102_limit(freqs[i], RE102_ORIGINAL);
            limit2[i] = RE102_limit(freqs[i], RE102_HELICOPTER);
            limit3[i] = RE102_limit(freqs[i], RE102_LARGE_AIRCRAFT);
            limit4[i] = CE102_limit(freqs[i]);            
        }
    }

    if(order < 4)
    {

        QCPGraph *graphLimit1 = customPlot->addGraph();
        // Plná čára (výchozí)
        graphLimit1->setPen(QPen(Qt::red, 1, Qt::SolidLine));
        graphLimit1->setName("RE102 limit");
        graphLimit1->setData(freqs, limit1);

        QCPGraph *graphLimit2 = customPlot->addGraph();
        // Čárkovaná čára (šrafování)
        graphLimit2->setPen(QPen(Qt::red, 1, Qt::DashLine));
        graphLimit2->setName("RE102 limit (Helicopter)");
        graphLimit2->setData(freqs, limit2);

        QCPGraph *graphLimit3 = customPlot->addGraph();
        // Tečkovaná čára
        graphLimit3->setPen(QPen(Qt::red, 1, Qt::DotLine));
        graphLimit3->setName("RE102 limit (Large Aircraft)");
        graphLimit3->setData(freqs, limit3);
    }
    else
    {
        QCPGraph *graphLimit1 = customPlot->addGraph();
        // Plná čára (výchozí)
        graphLimit1->setPen(QPen(Qt::red, 1, Qt::SolidLine));
        graphLimit1->setName("CE102 limit");
        graphLimit1->setData(freqs, limit4);
    }

    customPlot->xAxis->setRange(fstart, fstop);
    customPlot->legend->setVisible(true);
    customPlot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop | Qt::AlignLeft);
    // Zapneme manuální řízení okrajů pro axisRect
    customPlot->axisRect()->setAutoMargins(QCP::msLeft | QCP::msTop | QCP::msBottom); // Pravý (msRight) vynecháme z automatiky
    customPlot->axisRect()->setMargins(QMargins(
        customPlot->axisRect()->margins().left(),
        customPlot->axisRect()->margins().top(),
        40, // <-- TADY: Natvrdo nastavíme pravý okraj na 40 pixelů (výchozí bývá cca 15-20)
        customPlot->axisRect()->margins().bottom()
        ));
    customPlot->legend->setBrush(QBrush(QColor(255, 255, 255, 180)));
    customPlot->setAntialiasedElements(QCP::aeAll);

    m_tracer = new QCPItemTracer(customPlot);
    //m_tracer->setGraph(customPlot->graph(0));
    m_tracer->setGraph(nullptr);
    m_tracer->setInterpolating(true);
    m_tracer->setStyle(QCPItemTracer::tsCrosshair);
    m_tracer->setPen(QPen(Qt::darkGray, 1, Qt::DashLine));
    m_tracer->setBrush(Qt::darkGray);
    m_tracer->setSize(10);

    m_tracerLabel = new QCPItemText(customPlot);
    m_tracerLabel->setLayer("overlay");
    m_tracerLabel->setPositionAlignment(Qt::AlignBottom | Qt::AlignLeft);
    m_tracerLabel->position->setParentAnchor(m_tracer->position);
    m_tracerLabel->setFont(QFont("sans", 9));
    m_tracerLabel->setPadding(QMargins(8, 0, 0, 0));

    // Kontrola, zda vybraný graf vůbec obsahuje data, než zavoláme updatePosition
    if (m_tracer->graph() && !m_tracer->graph()->data()->isEmpty()) {
        m_tracer->updatePosition();
        double x = m_tracer->position->key();
        double y = m_tracer->position->value();
        m_tracerLabel->setText(QString("%1 MHz\n%2 dBµV").arg(x / 1e6, 0, 'f', 2).arg(y, 0, 'f', 1));
    } else {
        // Pokud graf data nemá, tracer schováme a nastavíme výchozí text
        m_tracer->setVisible(false);
        m_tracerLabel->setVisible(false);
        m_tracerLabel->setText("No data");
    }

    connect(customPlot, &QCustomPlot::mouseMove, this, &MainWindow::onMouseMove);
    customPlot->replot();
}

void MainWindow::onMouseMove(QMouseEvent *event)
{
    if (!m_tracer) return;

    QCustomPlot *customPlot = qobject_cast<QCustomPlot*>(sender());
    if (!customPlot) return;

    // KONTROLA DAT: Pokud tracer nemá přiřazený graf, nebo je tento graf úplně prázdný,
    // tracer schováme a metodu ihned ukončíme. Tím zabráníme vnitřnímu volání updatePosition() bez dat.
    if (!m_tracer->graph() || m_tracer->graph()->data()->isEmpty()) {
        m_tracer->setVisible(false);
        m_tracerLabel->setVisible(false);
        return;
    }

    if (!customPlot->axisRect()->rect().contains(event->pos())) {
        m_tracer->setVisible(false);
        m_tracerLabel->setVisible(false);
        customPlot->replot();
        return;
    }

    m_tracer->setVisible(true);
    m_tracerLabel->setVisible(true);

    double x = customPlot->xAxis->pixelToCoord(event->pos().x());
    m_tracer->setGraphKey(x);

    // updatePosition() se u interpolovaného traceru provádí interně při zjišťování value(),
    // ale protože jsme nahoře ověřili, že data existují, proběhne to potichu a správně.
    double y = m_tracer->position->value();

    m_tracerLabel->setText(QString("%1 MHz\n%2 dBµV").arg(x / 1e6, 0, 'f', 2).arg(y, 0, 'f', 1));
    customPlot->replot();
}

void MainWindow::updatePlot(double frequency, double level)
{
    double attenuator_dB = ui->PAAttenuaterSpinBox->value();
    double pa_power = pa->getOutputPowerFromDbuv(level, frequency, attenuator_dB);

    ui->qcustomplotWidget_2->graph(1)->addData(frequency, level);
    ui->PAPowerLabel->setText(QString("%1 W").arg(pa_power, 0, 'f', 2));

    m_tracer->setGraphKey(frequency);

    if (m_tracer->graph() && !m_tracer->graph()->data()->isEmpty()) {
        m_tracer->updatePosition();

        double tracerX = m_tracer->position->key();
        double tracerY = m_tracer->position->value();
        m_tracerLabel->setText(QString("%1 MHz\n%2 dBµV").arg(tracerX / 1e6, 0, 'f', 2).arg(tracerY, 0, 'f', 1));
    }
    ui->qcustomplotWidget_2->replot();
    QApplication::processEvents();
}

void MainWindow::updateMultiPlot(double frequency, double level)
{
    if (!m_currentGraph) return; // Pokud nemáme graf, nic neděláme

    double attenuator_dB = ui->PAAttenuaterSpinBox->value();
    double pa_power = pa->getOutputPowerFromDbuv(level, frequency, attenuator_dB);

    // 1. Zápis dat do aktuálního grafu
    m_currentGraph->addData(frequency, level);

    // 2. POJISTKA: Ujistíme se, že tracer sleduje aktuální graf
    // Pokud tracer z nějakého důvodu ukazuje jinam, přenastavíme ho
    if (m_tracer->graph() != m_currentGraph) {
        m_tracer->setGraph(m_currentGraph);
    }

    if (m_tracer->graph() && !m_tracer->graph()->data()->isEmpty()) {

        // 3. Aktualizace pozice traceru
        m_tracer->setGraphKey(frequency);
        m_tracer->updatePosition();

        // 4. Výpis do labelu
        ui->PAPowerLabel->setText(QString("%1 W").arg(pa_power, 0, 'f', 2));

        // Zde bereme hodnotu přímo z pozice traceru na grafu
        double tracerX = m_tracer->position->key();
        double tracerY = m_tracer->position->value();
        m_tracerLabel->setText(QString("%1 MHz\n%2 dBµV").arg(tracerX / 1e6, 0, 'f', 2).arg(tracerY, 0, 'f', 1));
    }
    ui->qcustomplotWidget_2->replot();
    // QApplication::processEvents(); // Pozor: ve vláknech raději nepoužívat, pokud newDataPoint chodí přes signál
}

void MainWindow::updatePlotMeasure(double frequency,
                                   double measuredCurrent,
                                   double actualUg,
                                   double limitImax)
{
    double attenuator_dB = ui->PAAttenuaterSpinBox->value();
    double pa_power = pa->getOutputPowerFromDbuv(actualUg, frequency, attenuator_dB);

    ui->qcustomplotWidget_2->graph(1)->addData(frequency, measuredCurrent);
    ui->qcustomplotWidget_2->graph(2)->addData(frequency, actualUg);
    ui->qcustomplotWidget_2->graph(3)->addData(frequency, limitImax);
    ui->PAPowerLabel->setText(QString("%1 W").arg(pa_power, 0, 'f', 2));

    if (m_tracer->graph() && !m_tracer->graph()->data()->isEmpty()) {

        m_tracer->setGraphKey(frequency);
        m_tracer->updatePosition();

        double tracerX = m_tracer->position->key();
        double tracerY = m_tracer->position->value();
        m_tracerLabel->setText(QString("%1 MHz\n%2 dBµV").arg(tracerX / 1e6, 0, 'f', 2).arg(tracerY, 0, 'f', 1));
    }
    ui->qcustomplotWidget_2->replot();
    QApplication::processEvents();
}

void MainWindow::addSpectrumChunk(const QVector<double>& freqs, const QVector<double>& levels)
{

    if (!m_currentGraph) return; // Pokud nemáme graf, nic neděláme

    // 1. Zápis dat do aktuálního grafu
    m_currentGraph->addData(freqs, levels);

    // 2. POJISTKA: Ujistíme se, že tracer sleduje aktuální graf
    // Pokud tracer z nějakého důvodu ukazuje jinam, přenastavíme ho
    if (m_tracer->graph() != m_currentGraph) {
        m_tracer->setGraph(m_currentGraph);
    }

    // 3. Aktualizace pozice traceru

    if (m_tracer->graph() && !m_tracer->graph()->data()->isEmpty()) {
        //m_tracer->setGraphKey(freqs);
        m_tracer->updatePosition();

        // Zde bereme hodnotu přímo z pozice traceru na grafu
        double tracerX = m_tracer->position->key();
        double tracerY = m_tracer->position->value();
        m_tracerLabel->setText(QString("%1 MHz\n%2 dBµV").arg(tracerX / 1e6, 0, 'f', 2).arg(tracerY, 0, 'f', 1));
    }
    ui->qcustomplotWidget->replot();
}
