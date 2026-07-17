#include "EmcMeasurementManager.h"
#include "StatusBarManager.h"
#include "GpibDevice.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QThread>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTimer>
#include <QDir>
#include <QSettings>

#ifdef __linux__
#include <gpib/ib.h>
#define GPIB_IBSTA ibsta
#define GPIB_IBERR iberr
#define GPIB_IBCNT ibcnt
#define GPIB_IBCNTL ibcntl
#else
#include <windows.h>
#include <ni4882.h>
#define GPIB_IBSTA Ibsta()
#define GPIB_IBERR Iberr()
#define GPIB_IBCNT Ibcnt()
#define GPIB_IBCNTL Ibcntl()
#endif

constexpr double PRIJ_MAX = 120.0;  // Max allowed receiver voltage (dBuV)
constexpr double GEN_MAX = 126.0;   // Max generator voltage

const char* smt_init_cmd[] = {
    "*RST",                 // OK: Reset
    "ROSC:SOUR EXT",        // Používá externí referenční oscilátor
    "POW 0DBUV",            // OK: Výkon
    "UNIT:POW DBUV",        // OK: Jednotka
    "AM:SOUR EXT",         // OK: Externí vstup 1
    "AM:EXT:COUP DC",       // DOPLNĚNO: Pro obdélník (ON/OFF) je nutné DC vazba,
    // aby se přenášely úrovně (při AC by se signál "vystředil")
    "AM:DEPT 100PCT",       // OK: Hloubka 100 %
};

const char* esi_init_cmd[] = {
    "*RST",                         // Nastaví přístroj do definovaného výchozího stavu [cite: 247]
    "*CLS",                         // Vymaže stavové registry a vyrovnávací paměť výstupu [cite: 223]
    "ROSC:SOUR INT",                // Používá interní referenční oscilátor
    "DISP:PSAV OFF",                // Vypne šetřič obrazovky (Power Save)
    "INST REC",                     // Přepne přístroj do režimu přijímače (Receiver Mode) [cite: 1548, 1815]
    "INIT:CONT OFF",                // Nastaví režim měření na Single Scan (vypne kontinuální měření) [cite: 1455]
    "CORR:TRAN:SEL 'CSP8465'",      // Vybere korekční faktor s názvem 'CSP8465' [cite: 1727]
    "CORR:TRAN ON",                 // Zapne vybraný korekční faktor (transducer) [cite: 1728]
    "DISP:FORM SPL",                // Přepne zobrazení na rozdělenou obrazovku (Split Screen) [cite: 1166]
    "SENS:DET:REC:FUNC POS",        // Vybere špičkový detektor (Peak) pro měření v režimu přijímače [cite: 1734]
    "INP:TYPE INPUT2",              // Vybere vstupní konektor INPUT 2 [cite: 1541]
    "SWE:TIME 200 ms",              // Nastaví dobu měření (Sweep Time) na 200 ms [cite: 1761]
    "BAND:RES 1000 Hz",               // Nastaví šířku mezifrekvenčního filtru (IF BW) na 1 kHz [cite: 1722]
    "INP:ATT:AUTO ON",              // Zapne automatické nastavení vstupního útlumu [cite: 1488]
    "INP:GAIN:STAT OFF",            // Vypne vestavěný předzesilovač [cite: 1532]
    "INP:ATT:PROT ON",              // Zapne ochranu vstupního útlumového článku (vypne možnost 0 dB útlumu) [cite: 1502]
    "CORR:TSET OFF",                // Vypne použití sady transducerů (Transducer Set) [cite: 1729]
    "CORR:TRAN OFF",                // Vypne použití korekčních faktorů [cite: 1728]
    "CALC:UNIT:POW DBUV",           // Nastaví jednotku výkonu na dBuV [cite: 1079]
};

