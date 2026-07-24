#ifdef __cplusplus
extern "C" {
#endif

#ifndef __PROJECT_CONFIG_H__
#define __PROJECT_CONFIG_H__


#define CONFIG_NUM_SENSORS 4   // change for 4 when ready!!!!!!!!!!!!

// Sensor0
#define CONFIG_HOST_DR0_PIN  29   //Hardware pin of DR0 line
#define CONFIG_HOST_SDA0_PIN  27   //Hardware pin of SDA0 line
#define CONFIG_HOST_SCL0_PIN  28   //Hardware pin of SCL0 line

// Sensor1
#define CONFIG_HOST_DR1_PIN  26   //Hardware pin of DR1 line
#define CONFIG_HOST_SDA1_PIN  0   //Hardware pin of SDA1 line
#define CONFIG_HOST_SCL1_PIN  22   //Hardware pin of SCL1 line



#define CONFIG_HOST_SDA2_PIN 3  //check these
#define CONFIG_HOST_SCL2_PIN 2  //check this
#define CONFIG_HOST_DR2_PIN  1  //check this

#define CONFIG_HOST_SDA3_PIN 6  //check this
#define CONFIG_HOST_SCL3_PIN 5  //check this
#define CONFIG_HOST_DR3_PIN  4  //checky thi



// Project Specific Header
#define CONFIG_HARDWARE_REV     4
#define CONFIG_FIRMWARE_REV     0
#define CONFIG_PROJECT_REV      1
#define CONFIG_PROJECT_SUB_REV  1
#define CONFIG_PROJECT_NAME "Gen6DevKit"

#define PROJECT_MAX_PACKET_SIZE 53
// #define PROJECT_I2C_FREQUENCY 100000
#define PROJECT_I2C_FREQUENCY 400000

#endif // __PROJECT_CONFIG_H__

#ifdef __cplusplus
}
#endif
