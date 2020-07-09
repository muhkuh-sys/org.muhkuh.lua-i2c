#include "i2c_core_hsoc_v2.h"

#include "netx_io_areas.h"
#include "systime.h"
#include "uprintf.h"


typedef enum
{
	I2CCMD_START    = 0,    /* Generate (r)START-condition. */
	I2CCMD_S_AC     = 1,    /* Acknowledge-polling: generate up to acpollmax+1 START-sequences (until acknowledged by slave). */
	I2CCMD_S_AC_T   = 2,    /* Run S_AC, then transfer tsize+1 bytes from/to master FIFO. Not to be continued. */
	I2CCMD_S_AC_TC  = 3,    /* Run S_AC, then transfer tsize+1 bytes from/to master FIFO. To be continued. */
	I2CCMD_CT       = 4,    /* Continued transfer not to be continued. */
	I2CCMD_CTC      = 5,    /* Continued transfer to be continued. */
	I2CCMD_STOP     = 6,    /* Generate STOP-condition. */
	I2CCMD_IDLE     = 7     /* Nothing to do, last command finished, break current command. */
} I2CCMD_T;

typedef enum
{
	I2CSPEED_50     = 0,    /* Fast/Standard-mode, 50kbit/s */
	I2CSPEED_100    = 1,    /* Fast/Standard-mode, 100kbit/s */
	I2CSPEED_200    = 2,    /* Fast/Standard-mode, 200kbit/s */
	I2CSPEED_400    = 3,    /* Fast/Standard-mode, 400kbit/s */
	I2CSPEED_800    = 4,    /* Fast/Standard-mode, 800kbit/s */
	I2CSPEED_1200   = 5,    /* Fast/Standard-mode, 1.2Mbit/s */
	I2CSPEED_1700   = 6,    /* High-speed-mode, 1.7Mbit/s */
	I2CSPEED_3400   = 7     /* High-speed-mode, 3.4Mbit/s */
} I2CSPEED_T;

/*-----------------------------------*/


HOSTADEF(I2C) * ptI2cUnit;


/*-----------------------------------*/

static int i2c_wait_for_command_done(void)
{
	unsigned long ulValue;
	TIMER_HANDLE_T tTimerHandle;
	int iResult;


	iResult = 0;

	/* Wait until the command is finished. */
	systime_handle_start_ms(&tTimerHandle, 1000);
	do
	{
		if( systime_handle_is_elapsed(&tTimerHandle)!=0 )
		{
			iResult = -1;
			break;
		}

		ulValue   = ptI2cUnit->ulI2c_cmd;
		ulValue  &= HOSTMSK(i2c_cmd_cmd);
		ulValue >>= HOSTSRT(i2c_cmd_cmd);
	} while( ulValue!=I2CCMD_IDLE );

	return iResult;
}