const char* esi_s21_cmd[] = {
    "*RST",                         // Nastaví přístroj do definovaného výchozího stavu [cite: 247]
    "*CLS",                         // Vymaže stavové registry a vyrovnávací paměť výstupu [cite: 223]
    "ROSC:SOUR INT",                // Používá interní referenční oscilátor
    "DISP:PSAV OFF",                // Vypne šetřič obrazovky (Power Save)
    "INST REC",                     // Přepne přístroj do režimu přijímače (Receiver Mode) [cite: 1548, 1815]
    "INIT:CONT OFF",                // Nastaví režim měření na Single Scan (vypne kontinuální měření) [cite: 1455]
    "DISP:FORM SPL",                // Přepne zobrazení na rozdělenou obrazovku (Split Screen) [cite: 1166]
    "SENS:DET:REC:FUNC POS",        // Vybere špičkový detektor (Peak) pro měření v režimu přijímače [cite: 1734]
    "INP:TYPE INPUT1",              // Vybere vstupní konektor INPUT 2 [cite: 1541]
    "SWE:TIME 200 ms",              // Nastaví dobu měření (Sweep Time) na 200 ms [cite: 1761]
    "BAND:RES 10 kHz",               // Nastaví šířku mezifrekvenčního filtru (IF BW) na 1 kHz [cite: 1722]
    "INP:ATT:AUTO ON",              // Zapne automatické nastavení vstupního útlumu [cite: 1488]
    "INP:GAIN:STAT OFF",            // Vypne vestavěný předzesilovač [cite: 1532]
    "INP:ATT:PROT OFF",              // Zapne ochranu vstupního útlumového článku (vypne možnost 0 dB útlumu) [cite: 1502]
    "CORR:TSET OFF",                // Vypne použití sady transducerů (Transducer Set) [cite: 1729]
    "CORR:TRAN OFF",                // Vypne použití korekčních faktorů [cite: 1728]
    "CALC:UNIT:POW DBUV",           // Nastaví jednotku výkonu na dBuV [cite: 1079]
};

const char* generator_init_cmd[] = {

"*RST",                 // Reset the instrument to factory default
    "*CLS",                 // Clear status registers and error queue
    "OUTP:LOAD INF",        // Set high impedance (SMT02 input is >100kOhm)
    "FUNC SQU",             // Square wave for ON/OFF keying (fixed from SQUR to SQU)
    "FREQ 1000",            // Set frequency to 1 kHz
    "VOLT 2.0",             // Set 2.0 Vpp (ensures 1.0 V peak for SMT02)
    "VOLT:OFFS 0",          // Symmetrical waveform +/- 1V
    "OUTP OFF"               // Disable RF output

};

const char* esi_measure_init_cmd[] = {
    "*RST",                         // Nastaví přístroj do výchozího stavu [cite: 247]
    "*CLS",                         // Vymaže stavové registry [cite: 223]
    "ROSC:SOUR INT",                // Používá interní referenční oscilátor
    "DISP:PSAV OFF",                // Vypne šetřič obrazovky
    "INST REC",                     // Přepne do režimu přijímače [cite: 1548]
    "INIT:CONT OFF",                // Nastaví režim měření na Single Scan [cite: 1455]
    "CORR:TRAN:SEL 'CSP8465'",      // Vybere korekční faktor 'CSP8465' [cite: 1727]
    "CORR:TRAN ON",                 // Aktivuje vybraný korekční faktor [cite: 1728]
    "DISP:FORM SPL",                // Přepne na rozdělenou obrazovku [cite: 1166]
    "SENS:DET:REC:FUNC POS",        // Nastaví špičkový detektor [cite: 1734]
    "INP:TYPE INPUT2",              // Vybere vstup INPUT 2 [cite: 1541]
    "SWE:TIME 200 ms",              // Nastaví dobu měření na 200 ms [cite: 1761]
    "BAND:RES 1 kHz",               // Nastaví šířku IF filtru na 1 kHz [cite: 1722]
    "INP:ATT:AUTO ON",              // Zapne automatický útlum [cite: 1488]
    "INP:GAIN:STAT OFF",            // Vypne předzesilovač [cite: 1532]
    "INP:ATT:PROT ON",              // Aktivuje ochranu útlumového článku [cite: 1502]
    "CORR:TSET OFF",                // Vypne sady transducerů [cite: 1729]
    "CORR:TRAN ON",                 // Zapne korekční faktory pro měření [cite: 1728]
    //"CORR:TRAN OFF",                 // Zapne korekční faktory pro měření [cite: 1728]
    "CALC:UNIT:POW DBUA",           // Nastaví jednotku výkonu na dBuV [cite: 1079]
    //"CALC:UNIT:POW DBUV",           // Nastaví jednotku výkonu na dBuV [cite: 1079]
};

