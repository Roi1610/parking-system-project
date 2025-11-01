#ifndef I2C_MASTER_H
#define I2C_MASTER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <arpa/inet.h>

#define I2C_BUS        "/dev/i2c-2"           /**< Path to I2C bus device */
#define I2C_SLAVE_ADDR 0x08                   /**< STM32 slave I2C address */
#define LOG_FILE       "/home/debian/embedded/i2c_master.log"       /**< Log file path */

#define GPIO_NUM       49                     /**< GPIO number connected to STM32 data-ready pin */
#define GPIO_BASE_PATH "/sys/class/gpio"      /**< Path for GPIO */

#define PIPE_PATH "/tmp/i2c_pipe"             /**< Named pipe for IPC */

/**
 * @file i2c_master
 * @brief I2C master application running on BeagleBone that reads frames
 *        from an STM32 slave device. GPIO is used as interrupt signal
 *        to trigger I2C read only when new data is available.
 */




 /**
 * @brief Swap bytes in a 16-bit value.
 * @param v 16-bit value
 * @return Byte-swapped 16-bit value
 */
static inline uint16_t swap16(uint16_t v){ 
    return (v>>8)|(v<<8);
 }

/**
 * @brief Swap bytes in a 32-bit float.
 * @param b Pointer to 4 bytes representing float in little-endian
 * @return Float in host endianness
 */
static inline float swap_float(const uint8_t *b){
    union { float f; uint8_t b[4]; } u;
    u.b[0]=b[3]; u.b[1]=b[2]; u.b[2]=b[1]; u.b[3]=b[0]; return u.f;
}


/**
 * @brief Append a message to the log file with timestamp.
 * @param msg Message string to log
 */
void log_message(const char *message);



/**
 * @brief Initialize a GPIO pin for input and rising edge detection.
 * @param gpio_num GPIO number
 * @return File descriptor of GPIO value file, or -1 on error
 */
int gpio_init(int gpio_num);



/**
 * @brief Read a data frame from I2C slave.
 * @param fd File descriptor of I2C bus
 * @param device_id Pointer to store device ID
 * @param cord_x Pointer to store X coordinate
 * @param cord_y Pointer to store Y coordinate
 * @param status Pointer to store status
 * @return 0 on success, -1 on failure
 */
int i2c_read_frame(int fd,uint16_t *device_id,float *cord_x,float *cord_y,uint16_t *status);


/**
 * @brief handle signals
 */
void handle_signal(int signal);



#endif