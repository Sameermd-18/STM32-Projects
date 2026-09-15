/*
 * ina219.h
 *
 *  Created on: Aug 31, 2026
 *      Author: SAMEER
 */

#ifndef INC_INA219_H_
#define INC_INA219_H_

#include "main.h"

#define INA219_ADDR			(0X40<<1)
#define REG_CONFIG		 	0X00
#define REG_SHUNTVOLTAGE 	0X01
#define REG_BUSVOLTAGE		0X02
#define	REG_POWER			0X03
#define	REG_CURRENT			0X04
#define	REG_CALIBRATION		0X05


#define	INA219_CAL_VALUE		4096
#define	INA219_CURRENT_LSB		0.0001f		//100 uA/bit
#define	INA219_POWER_LSB		0.002f		//2 mW/bit

void INA219_Init(I2C_HandleTypeDef *hi2c);
float INA219_ReadVoltage(I2C_HandleTypeDef *hi2c);
float INA219_ReadCurrent(I2C_HandleTypeDef *hi2c);
float INA219_ReadPower(I2C_HandleTypeDef *hi2c);

#endif /* INC_INA219_H_ */
