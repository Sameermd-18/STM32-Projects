/*
 * ina219.c
 *
 *  Created on: Aug 31, 2026
 *      Author: SAMEER
 */

#include "ina219.h"

void INA219_Init(I2C_HandleTypeDef *hi2c){
	uint8_t data[2];
	data[0] = (INA219_CAL_VALUE >> 8) & 0xFF;  //MSB
	data[1] = INA219_CAL_VALUE & 0XFF;

	HAL_I2C_Mem_Write(hi2c, INA219_ADDR,REG_CALIBRATION,I2C_MEMADD_SIZE_8BIT,data,2,100);

}

float INA219_ReadVoltage(I2C_HandleTypeDef *hi2c){
	uint8_t data[2];
	HAL_I2C_Mem_Read(hi2c, INA219_ADDR,REG_BUSVOLTAGE,I2C_MEMADD_SIZE_8BIT,data,2,100);

	uint16_t raw = (data[0]<<8) | data[1];
	raw = raw >> 3; 			//removing status bits
	return raw * 0.004f;		//Bus Voltage Register has 4 mV per bit
}

float INA219_ReadCurrent(I2C_HandleTypeDef *hi2c){
		uint8_t data[2];
		HAL_I2C_Mem_Read(hi2c,INA219_ADDR,REG_CURRENT,I2C_MEMADD_SIZE_8BIT,data,2,100);

		int16_t raw = (int16_t)((data[0]<<8) | data[1]); 	//Type casting to Signed : Current can be Negative
		return raw * INA219_CURRENT_LSB;
}

float INA219_ReadPower(I2C_HandleTypeDef *hi2c){
	uint8_t data[2];
	HAL_I2C_Mem_Read(hi2c,INA219_ADDR,REG_POWER,I2C_MEMADD_SIZE_8BIT,data,2,100);
	uint16_t raw = (data[0]<<8) | data[1];
	return raw * INA219_POWER_LSB;			// scale using Power_LSB (2mW/bit)


}

