const tTransducerFactor csp8465_tdf[] = {
    {10000,     27.0},
    {100000,    8.0},
    {200000,    3.5},
    {300000,    2.0},
    {400000,    1.0},
    {700000,    1.0},
    {2000000,   0.5},
    {500000000, 2.5}
};

extern tESI_SCAN_RANGES ranges[4];

EmcMeasurementManager::EmcMeasurementManager(GpibDevice* device, QObject *parent)
    : QObject(parent), hwDevice(device)
{
    // Propagate hardware progress to the UI
    connect(hwDevice, &GpibDevice::hardwareProgress, this, &EmcMeasurementManager::measurementProgress);
    connect(hwDevice, &GpibDevice::hardwareProgressCE, this, &EmcMeasurementManager::measurementProgressCE);
    connect(hwDevice, &GpibDevice::spectrumChunkReady, this, &EmcMeasurementManager::spectrumChunkReady);
    connect(hwDevice, &GpibDevice::spectrumChunkReadyCE, this, &EmcMeasurementManager::spectrumChunkReadyCE);
}

int EmcMeasurementManager::runSpectrumRead(unsigned char esiAddr,
                                           double fstart,
                                           double fstop,                                           
                                           const std::shared_ptr<std::atomic<bool>>& stop,
                                           const std::shared_ptr<std::atomic<bool>>& pause,
                                           int data_chunk_time) {
    int udESI = hwDevice->getEsiDescriptor();
    if (udESI == 0) return 0;

    // Clear vectors before a new measurement
    spectrum.clear();
    frequencies.clear();

    int stepDurationMs = data_chunk_time * 1000;
    int totalSize = 0;

    emit spectrummeasurementStarted();

    for (auto& range : ranges) {

        // 1. Convert range limits from MHz to Hz
        double rangeStart_Hz = range.Start_MHz * 1000000.0;
        double rangeStop_Hz = range.Stop_MHz * 1000000.0;

        // 2. Compare in Hz (using qMax/qMin to avoid C2589 error)
        double currentStart = qMax(fstart, rangeStart_Hz);
        double currentStop = qMin(fstop, rangeStop_Hz);

        // Skip segment if ranges do not overlap
        if (currentStart >= currentStop) {
            continue;
        }

        // Calculate sweep parameter
        double stepParam = 1000.0 * range.Step_size_kHz * stepDurationMs / range.Meas_Time_ms;

        // Read data - currentStart and currentStop are now correctly in Hz
        int segmentSize = hwDevice->sweepEsiBlocked(udESI,
                                                    currentStart,
                                                    currentStop,
                                                    stepParam,
                                                    rawdata,                                                    
                                                    stop,
                                                    pause,
                                                    fstart,
                                                    fstop);
        if (*stop) break;

        if (segmentSize <= 1) {
            qDebug() << "Warning: No valid data read from the analyzer for range"
                     << currentStart << "-" << currentStop << "Hz (size =" << segmentSize << ").";
            continue;
        }

        // Calculate X-axis step in Hz
        double fstep = (currentStop - currentStart) / (segmentSize - 1);
        float *p = (float *)rawdata;

        // Append read points to result vectors
        for (int j = 0; j < segmentSize; j++) {
            double freq = currentStart + j * fstep; // Frequency is in Hz

            // Prevent duplicate points at range boundaries
            if (!frequencies.empty() && freq <= frequencies.back()) {
                continue;
            }

            frequencies.push_back(freq);
            spectrum.push_back(p[j]);
            totalSize++;
        }
    }

    ibonl(udESI, 0); // Release handle

    if (totalSize == 0) {
        qDebug() << "Error: No data read from the analyzer overall.";
        return 0;
    }

    return totalSize;
}

