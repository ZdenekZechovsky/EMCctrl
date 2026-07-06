#include "PowerAmplifier.h"
#include <cmath>
#include <QDebug>

const tPowerAmplifierGain pa_gain[] = {
    {9000,       57.4},
    {30000,      56.5},
    {100000,     56.7},
    {300000,     56.7},
    {1000000,    56.8},
    {3000000,    57.0},
    {10000000,   57.4},
    {30000000,   55.8},
    {100000000,  56.8},
    {220000000,  58.2}
};

PowerAmplifier::PowerAmplifier(FtdiBitBang *ftdi, QObject *parent)
    : QObject(parent), m_ftdi(ftdi)
{
    // Uložíme si ukazatel, abychom ho nemuseli předávat v každé funkci
}

double PowerAmplifier::getGain(double frequency, const tPowerAmplifierGain *table, size_t size)
{
    if (size == 0) return 0.0;

    // Pod rozsahem
    if (frequency <= table[0].frequency)
        return table[0].gain;

    // Nad rozsahem
    if (frequency >= table[size - 1].frequency)
        return table[size - 1].gain;

    // Lineární interpolace (i když se jmenuje lineární, je pro logaritmické frekvence vhodnější logaritmická)
    for (size_t i = 0; i < size - 1; ++i) {
        if (frequency >= table[i].frequency && frequency <= table[i + 1].frequency) {
            double f1 = table[i].frequency;
            double f2 = table[i + 1].frequency;
            double g1 = table[i].gain;
            double g2 = table[i + 1].gain;
            return g1 + (g2 - g1) * (frequency - f1) / (f2 - f1);
        }
    }
    return 0.0;
}

double PowerAmplifier::getGainLog(double frequency, const tPowerAmplifierGain *table, size_t size)
{
    if (size == 0) return 0.0;

    if (frequency <= table[0].frequency)
        return table[0].gain;

    if (frequency >= table[size - 1].frequency)
        return table[size - 1].gain;

    // Logaritmická interpolace pro RF měření
    for (size_t i = 0; i < size - 1; ++i) {
        if (frequency >= table[i].frequency && frequency <= table[i + 1].frequency) {
            double f1 = table[i].frequency;
            double f2 = table[i + 1].frequency;
            double g1 = table[i].gain;
            double g2 = table[i + 1].gain;
            return g1 + (g2 - g1) * (log10(frequency) - log10(f1)) / (log10(f2) - log10(f1));
        }
    }
    return 0.0;
}

double PowerAmplifier::vrmsToDbm(double Vrms, double R)
{
    double P_W = (Vrms * Vrms) / R;
    double P_mW = P_W * 1000.0;
    return 10.0 * log10(P_mW);
}

double PowerAmplifier::dbmToVrms(double dBm, double R)
{
    double P_mW = pow(10.0, dBm / 10.0);
    double P_W = P_mW / 1000.0;
    return sqrt(P_W * R);
}

double PowerAmplifier::getOutputPowerFromDbuv(double input_dBuV, double frequency, double attenuator_dB)
{
    double gain = getGainLog(frequency, pa_gain, sizeof(pa_gain) / sizeof(pa_gain[0]));
    double out_dBuV = input_dBuV + gain - attenuator_dB;
    double pout_dBm = out_dBuV - 106.98;
    double pout_W = pow(10.0, (pout_dBm - 30.0) / 10.0);

    if (pout_W > PaConfig::MAX_OUTPUT_POWER) {
        pout_W = PaConfig::MAX_OUTPUT_POWER;
    }
    return pout_W;
}

double PowerAmplifier::getOutputDbuv(double input_dBuV, double frequency, double attenuator_dB)
{
    double gain = getGainLog(frequency, pa_gain, sizeof(pa_gain) / sizeof(pa_gain[0]));
    return input_dBuV + gain - attenuator_dB;
}

// --- HW Komunikace s použitím m_ftdi ---

int PowerAmplifier::initDevice()
{
    if (!m_ftdi) return -1; // Kontrola, že ukazatel existuje

    m_ftdi->listDevices();
    if (!m_ftdi->openDeviceBySerial("FTBGLQPW")) {
        return -1;
    }
    m_ftdi->setDirection(PaConfig::OUTPUT_SET);
    return 0;
}

int PowerAmplifier::getStatus()
{
    if (!m_ftdi) return -1;

    quint8 status;
    m_ftdi->readPins(status);
    return status;
}

int PowerAmplifier::setStatus(unsigned char status)
{
    if (!m_ftdi) return -1;
    m_ftdi->writeByte(status);
    return 0;
}
