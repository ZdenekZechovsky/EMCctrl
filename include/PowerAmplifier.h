#ifndef POWER_AMPLIFIER_H
#define POWER_AMPLIFIER_H

#include <QObject>
#include "ftdibitbang.h"

namespace PaConfig {
/* Power Amplifier limits and constants */
constexpr double MAX_OUTPUT_POWER = 250.0; // W
constexpr double MAX_INPUT_POWER     = 10.0;  // dBm
constexpr double NOMINAL_INPUT_POWER = -5.0;  // dBm

/* Status Bits (quint8 odpovídá přesně 1 bytu / unsigned char z FTDI) */
constexpr quint8 STATUS_AMPLIFIER_ON      = 0x08;
constexpr quint8 STATUS_SUMMARY_ALARM     = 0x10;
constexpr quint8 STATUS_REMOTE            = 0x20;
constexpr quint8 STATUS_HIGH_VOLTAGE_ON   = 0x40;
constexpr quint8 STATUS_STANDBY           = 0x80;

/* Command Bits */
constexpr quint8 COMMAND_REMOTE               = 0x01;
constexpr quint8 COMMAND_HIGH_VOLTAGE_ON_OFF  = 0x02;
constexpr quint8 COMMAND_AMPLIFIER_ON_OFF     = 0x04;

/* Složená maska (constexpr umí počítat i logické operace už při překladu!) */
constexpr quint8 OUTPUT_SET = (COMMAND_AMPLIFIER_ON_OFF |
                               COMMAND_HIGH_VOLTAGE_ON_OFF |
                               COMMAND_REMOTE);
}

// Structure for the gain table
struct tPowerAmplifierGain {
    double frequency;
    double gain;
};

// Extern declaration of the gain table
extern const tPowerAmplifierGain pa_gain[];

class PowerAmplifier : public QObject
{
    Q_OBJECT
public:
    // Konstruktor nyní vyžaduje ukazatel na komunikační rozhraní FTDI
    explicit PowerAmplifier(FtdiBitBang *ftdi, QObject *parent = nullptr);
    ~PowerAmplifier() = default;

    // --- RF výpočty (nepotřebují HW komunikaci) ---
    double getGain(double frequency, const tPowerAmplifierGain *table, size_t size);
    double getGainLog(double frequency, const tPowerAmplifierGain *table, size_t size);

    double vrmsToDbm(double Vrms, double R);
    double dbmToVrms(double dBm, double R);

    double getOutputPowerFromDbuv(double input_dBuV, double frequency, double attenuator_dB);
    double getOutputDbuv(double input_dBuV, double frequency, double attenuator_dB);

    // --- HW komunikace (používají interní m_ftdi) ---
    int initDevice();
    int getStatus();
    int setStatus(unsigned char status);

private:
    FtdiBitBang *m_ftdi; // Ukazatel na hardwarovou vrstvu
};

#endif // POWER_AMPLIFIER_H