double EmcMeasurementManager::calibratorRegulator(int udESI,
                                                  int udSMT,
                                                  double& Ug,
                                                  double targetVoltage,
                                                  const std::shared_ptr<std::atomic<bool>>& stop,
                                                  const std::shared_ptr<std::atomic<bool>>& pause) {
    double measuredVoltage = 0.0;
    double deltaUg = 0.0;
    const double kReg = 0.2;     // Proportional constant
    const int tRegMs = 500;      // Settling time in ms
    const double allowedDev = 0.2; // Allowed deviation

    do {
        while (*pause && !(*stop)) {
            QThread::msleep(100);
        }

        Ug += deltaUg;
        if (Ug > GEN_MAX || *stop) {
            hwDevice->setSmtLevel(udSMT, 0.0);
            return -100.0;
        }

        hwDevice->setSmtLevel(udSMT, Ug);
        QThread::msleep(tRegMs);

        measuredVoltage = hwDevice->readEsiSingleVoltage(udESI);
        if (measuredVoltage > PRIJ_MAX || *stop) {
            hwDevice->setSmtLevel(udSMT, 0.0);
            return -100.0;
        }

        deltaUg = kReg * (targetVoltage - measuredVoltage);

    } while (std::abs(targetVoltage - measuredVoltage) > allowedDev);

    return measuredVoltage;
}

int EmcMeasurementManager::runEsiCalibration(tEMC_measurement* pEMC,
                                             const QVector<double>& freqs,
                                             const QString& filePath,
                                             const std::shared_ptr<std::atomic<bool>>& stop,
                                             const std::shared_ptr<std::atomic<bool>>& pause) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Cannot open file for writing!";
        return -1;
    }

    QTextStream out(&file);
    // Header matched to the 3-column output
    out << "Frequency [Hz];GenLevel [V/dBm];MeasuredLevel [dBuV]\n";

    int udESI = ibdev(0, pEMC->ESI_addr, 0, T10s, 1, 0);
    int udSMT = ibdev(0, pEMC->SMT_addr, 0, T10s, 1, 0);
    int udGEN = ibdev(0, pEMC->GEN_addr, 0, T10s, 1, 0);

    for (const char* cmd : esi_init_cmd) hwDevice->writeCommand(udESI, cmd);
    for (const char* cmd : smt_init_cmd) hwDevice->writeCommand(udSMT, cmd);
    for (const char* cmd : generator_init_cmd) hwDevice->writeCommand(udGEN, cmd);

    hwDevice->writeCommand(udSMT, "AM:STAT OFF");  // OK: Aktivace AM
    hwDevice->writeCommand(udSMT, "OUTPUT:STATE ON");   // Zapne RF výstup generátoru
    hwDevice->writeCommand(udGEN, "OUTP OFF");     // Enable RF output

    hwDevice->setSmtLevel(udSMT, 10.0);

    double measuredVoltage = 0.0;
    double Ug = 0.0;

    for (double currentFreq : freqs) {
        hwDevice->setInstrumentsFrequency(udESI, udSMT, currentFreq);

        measuredVoltage = calibratorRegulator(udESI, udSMT, Ug, CS114_limit(currentFreq), stop, pause);

        if (measuredVoltage <= -90.0) break; // Stopped by user or error occurred

        out << currentFreq << ";" << Ug << ";" << measuredVoltage << "\n";
        out.flush();
        emit newDataPoint(currentFreq, Ug);
    }

    hwDevice->setSmtLevel(udSMT, 0.0);
    file.close();

    ibonl(udGEN, 0);
    ibonl(udESI, 0);
    ibonl(udSMT, 0);
    return 0;
}

bool EmcMeasurementManager::loadCsvData(const QString& filePath, QVector<double>& outFrequencies, QVector<double>& outLevels) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    outFrequencies.clear();
    outLevels.clear();
    QTextStream in(&file);
    bool isFirstLine = true;

    while (!in.atEnd()) {
        QString line = in.readLine();
        if (isFirstLine) { isFirstLine = false; continue; }

        QStringList fields = line.split(';');
        if (fields.size() >= 2) {
            bool okFreq, okLevel;
            double f = fields[0].toDouble(&okFreq);
            double l = fields[1].toDouble(&okLevel);
            if (okFreq && okLevel) {
                outFrequencies.append(f);
                outLevels.append(l);
            }
        }
    }
    return !outFrequencies.isEmpty();
}

