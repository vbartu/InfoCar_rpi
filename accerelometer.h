#ifndef ACCERELOMETER_H_
#define ACCERELOMETER_H_

#include <stdint.h>


// thermometer device I2C address
#define IMU_AD 0x6a

// configuration registers and values
#define REG1_AD 0x10
#define REG1_CONF 0xa0  // 1010 0000
#define REG2_AD 0x11
#define REG2_CONF 0x00	// 0000 0000
#define REG3_AD 0x12
#define REG3_CONF 0x00  // 0000 0000

// values addresses
#define XLOW_AD 0x28
#define XHIGH_AD 0x29
#define YLOW_AD 0x2a
#define YHIGH_AD 0x2b
#define ZLOW_AD 0x2c
#define ZHIGH_AD 0x2d

#define ANG_X_LOW_AD 0x18
#define ANG_X_HIGH_AD 0x19
#define ANG_Y_LOW_AD 0x1a
#define ANG_Y_HIGH_AD 0x1b
#define ANG_Z_LOW_AD 0x1c
#define ANG_Z_HIGH_AD 0x1d

// functions prototypes
void setupIMU();
int32_t get_x_acceleration();
int32_t get_y_acceleration();
int32_t get_z_acceleration();

int32_t get_x_gyro();
int32_t get_y_gyro();
int32_t get_z_gyro();


#endif /* ACCERELOMETER_H_ */
