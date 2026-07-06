#ifndef GPIB_H
#define GPIB_H

#include <QObject>

#define MAX_DEV 32

typedef struct sEMC_measurement
{
    double fstart;
    double fstop;
    unsigned char ATT_addr;
    unsigned char GEN_addr;
    unsigned char SMT_addr;
    unsigned char ESI_addr;
    unsigned char PWR_addr;
}tEMC_measurement;

typedef struct sESI_SCAN_RANGES
{
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
}tESI_SCAN_RANGES;

double CS114_limit(double f);

extern tESI_SCAN_RANGES ranges[3];

class GpibDevice : public QObject
{
    Q_OBJECT
public:

    explicit GpibDevice(QObject *parent = nullptr);                            // konstruktor
    ~GpibDevice() = default;                             // destruktor (není povinný, ale doporučený)

    int init(tEMC_measurement* pEMC);   // inicializace GPIB přístrojů
    int close(tEMC_measurement * pEMC);
    int error(int Device, const char* msg); // kontrola chyby
    int set_rf_step_attenuator(unsigned char gpib_addr, unsigned char attenuate);
    double read_spectrum_init(unsigned char gpib_addr, double fstart, double fstop, int ord);
    int read_spectrum_run(unsigned char gpib_addr, double fstart, double fstop);
    int sweep_ESI26_blocked(int ud, double fstart, double fstop, double fpart, unsigned char* p);
    int set_gen(unsigned char gpib_addr, double freq, double volt, double offset);
    int set_pwr(unsigned char gpib_addr, double p6V, double p25V, double m25V,
                double p6I, double p25I, double m25I);
    int get_pwr_voltage(unsigned char gpib_addr, double *pvoltage_p6, double *pvoltage_p25, double *pvoltage_m25);
    int get_pwr_current(unsigned char gpib_addr, double *pcurrent_p6, double *pcurrent_p25, double *pcurrent_m25);
    int esi_read(tEMC_measurement* pEMC, QVector<double> myFrequencies, QString filePath);
    bool loadCsvData(const QString &filePath, QVector<double> &frequencies, QVector<double> &levels);
    // Přístup k datům pro jiné třídy
    const QVector<double>& getSpectrum() const { return spectrum; }
    const QVector<double>& getFrequencies() const { return frequencies; }

    // Getter pro modifikaci – vrací odkaz, přes který můžeš clear()
    QVector<double>& spectrumRef() { return spectrum; }
    QVector<double>& frequenciesRef() { return frequencies; }

signals:
    void progress(int value);
    void newDataPoint(double frequency, double voltage);
private:
    QVector<double> spectrum;    // naměřené hodnoty Y
    QVector<double> frequencies; // osa X
};

#endif // GPIB_H