double EmcMeasurementManager::testRegulator(int udESI,
                                            int udSMT,
                                            double& actualUg,      // Actual generator voltage
                                            double maxUg,          // Max allowed voltage (calibrated)
                                            double limitCurrent,   // Target current (CS114 limit + 6dB)
                                            const std::shared_ptr<std::atomic<bool>>& stop,
                                            const std::shared_ptr<std::atomic<bool>>& pause) {
    double measuredCurrent = 0.0;
    double deltaUg = 0.0;
    const double kReg = 0.2;     // Proportional constant
    const int tRegMs = 500;      // Settling time in ms
    const double allowedDev = 0.3; // Requested tolerance 0.3 dB

    do {
        while (*pause && !(*stop)) {
            QThread::msleep(100);
        }
        if (*stop) return -100.0;

        actualUg += deltaUg;

        // Clamp the voltage to the calibrated limit
        bool isClamped = false;
        if (actualUg >= maxUg) {
            actualUg = maxUg;
            isClamped = true;
        }

        hwDevice->setSmtLevel(udSMT, actualUg);
        QThread::msleep(tRegMs);

        measuredCurrent = hwDevice->readEsiSingleVoltage(udESI);

        if (measuredCurrent > PRIJ_MAX || *stop) {
            hwDevice->setSmtLevel(udSMT, 0.0);
            return -100.0;
        }

        // Calculate deviation for the next step
        deltaUg = kReg * (limitCurrent - measuredCurrent);

        // Break condition 1: We reached the target current within the 0.3 dB tolerance
        if (std::abs(limitCurrent - measuredCurrent) <= allowedDev) {
            break;
        }

        // Break condition 2: We hit the calibrated generator voltage limit,
        // but the current is still below the limit (deltaUg is positive).
        // We cannot increase the voltage further.
        if (isClamped && deltaUg > 0) {
            break;
        }

    } while (true);

    return measuredCurrent;
}

int findClosestIndex(const QVector<double>& vec, double value)
{
    if (vec.isEmpty())
        return -1;

    auto it = std::lower_bound(vec.begin(), vec.end(), value);

    if (it == vec.begin())
        return 0;

    if (it == vec.end())
        return vec.size() - 1;

    int idx = std::distance(vec.begin(), it);

    // porovnej sousedy
    double lower = vec[idx - 1];
    double upper = vec[idx];

    if (std::abs(value - lower) <= std::abs(upper - value))
        return idx - 1;
    else
        return idx;
}

