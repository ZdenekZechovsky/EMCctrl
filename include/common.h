#ifndef COMMON_H
#define COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sEMC_measurement
{
	double fstart;
	double fstop;
	unsigned char SMT_addr;
	unsigned char ESI_addr;
	unsigned char ATT_addr;
}tEMC_measurement;

#ifdef __cplusplus
}
#endif

#endif //COMMON_H

