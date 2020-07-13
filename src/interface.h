#include <stdint.h>


#ifndef __INTERFACE_H__
#define __INTERFACE_H__


typedef enum I2C_CMD_ENUM
{
	I2C_CMD_Open = 0,
	I2C_CMD_RunSequence = 1,
	I2C_CMD_Close = 2
} I2C_CMD_T;



typedef enum I2C_SEQ_COMMAND_ENUM
{
	I2C_SEQ_COMMAND_Read = 0,
	I2C_SEQ_COMMAND_Write = 1,
	I2C_SEQ_COMMAND_Delay = 2
} I2C_SEQ_COMMAND_T;



typedef enum I2C_SEQ_CONDITION_ENUM
{
	I2C_SEQ_CONDITION_None = 0,
	I2C_SEQ_CONDITION_Start = 1,
	I2C_SEQ_CONDITION_Stop = 2,
	I2C_SEQ_CONDITION_Continue = 4
} I2C_SEQ_CONDITION_T;



typedef struct I2C_PARAMETER_OPEN_STRUCT
{
	uint32_t ptHandle;
	uint16_t usI2CCore;
	uint8_t ucMMIOIndexSCL;
	uint8_t ucMMIOIndexSDA;
	uint16_t usPortcontrolSCL;
	uint16_t usPortcontrolSDA;
} I2C_PARAMETER_OPEN_T;



typedef struct I2C_PARAMETER_RUN_SEQUENCE_STRUCT
{
	uint32_t ptHandle;
	const uint8_t *pucCommand;
	uint32_t sizCommand;
	uint8_t *pucReceivedData;
	uint32_t sizReceivedDataMax;
	uint32_t sizReceivedData;
} I2C_PARAMETER_RUN_SEQUENCE_T;



typedef struct I2C_PARAMETER_STRUCT
{
	uint32_t ulVerbose;
	uint32_t ulCommand;
	union {
		I2C_PARAMETER_OPEN_T tOpen;
		I2C_PARAMETER_RUN_SEQUENCE_T tRunSequence;
	} uParameter;
} I2C_PARAMETER_T;


typedef enum TEST_RESULT_ENUM
{
	TEST_RESULT_OK = 0,
	TEST_RESULT_ERROR = 1
} TEST_RESULT_T;


#endif  /* __INTERFACE_H__ */
