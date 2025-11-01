/*
 * I2C_Coordinate_generator.c
 *
 *  Created on: Aug 30, 2025
 *      Author: Roi
 */

#include <string.h>
#include <stdio.h>
#include "stm32f7xx_hal.h"
#include "stm32f7xx_hal_i2c.h"
#include "protocol.h"
#include "coordinates.h"
#include "I2C_Coordinate_generator.h"

extern I2C_HandleTypeDef hi2c2;

static int coordinate_index = 0;
static uint8_t txBuffer[sizeof(gps_frame)];

volatile uint8_t data_ready = 0;

void FillDataStruct(gps_frame *data){
	if (coordinate_index >= MAX_COORDINATES){
		coordinate_index = 0;
	}

	data->device_id = DEVICE_ID;
	data->cord_x = coordinates[coordinate_index].x;
	data->cord_y = coordinates[coordinate_index].y;
	data->status = START;

	coordinate_index++;
}


void I2C2_Send_Data(const uint8_t *data, uint8_t length){
	if (data_ready)
		return;
	memcpy(txBuffer, data, length);
	data_ready = 1;

	HAL_GPIO_WritePin(DATA_READY_GPIO_PORT, DATA_READY_PIN, GPIO_PIN_SET);

	if (HAL_I2C_Slave_Transmit(&hi2c2, txBuffer, length, HAL_MAX_DELAY) != HAL_OK) {
		printf("I2C Error: transmit start failed\n");
		HAL_GPIO_WritePin(DATA_READY_GPIO_PORT, DATA_READY_PIN, GPIO_PIN_RESET);
		data_ready = 0;
	}
	HAL_GPIO_WritePin(DATA_READY_GPIO_PORT, DATA_READY_PIN, GPIO_PIN_RESET);
	data_ready = 0;
}


void SwapEndian(uint8_t *data, uint8_t length){
	for(int i = 0; i < length / 2; i++){
		uint8_t temp_data = data[i];
		data[i] = data[length - i - 1];
		data[length - i - 1] = temp_data;
	}
}

//void HAL_I2C_SlaveTxCpltCallback(I2C_HandleTypeDef *hi2c)
//{
//	if (hi2c->Instance == I2C2 && data_ready) {
//		HAL_GPIO_WritePin(DATA_READY_GPIO_PORT, DATA_READY_PIN, GPIO_PIN_RESET);
//		data_ready = 0;
//	}
//}
