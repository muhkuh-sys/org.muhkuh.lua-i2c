
#ifndef __I2C_INTERFACE_H__
#define __I2C_INTERFACE_H__

#include <stddef.h>
#include "netx_io_areas.h"


typedef enum
{
	I2C_START_COND  = 0x1000,
	I2C_STOP_COND   = 0x2000,
	I2C_CONTINUE    = 0x4000
} I2C_COND_T;



struct I2C_HANDLE_STRUCT;

typedef int (*PFN_I2C_SEND_T)(const struct I2C_HANDLE_STRUCT *ptHandle, int iCond, unsigned int uiAckPoll, unsigned int uiDataLength, const unsigned char *pucData);
typedef int (*PFN_I2C_RECV_T)(const struct I2C_HANDLE_STRUCT *ptHandle, int iCond, unsigned int uiAckPoll, unsigned int uiDataLength, unsigned char *pucData);
/* TODO: Add a function to convert a clock speed in kHz to a device specific value. */
typedef int (*PFN_I2C_SET_DEVICE_SPECIFIC_SPEED_T)(const struct I2C_HANDLE_STRUCT *ptHandle, unsigned long ulDeviceSpecificValue);

typedef struct I2C_FUNCTIONS_STRUCT
{
	PFN_I2C_SEND_T fnSend;
	PFN_I2C_RECV_T fnRecv;
	PFN_I2C_SET_DEVICE_SPECIFIC_SPEED_T fnSetDeviceSpecificSpeed;
} I2C_FUNCTIONS_T;

typedef struct I2C_HANDLE_STRUCT
{
	I2C_FUNCTIONS_T tI2CFn;
	HOSTADEF(I2C) * ptI2cUnit;
} I2C_HANDLE_T;

#endif  /* __I2C_INTERFACE_H__ */
