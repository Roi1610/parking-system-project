#ifndef PROTOCOL_H
#define PROTOCOL_H  

#include <stdint.h>

#define START 1
#define AND 0

/**
 * @brief struct that contains the information
 * that is passed to UUT
 * @param device_id - identifies the device
 * @param cord_x - X coordinate (scaled integer)
 * @param cord_y - Y coordinate (scaled integer)
 * @param status - status flag

 */
typedef struct __attribute__((packed)) {
    uint16_t device_id;
    float cord_x;
    float cord_y;
    uint16_t status;
} gps_frame;


#endif