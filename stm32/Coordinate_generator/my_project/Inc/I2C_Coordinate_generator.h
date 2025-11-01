/*
 * I2C_Coordinate_generator.h
 *
 *  Created on: Aug 30, 2025
 *      Author: Roi
 */

#ifndef INC_I2C_COORDINATE_GENERATOR_H_
#define INC_I2C_COORDINATE_GENERATOR_H_

#include "protocol.h"

#define RAND_WAITING ((rand() % 5 + 1)*60*1000)
#define DEVICE_ID 1610
#define DATA_READY_GPIO_PORT GPIOE
#define DATA_READY_PIN GPIO_PIN_15


void FillDataStruct(gps_frame *data);

void I2C2_Send_Data(const uint8_t *data, uint8_t length);

void SwapEndian(uint8_t *data, uint8_t length);


#endif /* INC_I2C_COORDINATE_GENERATOR_H_ */
