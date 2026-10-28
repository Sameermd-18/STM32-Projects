#include "ina219.h"
#include <math.h>

#define INA219_CONFIG_VALUE  0x1F77   /* 16V, PGA/8, 64-sample avg on bus+shunt, continuous */

/* Change these to match your I2C1 pins (PB6/PB7 default, PB8/PB9 if remapped) */
#define I2C_SCL_PORT  GPIOB
#define I2C_SCL_PIN   GPIO_PIN_6
#define I2C_SDA_PORT  GPIOB
#define I2C_SDA_PIN   GPIO_PIN_7

volatile uint32_t i2cFailCount;

static void i2c_delay(void) { for (volatile int i = 0; i < 200; i++); }

static HAL_StatusTypeDef ina219_write(I2C_HandleTypeDef *hi2c, uint8_t reg, uint16_t val)
{
    uint8_t d[2] = { (uint8_t)(val >> 8), (uint8_t)(val & 0xFF) };
    return HAL_I2C_Mem_Write(hi2c, INA219_ADDR, reg, I2C_MEMADD_SIZE_8BIT, d, 2, 20);
}

static void ina219_bus_recover(I2C_HandleTypeDef *hi2c)
{
    HAL_I2C_DeInit(hi2c);

    GPIO_InitTypeDef g = {0};
    g.Mode  = GPIO_MODE_OUTPUT_OD;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Pin = I2C_SCL_PIN; HAL_GPIO_Init(I2C_SCL_PORT, &g);
    g.Pin = I2C_SDA_PIN; HAL_GPIO_Init(I2C_SDA_PORT, &g);

    HAL_GPIO_WritePin(I2C_SDA_PORT, I2C_SDA_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(I2C_SCL_PORT, I2C_SCL_PIN, GPIO_PIN_SET);
    i2c_delay();

    /* Clock out up to 9 pulses so a slave holding SDA low lets go */
    for (int i = 0; i < 9; i++) {
        HAL_GPIO_WritePin(I2C_SCL_PORT, I2C_SCL_PIN, GPIO_PIN_RESET); i2c_delay();
        HAL_GPIO_WritePin(I2C_SCL_PORT, I2C_SCL_PIN, GPIO_PIN_SET);   i2c_delay();
    }

    /* Manual STOP */
    HAL_GPIO_WritePin(I2C_SDA_PORT, I2C_SDA_PIN, GPIO_PIN_RESET); i2c_delay();
    HAL_GPIO_WritePin(I2C_SCL_PORT, I2C_SCL_PIN, GPIO_PIN_SET);   i2c_delay();
    HAL_GPIO_WritePin(I2C_SDA_PORT, I2C_SDA_PIN, GPIO_PIN_SET);   i2c_delay();

    HAL_I2C_Init(hi2c);        /* MspInit puts the pins back to AF open-drain */
    INA219_Init(hi2c);         /* chip may have reset, so reprogram it */
}

static HAL_StatusTypeDef ina219_read(I2C_HandleTypeDef *hi2c, uint8_t reg, uint16_t *out)
{
    uint8_t d[2];
    if (HAL_I2C_Mem_Read(hi2c, INA219_ADDR, reg, I2C_MEMADD_SIZE_8BIT, d, 2, 20) != HAL_OK) {
        i2cFailCount++;
        ina219_bus_recover(hi2c);
        return HAL_ERROR;
    }
    *out = ((uint16_t)d[0] << 8) | d[1];
    return HAL_OK;
}

HAL_StatusTypeDef INA219_Init(I2C_HandleTypeDef *hi2c)
{
    HAL_StatusTypeDef st = ina219_write(hi2c, REG_CONFIG, INA219_CONFIG_VALUE);
    if (st != HAL_OK) return st;
    return ina219_write(hi2c, REG_CALIBRATION, INA219_CAL_VALUE);
}

/* Call ~once per second: re-programs the chip if calibration got lost */
void INA219_Verify(I2C_HandleTypeDef *hi2c)
{
    uint16_t cal;
    if (ina219_read(hi2c, REG_CALIBRATION, &cal) == HAL_OK && cal != INA219_CAL_VALUE)
        INA219_Init(hi2c);
}

float INA219_ReadVoltage(I2C_HandleTypeDef *hi2c)
{
    uint16_t raw;
    if (ina219_read(hi2c, REG_BUSVOLTAGE, &raw) != HAL_OK) return NAN;
    return (raw >> 3) * 0.004f;
}

float INA219_ReadCurrent(I2C_HandleTypeDef *hi2c)
{
    uint16_t raw;
    if (ina219_read(hi2c, REG_CURRENT, &raw) != HAL_OK) return NAN;
    return (int16_t)raw * INA219_CURRENT_LSB;
}

float INA219_ReadPower(I2C_HandleTypeDef *hi2c)
{
    uint16_t raw;
    if (ina219_read(hi2c, REG_POWER, &raw) != HAL_OK) return NAN;
    return raw * INA219_POWER_LSB;
}