int EmcMeasurementManager::runEsiTest(tEMC_measurement* pEMC,
                                      const QString& calFilePath,
                                      const QString& resultFilePath,                                      
                                      const std::shared_ptr<std::atomic<bool>>& stop,
                                      const std::shared_ptr<std::atomic<bool>>& pause) {
    QVector<double> calFreqs;
    QVector<double> calLevels;

    // Load calibration data
    if (!loadCsvData(calFilePath, calFreqs, calLevels)) {
        qDebug() << "Failed to load calibration data!";
        return -1;
    }

    QFile file(resultFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Cannot open result file for writing!";
        return -1;
    }

    QTextStream out(&file);

    out << "Frequency [Hz];Calibrated level [dBuV];Measured current [dBuA];actualUg [dBuV];limitImax [dBuA]\n";

    int udESI = ibdev(0, pEMC->ESI_addr, 0, T10s, 1, 0);
    int udSMT = ibdev(0, pEMC->SMT_addr, 0, T10s, 1, 0);
    int udGEN = ibdev(0, pEMC->GEN_addr, 0, T10s, 1, 0);

    for (const char* cmd : esi_measure_init_cmd) hwDevice->writeCommand(udESI, cmd);
    for (const char* cmd : smt_init_cmd) hwDevice->writeCommand(udSMT, cmd);
    for (const char* cmd : generator_init_cmd) hwDevice->writeCommand(udGEN, cmd);

    if(pEMC->AMenable)
    {
        hwDevice->writeCommand(udSMT, "AM:STAT ON");        // OK: Aktivace AM;
        hwDevice->writeCommand(udSMT, "OUTPUT:STATE ON");   // Zapne RF výstup generátoru
        hwDevice->writeCommand(udGEN, "OUTP ON");           // Enable RF output
    }
    else
    {
        hwDevice->writeCommand(udSMT, "AM:STAT OFF");  // OK: Aktivace AM
        hwDevice->writeCommand(udSMT, "OUTPUT:STATE ON");   // Zapne RF výstup generátoru
        hwDevice->writeCommand(udGEN, "OUTP OFF");     // Enable RF output
    }

    // Keep actualUg outside the loop to ramp up smoothly between frequencies (like in calibration)
    double actualUg = 0.0;    

    for (int i = 0; i < calFreqs.size(); ++i) {
        if (*stop) break;

        double currentFreq = calFreqs[i];
        double maxUg = calLevels[i]; // The voltage limit established during calibration
        double measuredCurrent;

        // Filter frequencies for partial EUT test
        if (currentFreq < pEMC->fstart) continue;
        if (currentFreq > pEMC->fstop) break; // Assuming ascending order

        /*
        * LIMITNÍ PODMÍNKA PRO I_max (MIL-STD-461G CS114):
        * * Proč se testovací proud limituje na +6 dB nad kalibrační hodnotou?
        * Kalibrace probíhá ve 100Ohm smyčce, která omezuje proud přesně o 6 dB
        * vůči 50Ohm budicímu systému. Při testu je impedance EUT kabelu neznámá.
        * Povolení limitu +6 dB zaručuje, že při nízké impedanci kabelu vzroste
        * proud na fyzikální maximum (odpovídající reálné hrozbě vyzařovaného pole),
        * ale nedojde k nereálnému a destruktivnímu přetížení zařízení.
        * * PRAVIDLO MĚŘENÍ:
        * Zvyšuj výkon signálu, dokud nenastane jedna z podmínek (co nastane dříve):
        * 1. Je dosažen kalibrovaný dopředný výkon (Forward Power).
        * 2. Měřený proud dosáhne úrovně I_max (Kalibrační proud + 6 dB).
        * -> Pokud je dosažen I_max dříve, zastav zvyšování výkonu a měř na této úrovni.
        */
        double limitImax = CS114_limit(currentFreq) + 6.0;

        // Safetylimit due to impedance change
        if (i > 0) {
            actualUg -= pEMC->safetyDropdB;
        }

        // Ensure the starting voltage for this step doesn't exceed the calibrated max
        if (actualUg > maxUg) {
            actualUg = maxUg;
        }

        hwDevice->setInstrumentsFrequency(udESI, udSMT, currentFreq);

        // Regulate towards limitImax, bounded by maxUg
        measuredCurrent = testRegulator(udESI, udSMT, actualUg, maxUg, limitImax, stop, pause);

        if (measuredCurrent <= -90.0) break; // Error or Stop condition triggered

        QString currentFreqStr = QString::number(currentFreq, 'f', 4).replace('.', ',');
        QString maxUgStr = QString::number(maxUg, 'f', 4).replace('.', ',');
        QString measuredCurrentStr = QString::number(measuredCurrent, 'f', 4).replace('.', ',');
        QString actualUgStr = QString::number(actualUg, 'f', 4).replace('.', ',');
        QString limitImaxStr = QString::number(limitImax, 'f', 4).replace('.', ',');

        //out << "Frequency [Hz];Calibrated level [dBuV];Measured current [dBuA];actualUg [dBuV];limitImax [dBuA]\n";
        out << currentFreqStr << ";"
            << maxUgStr << ";"
            << measuredCurrentStr << ";"
            << actualUgStr << ";"
            << limitImaxStr << "\n";
        out.flush();

        emit newDataPointMeasure(currentFreq,
                                 maxUg,
                                 measuredCurrent,
                                 actualUg,
                                 limitImax);
        /* The standard requires you to dwell at each tuned frequency for greater than 3 seconds
        * or for the length of the EUT's maximum response
        */
        QThread::msleep(3000);
    }

    hwDevice->setSmtLevel(udSMT, 0.0);
    file.close();

    ibonl(udGEN, 0);
    ibonl(udESI, 0);
    ibonl(udSMT, 0);
    return 0;
}

