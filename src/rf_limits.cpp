#include <cmath>
#include "rf_limits.h"

double CS114_limit(double f)
{
    if (f < 1e6)
        return 20*log10(f/1000.0) + 29;

    if (f <= 30e6)
        return 89;

    return -9.71*log10(f/1000.0) + 132.47;
}

double RE102_limit(double f, Platform_t platform)
{
    switch (platform) {

    // --- HELIKOPTÉRA ---
    case RE102_HELICOPTER:
        if (f <= 10e3) {
            return 60.0;
        }
        else if (f < 2e6) {
            // Interpolace od 10 kHz (60 dBuV) do 2 MHz (24 dBuV)
            return 60.0 + (24.0 - 60.0) / (log10(2e6) - log10(10e3)) * (log10(f) - log10(10e3));
        }
        else if (f <= 100e6) {
            // Pásmo 2 MHz až 100 MHz je ploché (24 dBuV)
            return 24.0;
        }
        else {
            // Interpolace od 100 MHz (24 dBuV) do 18 GHz (69 dBuV)
            if (f >= 18e9) return 69.0;
            return 24.0 + (69.0 - 24.0) / (log10(18e9) - log10(100e6)) * (log10(f) - log10(100e6));
        }

    // --- VELKÁ LETADLA ---
    case RE102_LARGE_AIRCRAFT:
        // Pro velká letadla (z vašeho zadání: 44 dBuV 2MHz - 100MHz)
        // Uplatníme plošně 44 dBuV na všechno do 100 MHz
        if (f <= 100e6) {
            return 44.0;
        }
        else {
            // Interpolace od 100 MHz (44 dBuV) do 18 GHz (89 dBuV)
            if (f >= 18e9) return 89.0;
            return 44.0 + (89.0 - 44.0) / (log10(18e9) - log10(100e6)) * (log10(f) - log10(100e6));
        }

    // --- VÁŠ PŮVODNÍ KÓD (např. Navy / Ground) ---
    case RE102_ORIGINAL:
    default:
        if (f <= 100e6) {
            return 34.0;
        }
        if (f >= 18e9) return 79.0; // Pojistka pro f > 18 GHz
        return 34.0 + (79.0 - 34.0) / (log10(18e9) - log10(100e6)) * (log10(f) - log10(100e6));
    }
}

double CE102_limit(double f)
{
    // Limit je konstantní pro frekvence od 500 kHz do 10 MHz
    if (f >= 500e3)
        return 60.0;

    // Pro frekvence od 10 kHz do 500 kHz klesá logaritmicky
    // Počáteční hodnota = 94 dBµV, koncová = 60 dBµV
    return 94.0 + (60.0 - 94.0) / (log10(500e3) - log10(10e3)) * (log10(f) - log10(10e3));
}