static int i2c_core_hsoc_v2_send(int iCond, unsigned int uiAckPoll, unsigned int uiDataLength, const unsigned char *pucData)
{
	unsigned long ulAddress;
	unsigned long ulValue;
	const unsigned char *pucBufferCnt;
	const unsigned char *pucBufferEnd;
	unsigned int uiChunkTransaction;
	int iResult;


	iResult = 0;

	/* Limit ACK poll to valid range. */
	if( uiAckPoll>(HOSTMSK(i2c_cmd_acpollmax)>>HOSTSRT(i2c_cmd_acpollmax)) )
	{
		uiAckPoll = HOSTMSK(i2c_cmd_acpollmax) >> HOSTSRT(i2c_cmd_acpollmax);
	}

	/* Check parameters. */
	/* This core can not send start conditions without data. */
	if( (iCond&I2C_START_COND)!=0 && uiDataLength==0 )
	{
		iResult = -1;
	}

	/* handle start condition separately */
	if( iResult==0 && (iCond&I2C_START_COND)!=0 )
	{
		/* Get the first data byte and make a proper ID. */
		ulAddress   = (unsigned long)(iCond & 0x7f);
		ulAddress <<= HOSTSRT(i2c_mcr_sadr);
		ulAddress  &= HOSTMSK(i2c_mcr_sadr);

		/* First byte of the command is the ID. */
		ulValue  = ptI2cUnit->ulI2c_mcr;
		ulValue &= ~HOSTMSK(i2c_mcr_sadr);
		ulValue |= ulAddress;
		ptI2cUnit->ulI2c_mcr = ulValue;

		/* Execute start condition in write mode. */
		ulValue  = 0 << HOSTSRT(i2c_cmd_nwr);
		ulValue |= I2CCMD_S_AC << HOSTSRT(i2c_cmd_cmd);
		ulValue |= uiAckPoll << HOSTSRT(i2c_cmd_acpollmax);
		ptI2cUnit->ulI2c_cmd = ulValue;

		iResult = i2c_wait_for_command_done();
		if( iResult!=0 )
		{
			uprintf("Failed to execute the start command.\n");
		}
		else
		{
			/* Was the start condition acknowledged? */
			ulValue  = ptI2cUnit->ulI2c_sr;
			ulValue &= HOSTMSK(i2c_sr_last_ac);
			if( ulValue==0 )
			{
				/* No ACK received. */
				uprintf("No ACK received.\n");
				iResult = -1;
			}
		}
	}

	if( iResult==0 && uiDataLength!=0 )
	{
		/* Send data. */
		pucBufferCnt = pucData;

		ptI2cUnit->ulI2c_mdr = *(pucBufferCnt++);

		uiChunkTransaction = uiDataLength;
		if( uiChunkTransaction>((HOSTMSK(i2c_cmd_tsize)>>HOSTSRT(i2c_cmd_tsize))+1U) )
		{
			uiChunkTransaction = (HOSTMSK(i2c_cmd_tsize)>>HOSTSRT(i2c_cmd_tsize)) + 1U;
		}

		/* Execute transfer. */
		ulValue  = 0 << HOSTSRT(i2c_cmd_nwr);
		if( (iCond&I2C_CONTINUE)==0 )
		{
			/* Do not continue this operation. */
			ulValue |= I2CCMD_CT << HOSTSRT(i2c_cmd_cmd);
		}
		else
		{
			/* Continue this operation with another write command. */
			ulValue |= I2CCMD_CTC << HOSTSRT(i2c_cmd_cmd);
		}
		ulValue |= (uiChunkTransaction-1U) << HOSTSRT(i2c_cmd_tsize);
		ulValue |= 0 << HOSTSRT(i2c_cmd_acpollmax);
		ptI2cUnit->ulI2c_cmd = ulValue;

		/* Receive the transaction data. */
		pucBufferEnd = pucData + uiChunkTransaction;
		while( pucBufferCnt<pucBufferEnd )
		{
			ulValue  = ptI2cUnit->ulI2c_sr;
			ulValue &= HOSTMSK(i2c_sr_mfifo_full);
			if( ulValue==0 )
			{
				ptI2cUnit->ulI2c_mdr = *(pucBufferCnt++);
			}
		}

		uiDataLength -= uiChunkTransaction;

		iResult = i2c_wait_for_command_done();
		if( iResult!=0 )
		{
			uprintf("Failed to execute the start command.\n");
		}
		else
		{
			/* Was the start condition acknowledged? */
			ulValue  = ptI2cUnit->ulI2c_sr;
			ulValue &= HOSTMSK(i2c_sr_last_ac);
			if( ulValue==0 )
			{
				/* No ACK received. */
				uprintf("No ACK received.\n");
				iResult = -1;
			}
		}
	}

	/* Send a stop condition? */
	if( iResult==0 && (iCond&I2C_STOP_COND)!=0 )
	{
		/* Execute stop condition. */
		ulValue  = 1 << HOSTSRT(i2c_cmd_nwr);
		ulValue |= I2CCMD_STOP << HOSTSRT(i2c_cmd_cmd);
		ulValue |= 0 << HOSTSRT(i2c_cmd_tsize);
		ulValue |= 0 << HOSTSRT(i2c_cmd_acpollmax);
		ptI2cUnit->ulI2c_cmd = ulValue;

		iResult = i2c_wait_for_command_done();
		if( iResult!=0 )
		{
			uprintf("Failed to execute the start command.\n");
		}
	}

	return iResult;
}


