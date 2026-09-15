/*
 * ntc.c
 *
 *  Created on: Sep 1, 2026
 *      Author: SAMEER
 */

#include "ntc.h"
#include "math.h"

float NTC_ReadTemperature(ADC_HandleTypeDef *hadc){

		HAL_ADC_Start(hadc);
		HAL_ADC_PollForConversion(hadc, 100);
		uint32_t adcRaw = HAL_ADC_GetValue(hadc);

		float vAdc = (adcRaw/4095.0f)*3.3f;
		float rNtc =  NTC_FIXED_R * (vAdc/(3.3f-vAdc));

		float tempK = 1.0f / ( (1.0f/NTC_T0) + (1.0f/NTC_BETA) * logf(rNtc/NTC_R0) );
		float tempC = tempK - 273.15f;

		return tempC;
}
