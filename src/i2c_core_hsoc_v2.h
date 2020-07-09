#include "i2c_interface.h"


#ifndef __I2C_CORE_HSOC_V2_H__
#define __I2C_CORE_HSOC_V2_H__


typedef enum I2C_SETUP_CORE_ENUM
{
	I2C_SETUP_CORE_RAPI2C0    = 0,
	I2C_SETUP_CORE_RAPI2C1    = 1,
	I2C_SETUP_CORE_RAPI2C2    = 2,
	I2C_SETUP_CORE_RAPI2C3    = 3,
	I2C_SETUP_CORE_RAPI2C4    = 4,
	I2C_SETUP_CORE_RAPI2C5    = 5,
	I2C_SETUP_CORE_I2C0       = 6,
	I2C_SETUP_CORE_I2C1       = 7,
	I2C_SETUP_CORE_I2C2       = 8
} I2C_SETUP_CORE_T;



/* This enum defines the order of the pins in the aucMmioIndex and
 * ausPortControl arrays of the I2C_SETUP_T structure.
 */
typedef enum I2C_SETUP_PIN_INDEX_ENUM
{
	I2C_SETUP_PIN_INDEX_SCL = 0,
	I2C_SETUP_PIN_INDEX_SDA = 1
} I2C_SETUP_PIN_INDEX_T;



typedef struct I2C_SETUP_STRUCT
{
	I2C_SETUP_CORE_T tI2CCore;
	unsigned char aucMmioIndex[2];
	unsigned short ausPortControl[2];
} I2C_SETUP_T;


const I2C_FUNCTIONS_T *i2c_core_hsoc_v2_init(I2C_SETUP_T *ptI2CSetup);

#endif  /* __I2C_CORE_HSOC_V2_H__ */

