
#ifndef __I2C_INTERFACE_H__
#define __I2C_INTERFACE_H__

#include <stddef.h>


typedef enum
{
	I2C_START_COND  = 0x1000,
	I2C_STOP_COND   = 0x2000,
	I2C_CONTINUE    = 0x4000
} I2C_COND_T;


typedef int (*PFN_I2C_SEND_T)(int iCond, unsigned int uiAckPoll, unsigned int uiDataLength, const unsigned char *pucData);
typedef int (*PFN_I2C_RECV_T)(int iCond, unsigned int uiAckPoll, unsigned int uiDataLength, unsigned char *pucData);
typedef int (*PFN_I2C_SET_DEVICE_SPECIFIC_SPEED_T)(unsigned long ulDeviceSpecificValue);

typedef struct
{
	PFN_I2C_SEND_T fnSend;
	PFN_I2C_RECV_T fnRecv;
	PFN_I2C_SET_DEVICE_SPECIFIC_SPEED_T fnSetDeviceSpecificSpeed;
} I2C_FUNCTIONS_T;


#endif  /* __I2C_INTERFACE_H__ */
