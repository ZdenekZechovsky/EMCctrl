#ifndef GPIB_DEVICE_H
#define GPIB_DEVICE_H

#include <atomic>
#include <QObject>

// Structure holding GPIB addresses for connected devices
struct tEMC_measurement {
    double fstart;
    double fstop;
    bool AMenable;
    bool Curentenable;
    double safetyDropdB;
    double sweepVoltagedBuV;
    unsigned char ATT_addr;
    unsigned char GEN_addr;
    unsigned char SMT_addr;
    unsigned char ESI_addr;
    unsigned char PWR_addr;
};

// Scan range structure for the ESI receiver
struct tESI_SCAN_RANGES {
    double Start_MHz;
    double Stop_MHz;
    double Step_size_kHz;
    unsigned int Res_BW_kHz;
    unsigned int Meas_Time_ms;
    unsigned char Auto_Ranging;
    unsigned char RF_Attn_dB;
    unsigned char Preamp;
    unsigned char Auto_Preamp;
    unsigned char Input;
};

extern tESI_SCAN_RANGES ranges[4];

class GpibDevice : public QObject
{
    Q_OBJECT
public:
    explicit GpibDevice(QObject *parent = nullptr);
    ~GpibDevice() = default;

    // Connection lifecycle
    int init(tEMC_measurement* pEMC);
    int close(tEMC_measurement* pEMC);
    int checkError(int deviceDescriptor, const char* msg);

    // Basic I/O operations
    int writeCommand(int deviceDescriptor, const char* cmd);
    int readData(int deviceDescriptor, char* buffer, int maxLen);

    // Instrument specific commands
    int setRfStepAttenuator(unsigned char gpibAddr, unsigned char attenuate);
    int setGenerator(unsigned char gpibAddr, double freq, double volt, double offset, int function);
    int setPowerSupply(unsigned char gpibAddr, double p6V, double p25V, double m25V,
                       double p6I, double p25I, double m25I);
    int setDisplay(unsigned char gpibAddr, int num);
    int getPowerVoltage(unsigned char gpibAddr, double* p6V, double* p25V, double* m25V);
    int getPowerCurrent(unsigned char gpibAddr, double* p6I, double* p25I, double* m25I);
    int enablePowerSupply(unsigned char gpibAddr, int on);

    // ESI Analyzer configuration and basic readout
    double initSpectrumRead(unsigned char gpibAddr, double fstart, double fstop, int ord, int data_chunk_time);
    double initSpectrumReadCE102(unsigned char gpibAddr, double fstart, double fstop, int data_chunk_time);

    int sweepEsiBlocked(int ud,
                        double fstart,
                        double fstop,
                        double fpart,
                        unsigned char* p,                        
                        const std::shared_ptr<std::atomic<bool>>& stop,
                        const std::shared_ptr<std::atomic<bool>>& pause,
                        double fstart_orig,
                        double fstop_orig);
    double readEsiSingleVoltage(int udESI);
    
    // Helpers for measurement loops
    void setInstrumentsFrequency(int udESI, int udSMT, double freq);
    void setSmtLevel(int udSMT, double level);
    void setAttenutor(int udESI, quint8 attenuate);

    // Getters for specific instrument descriptors (if needed)
    int getEsiDescriptor() const { return esiDescriptor; }
    int getSmtDescriptor() const { return smtDescriptor; }

signals:
    void hardwareProgress(int percent);
    void hardwareProgressCE(int percent);
    void spectrumChunkReady(const QVector<double>& freqs, const QVector<double>& levels);
    void spectrumChunkReadyCE(const QVector<double>& freqs, const QVector<double>& levels);

private:
    int esiDescriptor = 0;
    int smtDescriptor = 0;
};

#endif // GPIB_DEVICE_H
