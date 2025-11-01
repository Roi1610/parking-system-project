/*
 * my_main.c
 *
 *  Created on: Aug 30, 2025
 *      Author: Roi
 */

#include <string.h>
#include <stdlib.h>
#include "stm32f7xx_hal.h"
#include "protocol.h"
#include "coordinates.h"
#include "I2C_Coordinate_generator.h"

void my_main(){
	gps_frame data;
	srand(HAL_GetTick());

	while (1){
		FillDataStruct(&data);

		uint8_t data_arry[sizeof(gps_frame)];
		memcpy(data_arry, &data, sizeof(gps_frame));
		SwapEndian(data_arry, sizeof(gps_frame));

		I2C2_Send_Data(data_arry, sizeof(gps_frame));

		int wait_time = RAND_WAITING;
		HAL_Delay(wait_time);

		data.status = AND;
		memcpy(data_arry, &data, sizeof(gps_frame));
		SwapEndian(data_arry, sizeof(gps_frame));

		I2C2_Send_Data(data_arry, sizeof(gps_frame));
		HAL_Delay(30000);
	}
}
