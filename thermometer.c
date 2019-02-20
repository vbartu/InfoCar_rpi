
#include <stdint.h>
#include <wiringPiI2C.h>

#include "thermometer.h"

static int fd;

// thermometer configuration
void setup_thermometer() {
	fd = wiringPiI2CSetup(T_AD);

	wiringPiI2CWriteReg8(fd, REG1_AD, REG1_CONF);
	wiringPiI2CWriteReg8(fd, REG2_AD, REG2_CONF);
}

int32_t get_temperature() {
	uint8_t tempLow = wiringPiI2CReadReg8 (fd, TEMP_LOW_AD); //least significant bit
	uint8_t tempHigh = wiringPiI2CReadReg8 (fd, TEMP_HIGH_AD);  //most significant bit

	int16_t temperature = ((int16_t)tempHigh << 8) | tempLow; //concatenation

	return temperature;
}

int32_t get_humidity() {
	uint8_t humLow = wiringPiI2CReadReg8 (fd, HUM_LOW_AD); //least significant bit
	uint8_t humHigh = wiringPiI2CReadReg8 (fd, HUM_HIGH_AD); //most significant bit

	int16_t humidity = ((int16_t)humHigh << 8) | humLow; //concatenation

	return humidity;
}

