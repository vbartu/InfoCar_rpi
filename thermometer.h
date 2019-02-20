
#ifndef THERMOMETER_H_
#define THERMOMETER_H_

// thermometer device I2C address
#define T_AD 0x5f

// configuration registers and values
#define REG1_AD 0x20
#define REG1_CONF 0x85 // 1000 0101
#define REG2_AD 0x21
#define REG2_CONF 0x00 // 0000 0000

// output registers
#define HUM_LOW_AD 0x28
#define HUM_HIGH_AD 0x29
#define TEMP_LOW_AD 0x2a
#define TEMP_HIGH_AD 0x2b

// functions prototypes
void setup_thermometer();
int32_t get_temperature();
int32_t get_humidity();

#endif /* THERMOMETER_H_ */