double getGainForFrequency(double freqHz) {
    // Definice tabulky (statická, aby se neinicializovala při každém volání)
    static const std::vector<tTransducerFactor> tdf = {
        {10000,     27.0},
        {100000,    8.0},
        {200000,    3.5},
        {300000,    2.0},
        {400000,    1.0},
        {700000,    1.0},
        {2000000,   0.5},
        {500000000, 2.5}
    };

    // Ošetření mezních stavů (Clamping)
    if (freqHz <= tdf.front().frequency) return tdf.front().factor_dB;
    if (freqHz >= tdf.back().frequency) return tdf.back().factor_dB;

    // Nalezení intervalu, do kterého frekvence spadá
    for (size_t i = 0; i < tdf.size() - 1; ++i) {
        if (freqHz >= tdf[i].frequency && freqHz <= tdf[i+1].frequency) {

            // Logaritmická interpolace frekvence (vhodnější pro Hz)
            double logF1 = std::log10(tdf[i].frequency);
            double logF2 = std::log10(tdf[i+1].frequency);
            double logF  = std::log10(freqHz);

            // Výpočet váhy (t) mezi body 0.0 a 1.0
            double t = (logF - logF1) / (logF2 - logF1);

            // Lineární interpolace zisku v dB
            return tdf[i].factor_dB + t * (tdf[i+1].factor_dB - tdf[i].factor_dB);
        }
    }

    return 0.0; // Teoreticky nedosažitelné díky ošetření mezí
}

int EmcMeasurementManager::runS21Measure(tEMC_measurement* pEMC,
                                             const QVector<double>& freqs,
                                             const QString& filePath,
                                             const std::shared_ptr<std::atomic<bool>>& stop,
                                             const std::shared_ptr<std::atomic<bool>>& pause,
                                             bool corr_enable) {

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Cannot open file for writing!";
        return -1;
    }

    QTextStream out(&file);


    int udESI = ibdev(0, pEMC->ESI_addr, 0, T10s, 1, 0);
    int udSMT = ibdev(0, pEMC->SMT_addr, 0, T10s, 1, 0);
#if 0
    int udATT = ibdev(0, pEMC->ATT_addr, 0, T10s, 1, 0);
#endif

    for (const char* cmd : esi_s21_cmd) hwDevice->writeCommand(udESI, cmd);
    for (const char* cmd : smt_init_cmd) hwDevice->writeCommand(udSMT, cmd);

    if(pEMC->Curentenable){
        hwDevice->writeCommand(udESI, "CORR:TRAN ON");
        hwDevice->writeCommand(udESI, "CALC:UNIT:POW DBUA");           // Nastaví jednotku výkonu na dBµV [cite: 1079]
        // Header matched to the 2-column output
        out << "Frequency [MHz];Relative measured level [dBµA]\n";
    }
    else
    {
        hwDevice->writeCommand(udESI, "CORR:TRAN OFF");
        hwDevice->writeCommand(udESI, "CALC:UNIT:POW DBUV");           // Nastaví jednotku výkonu na dBµV [cite: 1079]
        // Header matched to the 2-column output
        out << "Frequency [MHz];Relative measured level [dBµV]\n";
    }

    hwDevice->writeCommand(udSMT, "AM:STAT OFF");  // OK: Aktivace AM
    if(corr_enable) {
        hwDevice->writeCommand(udSMT, "CORRection:STATe ON");
    }
    else {
        hwDevice->writeCommand(udSMT, "CORRection:STATe OFF");
    }

    hwDevice->writeCommand(udSMT, "OUTPUT:STATE ON");   // Zapne RF výstup generátoru

    hwDevice->setSmtLevel(udSMT, pEMC->sweepVoltagedBuV);

    for(int cnt = 0; cnt < 1; cnt++)
    {
#if 0
        hwDevice->setAttenutor(udATT, cnt * 1);
#endif

        emit measurementStarted();
        QThread::msleep(500);        

        for (double currentFreq : freqs) {

            while (*pause && !(*stop)) {
                QThread::msleep(100);
            }
            if (*stop) break;

            hwDevice->setInstrumentsFrequency(udESI, udSMT, currentFreq);

            QThread::msleep(100);
            double measuredVoltage;
            if(pEMC->Curentenable){
                measuredVoltage = hwDevice->readEsiSingleVoltage(udESI) - (pEMC->sweepVoltagedBuV - 34);// + getGainForFrequency(currentFreq);
            }
            else {
                measuredVoltage = hwDevice->readEsiSingleVoltage(udESI)- pEMC->sweepVoltagedBuV;
            }

            out << currentFreq << ";" << measuredVoltage << "\n";
            out.flush();
            emit newMultiDataPoint(currentFreq, measuredVoltage);
        }
        while (*pause && !(*stop)) {
            QThread::msleep(100);
        }
        if (*stop) break;
    }

    hwDevice->setSmtLevel(udSMT, 0.0);
    file.close();

    ibonl(udESI, 0);
    ibonl(udSMT, 0);
