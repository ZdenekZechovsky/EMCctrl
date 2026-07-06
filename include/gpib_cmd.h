#ifndef GPIB_CMD_H
#define GPIB_CMD_H

#ifdef __cplusplus
extern "C" {
#endif

	int gpib_error(int Device, const char* msg);
	int gpib_read_binary_block(int ud, float* data);
	int gpib_write(int ud, const char* cmd);
    int gpib_init(tEMC_measurement *pEMC);

#ifdef __cplusplus
}
#endif

#endif
