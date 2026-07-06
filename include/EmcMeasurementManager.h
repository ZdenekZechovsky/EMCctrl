#ifndef EMC_MEASUREMENT_MANAGER_H
#define EMC_MEASUREMENT_MANAGER_H
#include <atomic>
#include <QObject>
#include <QVector>
#include <QString>
#include "GpibDevice.h"

// Structure for the gain table
struct tTransducerFactor {
    double frequency;
    double factor_dB;
};

class GpibDevice;

class EmcMeasurementManager : public QObject
{
    Q_OBJECT
public:
    explicit EmcMeasurementManager(GpibDevice* device, QObject *parent = nullptr);
    ~EmcMeasurementManager() = default;

    // Measurement processes
    int runEsiCalibration(tEMC_measurement* pEMC,
                          const QVector<double>& freqs,
                          const QString& filePath,
                          const std::shared_ptr<std::atomic<bool>>& stop,
                          const std::shared_ptr<std::atomic<bool>>& pause);
    int runEsiTest(tEMC_measurement* pEMC,
                   const QString& calFilePath,
                   const QString& resultFilePath,                   
                   const std::shared_ptr<std::atomic<bool>>& stop,
                   const std::shared_ptr<std::atomic<bool>>& pause);
    int runSpectrumRead(unsigned char esiAddr,
                        double fstart,
                        double fstop,                        
                        const std::shared_ptr<std::atomic<bool>>& stop,
                        const std::shared_ptr<std::atomic<bool>>& pause,
                        int data_chunk_time);
    int runS21Measure(tEMC_measurement* pEMC,
                      const QVector<double>& freqs,
                      const QString& filePath,
                      const std::shared_ptr<std::atomic<bool>>& stop,
                      const std::shared_ptr<std::atomic<bool>>& pause,
                      bool corr_enable);
    int SmtUserCorrection(tEMC_measurement* pEMC, const QString& corFilePath, int inc_attenuator);

    // Data management
    bool loadCsvData(const QString& filePath, QVector<double>& outFrequencies, QVector<double>& outLevels);
    
    // Accessors for UI
    const QVector<double>& getSpectrum() const { return spectrum; }
    const QVector<double>& getFrequencies() const { return frequencies; }

signals:
    void measurementProgress(int value);
    void measurementProgressCE(int value);
    void newDataPoint(double frequency, double voltage);
    void measurementStarted();
    void spectrummeasurementStarted();
    void newMultiDataPoint(double frequency, double voltage);
    void newDataPointMeasure(double frequency,
                             double measuredCurrent,
                             double actualUg,
                             double limitImax);
    void spectrumChunkReady(const QVector<double>& freqs, const QVector<double>& levels);
    void spectrumChunkReadyCE(const QVector<double>& freqs, const QVector<double>& levels);

private:
    double calibratorRegulator(int udESI,
                               int udSMT,
                               double& Ug, // Změněno na referenci
                               double targetVoltage,
                               const std::shared_ptr<std::atomic<bool>>& stop,
                               const std::shared_ptr<std::atomic<bool>>& pause);

    double testRegulator(int udESI,
                         int udSMT,
                         double& actualUg,      // Actual generator voltage
                         double maxUg,          // Max allowed voltage (calibrated)
                         double limitCurrent,   // Target current (CS114 limit + 6dB)
                         const std::shared_ptr<std::atomic<bool>>& stop,
                         const std::shared_ptr<std::atomic<bool>>& pause);


    GpibDevice* hwDevice;
    QVector<double> spectrum;
    QVector<double> frequencies;
    
    unsigned char rawdata[8 * 65536]; // Buffer for sweep
};

// External helper (declare here if not in rf_limits.h)
double CS114_limit(double f);

#endif // EMC_MEASUREMENT_MANAGER_H