static int i2c_core_hsoc_v2_recv(int iCond, unsigned int uiAckPoll, unsigned int uiDataLength, unsigned char *pucData)
{
	int iResult;
	unsigned long ulAddress;
	unsigned long ulValue;
	unsigned long ulChunkTransaction;
	unsigned long ulChunkFifo;


	iResult = 0;

	/* Limit ACK poll to valid range. */
	if( uiAckPoll>(HOSTMSK(i2c_cmd_acpollmax)>>HOSTSRT(i2c_cmd_acpollmax)) )
	{
		uiAckPoll = HOSTMSK(i2c_cmd_acpollmax) >> HOSTSRT(i2c_cmd_acpollmax);
	}

	/* Check parameters. */
	/* This core can not send a start condition without data. */
	if( (iCond&I2C_START_COND)!=0 && uiDataLength==0 )
	{
		iResult = -1;
	}
	else
	{
		if( (iCond&I2C_START_COND)!=0 )
		{
			/* Get the first data byte and make a proper ID. */
			ulAddress   = (unsigned long)(iCond & 0x7f);
			ulAddress <<= HOSTSRT(i2c_mcr_sadr);
			ulAddress  &= HOSTMSK(i2c_mcr_sadr);

			/* First byte of the command is the ID. */
			ulValue  = ptI2cUnit->ulI2c_mcr;
			ulValue &= ~HOSTMSK(i2c_mcr_sadr);
			ulValue |= ulAddress;
			ptI2cUnit->ulI2c_mcr = ulValue;

			/* Execute start condition in write mode. */
			ulValue  = 1 << HOSTSRT(i2c_cmd_nwr);
			ulValue |= I2CCMD_S_AC << HOSTSRT(i2c_cmd_cmd);
			ulValue |= uiAckPoll << HOSTSRT(i2c_cmd_acpollmax);
			ptI2cUnit->ulI2c_cmd = ulValue;

			iResult = i2c_wait_for_command_done();
			if( iResult!=0 )
			{
				uprintf("Failed to execute the start command.\n");
			}
			else
			{
				/* Was the start condition acknowledged? */
				ulValue  = ptI2cUnit->ulI2c_sr;
				ulValue &= HOSTMSK(i2c_sr_last_ac);
				if( ulValue==0 )
				{
					/* No ACK received. */
					uprintf("No ACK received 1.\n");
					iResult = -1;
				}
			}
		}

		if( iResult==0 && uiDataLength!=0 )
		{
			while( uiDataLength!=0 )
			{
				ulChunkTransaction = uiDataLength;
				if( ulChunkTransaction>((HOSTMSK(i2c_cmd_tsize)>>HOSTSRT(i2c_cmd_tsize))+1U) )
				{
					ulChunkTransaction = (HOSTMSK(i2c_cmd_tsize)>>HOSTSRT(i2c_cmd_tsize)) + 1U;
				}
				uiDataLength -= ulChunkTransaction;

				/* Execute transfer. */
				ulValue  = 1 << HOSTSRT(i2c_cmd_nwr);
				/* Is this the last transfer for this data block? */
				if( uiDataLength!=0 )
				{
					/* No -> there will be more transfers. */
					ulValue |= I2CCMD_CTC << HOSTSRT(i2c_cmd_cmd);
				}
				/* This is the last transfer for this data block.
				 * Should the transfer be continued after this request?
				 */
				else if( (iCond&I2C_CONTINUE)==0 )
				{
					/* No, the transfer should not be continued. */
					ulValue |= I2CCMD_CT << HOSTSRT(i2c_cmd_cmd);
				}
				else
				{
					/* Yes, the transfer should be continued. */
					ulValue |= I2CCMD_CTC << HOSTSRT(i2c_cmd_cmd);
				}
				ulValue |= (ulChunkTransaction-1U) << HOSTSRT(i2c_cmd_tsize);
				ulValue |= 0 << HOSTSRT(i2c_cmd_acpollmax);
				ptI2cUnit->ulI2c_cmd = ulValue;

				/* Loop over all requested bytes. */
				do
				{
					ulChunkFifo   = ptI2cUnit->ulI2c_sr;
					ulChunkFifo  &= HOSTMSK(i2c_sr_mfifo_level);
					ulChunkFifo >>= HOSTSRT(i2c_sr_mfifo_level);

					/* Limit the FIFO chunk with the number of bytes to transfer. */
					if( ulChunkFifo>ulChunkTransaction )
					{
						ulChunkFifo = ulChunkTransaction;
					}
					ulChunkTransaction -= ulChunkFifo;

					while( ulChunkFifo!=0 )
					{
						ulValue = ptI2cUnit->ulI2c_mdr;
						*(pucData++) = (unsigned char)ulValue;
						--ulChunkFifo;
					}
				} while( ulChunkTransaction!=0 );

				iResult = i2c_wait_for_command_done();
				if( iResult!=0 )
				{
					uprintf("Failed to execute the start command.\n");
					break;
				}
			}
		}

		/* Send a stop condition? */
		if( iResult==0 && (iCond&I2C_STOP_COND)!=0 )
		{
			/* Execute stop condition. */
			ulValue  = 1 << HOSTSRT(i2c_cmd_nwr);
			ulValue |= I2CCMD_STOP << HOSTSRT(i2c_cmd_cmd);
			ulValue |= 0 << HOSTSRT(i2c_cmd_tsize);
			ulValue |= 0 << HOSTSRT(i2c_cmd_acpollmax);
			ptI2cUnit->ulI2c_cmd = ulValue;

			iResult = i2c_wait_for_command_done();
			if( iResult!=0 )
			{
				uprintf("Failed to execute the start command.\n");
			}
		}
	}

	return iResult;
}



static int i2c_core_hsoc_v2_set_device_specific_speed(unsigned long ulDeviceSpecificValue)
{
	int iResult;
	unsigned long ulValue;


	if( ulDeviceSpecificValue>(HOSTMSK(i2c_mcr_mode)>>HOSTSRT(i2c_mcr_mode)) )
	{
		iResult = -1;
	}
	else
	{
		ulValue  = ptI2cUnit->ulI2c_mcr;
		ulValue &= ~HOSTMSK(i2c_mcr_mode);
		ulValue |= ulDeviceSpecificValue << HOSTSRT(i2c_mcr_mode);
		ptI2cUnit->ulI2c_mcr = ulValue;

		iResult = 0;
	}

	return iResult;
}



