/*
 * ntc.h
 *
 *  Created on: Sep 1, 2026
 *      Author: SAMEER
 */

#ifndef INC_NTC_H_
#define INC_NTC_H_

#include "main.h"



#define NTC_FIXED_R 	10000.0f
#define NTC_R0			10000.0f
#define NTC_BETA		3950.0f
#define	NTC_T0			298.15f			//25°C in Kelvin

float NTC_ReadTemperature(ADC_HandleTypeDef *hadc);


#endif /* INC_NTC_H_ */
