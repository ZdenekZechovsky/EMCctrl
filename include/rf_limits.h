#ifndef RF_LIMITS_H
#define RF_LIMITS_H

typedef enum {
    RE102_ORIGINAL,
    RE102_HELICOPTER,
    RE102_LARGE_AIRCRAFT
} Platform_t;

double CS114_limit(double f);
double RE102_limit(double f, Platform_t platform);
double CE102_limit(double f);

#endif // RF_LIMITS_H