static const I2C_FUNCTIONS_T i2c_core_functions =
{
	.fnSend                     = i2c_core_hsoc_v2_send,
	.fnRecv                     = i2c_core_hsoc_v2_recv,
	.fnSetDeviceSpecificSpeed   = i2c_core_hsoc_v2_set_device_specific_speed
};


const I2C_FUNCTIONS_T *i2c_core_hsoc_v2_init(unsigned int uiCoreIdx)
{
	HOSTDEF(ptRAPI2C0Area);
	HOSTDEF(ptRAPI2C1Area);
	HOSTDEF(ptRAPI2C2Area);
	HOSTDEF(ptRAPI2C3Area);
	HOSTDEF(ptRAPI2C4Area);
	HOSTDEF(ptRAPI2C5Area);
	HOSTDEF(ptI2c0Area);
	HOSTDEF(ptI2c1Area);
	HOSTDEF(ptI2c2Area);
	unsigned long ulValue;
	HOSTADEF(I2C) * ptUnit;
	const I2C_FUNCTIONS_T *ptFn;


	ptUnit = NULL;
	ptFn = NULL;

	if( uiCoreIdx==0 )
	{
		ptUnit = ptRAPI2C0Area;
	}
	else if( uiCoreIdx==1 )
	{
		ptUnit = ptRAPI2C1Area;
	}
	else if( uiCoreIdx==2 )
	{
		ptUnit = ptRAPI2C2Area;
	}
	else if( uiCoreIdx==3 )
	{
		ptUnit = ptRAPI2C3Area;
	}
	else if( uiCoreIdx==4 )
	{
		ptUnit = ptRAPI2C4Area;
	}
	else if( uiCoreIdx==5 )
	{
		ptUnit = ptRAPI2C5Area;
	}
	else if( uiCoreIdx==6 )
	{
		ptUnit = ptI2c0Area;
	}
	else if( uiCoreIdx==7 )
	{
		ptUnit = ptI2c1Area;
	}
	else if( uiCoreIdx==8 )
	{
		ptUnit = ptI2c2Area;
	}

	if( ptUnit!=NULL )
	{
		ptI2cUnit = ptUnit;

		/* Reset the unit. */
		ulValue = HOSTMSK(i2c_mcr_rst_i2c);
		ptI2cUnit->ulI2c_mcr = ulValue;

		/* Disable the unit. */
		ptI2cUnit->ulI2c_mcr = 0;
		/* Disable slave mode. */
		ptI2cUnit->ulI2c_scr = 0;

		/* Clear the master FIFO. */
		ptI2cUnit->ulI2c_mfifo_cr = HOSTMSK(i2c_mfifo_cr_mfifo_clr);
		ptI2cUnit->ulI2c_mfifo_cr = 0;
		/* Clear the slave FIFO. */
		ptI2cUnit->ulI2c_sfifo_cr = HOSTMSK(i2c_sfifo_cr_sfifo_clr);
		ptI2cUnit->ulI2c_sfifo_cr = 0;

		/* Do not use IRQs. */
		ptI2cUnit->ulI2c_irqmsk = 0;
		ulValue  = HOSTMSK(i2c_irqsr_sreq);
		ulValue |= HOSTMSK(i2c_irqsr_sfifo_req);
		ulValue |= HOSTMSK(i2c_irqsr_mfifo_req);
		ulValue |= HOSTMSK(i2c_irqsr_bus_busy);
		ulValue |= HOSTMSK(i2c_irqsr_fifo_err);
		ulValue |= HOSTMSK(i2c_irqsr_cmd_err);
		ulValue |= HOSTMSK(i2c_irqsr_cmd_ok);
		ptI2cUnit->ulI2c_irqsr = ulValue;

		/* Do not use DMAs. */
		ptI2cUnit->ulI2c_dmacr = 0;

		/* Clear the timeout state. */
		ulValue  = HOSTMSK(i2c_sr_timeout);
		ptI2cUnit->ulI2c_sr = ulValue;

		/* Enable I2C core, and set the speed to 100KHz. */
		ulValue  = HOSTMSK(i2c_mcr_en_timeout);
		ulValue |= I2CSPEED_100 << HOSTSRT(i2c_mcr_mode);
		ulValue |= HOSTMSK(i2c_mcr_en_i2c);
		ptI2cUnit->ulI2c_mcr = ulValue;

		ptFn = &i2c_core_functions;
	}

	/* return the pointer to the functions */
	return ptFn;
}


/*-----------------------------------*/

