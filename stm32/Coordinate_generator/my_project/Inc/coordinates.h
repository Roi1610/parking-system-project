/*
 * coordinates.h
 *
 *  Created on: Aug 30, 2025
 *      Author: Roi
 */

#ifndef INC_COORDINATES_H_
#define INC_COORDINATES_H_

#define MAX_COORDINATES 10

static const struct{
	float x;
	float y;
} coordinates[MAX_COORDINATES] = {
		{31.962,34.802},      // Rishon Lezion
		{32.087,34.789},      // Tel Aviv
		{31.749,35.170},      // Jerusalem
		{29.549,34.954},      // Eilat
		{31.073,35.044},      // Dimona
		{32.999,35.091},      // Nahariyya
		{33.174,35.574},      // Qiryat Shemona
		{32.422,34.909},      // Hadera
		{31.883,34.794},      // Rehovot
		{31.255,35.166}       // Arad
};

#endif /* INC_COORDINATES_H_ */
