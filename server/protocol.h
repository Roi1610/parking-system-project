#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define START 1
#define AND 0

/**
 * @brief struct that contains the information
 * that is passed to UUT
 */
typedef struct __attribute__((packed)) {
    uint16_t device_id;
    float cord_x;
    float cord_y;
    uint16_t status;
} gps_frame;

#endif // PROTOCOL_H