#if 0
    ibonl(udATT, 0);
#endif
    return 0;
}

int EmcMeasurementManager::SmtUserCorrection(tEMC_measurement* pEMC, const QString& corFilePath, int inc_attenuator)
{
    QFile file(corFilePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCritical() << "Chyba: Nepodařilo se otevřít soubor" << corFilePath;
        return -1;
    }

    QTextStream in(&file);
    QStringList freqList;
    QStringList powList;

    bool isFirstLine = true;
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        // Přeskočení hlavičky CSV
        if (isFirstLine && line.contains("Frequency", Qt::CaseInsensitive)) {
            isFirstLine = false;
            continue;
        }

        // CSV je oddělené středníkem
        QStringList parts = line.split(';');
        if (parts.size() >= 2) {
            bool okFreq, okLevel;
            // QString::toDouble() spolehlivě zkonvertuje i formát "1.03035e+06"
            double freqHz = parts[0].toDouble(&okFreq);
            double measLevel = parts[1].toDouble(&okLevel);

            if (okFreq && okLevel) {
                // INVERZE ZNAMÉNKA: Pokud trasa zesiluje (kladné dB), musíme generátor stáhnout (záporná korekce)
                double correctionDb = -measLevel - inc_attenuator;

                // Uložení bodů. Frekvenci zapíšeme jako celé číslo (Hz), korekci na 3 desetinná místa.
                freqList.append(QString::number(freqHz, 'f', 0));
                powList.append(QString::number(correctionDb, 'f', 3));
            }
        }
    }
    file.close();

    if (freqList.isEmpty()) {
        qCritical() << "Chyba: Nebyla načtena žádná platná data z CSV!";
        return -1;
    }
    qInfo() << "Úspěšně načteno" << freqList.size() << "bodů z CSV.";

    int udSMT = ibdev(0, pEMC->SMT_addr, 0, T100s, 1, 0);

    // SMT-02 očekává data oddělená čárkou. Poskládáme list do jednoho velkého stringu.
    QString freqString = freqList.join(",");
    QString powString = powList.join(",");
    QString cmdFreq = "CORRection:CSET:DATA:FREQuency " + freqString + "\n";
    QString cmdPow  = "CORRection:CSET:DATA:POWer " + powString + "\n";

    // Výběr tabulky UCOR0 (vymaže předchozí nebo založí novou)
    hwDevice->writeCommand(udSMT, "CORRection:CSET:SELect 'UCOR0'");

    // 2. Převod QString -> QByteArray -> const char* a samotné odeslání do přístroje
    hwDevice->writeCommand(udSMT, cmdFreq.toUtf8().constData());
    hwDevice->writeCommand(udSMT, cmdPow.toUtf8().constData());

    // Aktivace korekční křivky
    //hwDevice->writeCommand(udSMT, "CORRection:STATe ON");

    ibonl(udSMT, 0);

    StatusBarManager::instance().showMessage(QString("Correction successfully loaded, %1 points").arg(freqList.size()));

    return 0;
}
