/***************************************************************************
 *   Copyright (C) 2019 by Christoph Thelen                                *
 *   doc_bacardi@users.sourceforge.net                                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/


#include "main_test.h"

#include <string.h>

#include "netx_io_areas.h"
#include "portcontrol.h"
#include "rdy_run.h"
#include "systime.h"
#include "uprintf.h"
#include "version.h"


/*-------------------------------------------------------------------------*/


struct __attribute__((__packed__)) I2C_SEQ_COMMAND_RW_STRUCT
{
        unsigned char ucConditions;
        unsigned char ucAddress;
        unsigned char ucAckPoll;
        unsigned short usDataSize;
};

typedef union I2C_SEQ_COMMAND_RW_UNION
{
        struct I2C_SEQ_COMMAND_RW_STRUCT s;
        unsigned char auc[5];
} I2C_SEQ_COMMAND_RW_T;



struct __attribute__((__packed__)) I2C_SEQ_COMMAND_DELAY_STRUCT
{
        unsigned long ulDelayInMs;
};

typedef union I2C_SEQ_COMMAND_DELAY_UNION
{
        struct I2C_SEQ_COMMAND_DELAY_STRUCT s;
        unsigned char auc[4];
} I2C_SEQ_COMMAND_DELAY_T;



typedef struct CMD_STATE_STRUCT
{
	unsigned long ulVerbose;
	const unsigned char *pucCmdCnt;
	const unsigned char *pucCmdEnd;
	unsigned char *pucRecCnt;
	unsigned char *pucRecEnd;
} CMD_STATE_T;



static int command_read(CMD_STATE_T *ptState, const I2C_HANDLE_T *ptHandle)
{
	int iResult;
	const I2C_SEQ_COMMAND_RW_T *ptCmd;
	unsigned long ulDataSize;
	unsigned long ulValue;
	int iConditions;
	unsigned int uiAckPoll;


	if( (ptState->pucCmdCnt + sizeof(I2C_SEQ_COMMAND_RW_T))>ptState->pucCmdEnd )
	{
		if( ptState->ulVerbose!=0U )
		{
			uprintf("Not enough data for the read command left.\n");
		}
		iResult = -1;
	}
	else
	{
		ptCmd = (const I2C_SEQ_COMMAND_RW_T*)(ptState->pucCmdCnt);
		ulDataSize = ptCmd->s.usDataSize;
		if( (ptState->pucRecCnt + ulDataSize)>ptState->pucRecEnd )
		{
			if( ptState->ulVerbose!=0U )
			{
				uprintf("Not enough data for the receive data left.\n");
			}
			iResult = -1;
		}
		else
		{
			/* Combine the address and the conditions for the
			 * driver to one 32bit value.
			 */
			iConditions = (int)(ptCmd->s.ucAddress);
			ulValue = (unsigned long)(ptCmd->s.ucConditions);
			if( (ulValue&I2C_SEQ_CONDITION_Start)!=0 )
			{
				iConditions |= I2C_START_COND;
			}
			if( (ulValue&I2C_SEQ_CONDITION_Stop)!=0 )
			{
				iConditions |= I2C_STOP_COND;
			}
			if( (ulValue&I2C_SEQ_CONDITION_Continue)!=0 )
			{
				iConditions |= I2C_CONTINUE;
			}

			/* Get the ACK poll value. */
			uiAckPoll = (unsigned int)(ptCmd->s.ucAckPoll);

			if( ptState->ulVerbose!=0U )
			{
				if( (iConditions&I2C_START_COND)!=0 )
				{
					uprintf("START\n");
				}
				if( (iConditions&I2C_CONTINUE)!=0 )
				{
					uprintf("CONTINUE\n");
				}
				uprintf("READ from address 0x%02x, %d retries, %d bytes\n", ptCmd->s.ucAddress, uiAckPoll, ulDataSize);
			}

			/* Run the command. */
			iResult = ptHandle->tI2CFn.fnRecv(ptHandle, iConditions, uiAckPoll, ulDataSize, ptState->pucRecCnt);
			if( iResult!=0 )
			{
				if( ptState->ulVerbose!=0U )
				{
					uprintf("The I2C receive operation failed.\n");
				}
			}
			else
			{
				if( ptState->ulVerbose!=0U )
				{
					hexdump(ptState->pucRecCnt, ulDataSize);
					if( (iConditions&I2C_STOP_COND)!=0 )
					{
						uprintf("STOP\n");
					}
				}
				ptState->pucCmdCnt += sizeof(I2C_SEQ_COMMAND_RW_T);
				ptState->pucRecCnt += ulDataSize;
			}
		}
	}

	return iResult;
}



static int command_write(CMD_STATE_T *ptState, const I2C_HANDLE_T *ptHandle)
{
	int iResult;
	const I2C_SEQ_COMMAND_RW_T *ptCmd;
	unsigned long ulDataSize;
	unsigned long ulValue;
	int iConditions;
	unsigned int uiAckPoll;


	if( (ptState->pucCmdCnt + sizeof(I2C_SEQ_COMMAND_RW_T))>ptState->pucCmdEnd )
	{
		if( ptState->ulVerbose!=0U )
		{
			uprintf("Not enough data for the write header left.\n");
		}
		iResult = -1;
	}
	else
	{
		ptCmd = (const I2C_SEQ_COMMAND_RW_T*)(ptState->pucCmdCnt);
		ulDataSize = ptCmd->s.usDataSize;
		if( (ptState->pucCmdCnt + sizeof(I2C_SEQ_COMMAND_RW_T) + ulDataSize)>ptState->pucCmdEnd )
		{
			if( ptState->ulVerbose!=0U )
			{
				uprintf("Not enough data for the complete write command left.\n");
			}
			iResult = -1;
		}
		else
		{
			/* Combine the address and the conditions for the
			 * driver to one 32bit value.
			 */
			iConditions = (int)(ptCmd->s.ucAddress);
			ulValue = (unsigned long)(ptCmd->s.ucConditions);
			if( (ulValue&I2C_SEQ_CONDITION_Start)!=0 )
			{
				iConditions |= I2C_START_COND;
			}
			if( (ulValue&I2C_SEQ_CONDITION_Stop)!=0 )
			{
				iConditions |= I2C_STOP_COND;
			}
			if( (ulValue&I2C_SEQ_CONDITION_Continue)!=0 )
			{
				iConditions |= I2C_CONTINUE;
			}

			/* Get the ACK poll value. */
			uiAckPoll = (unsigned int)(ptCmd->s.ucAckPoll);

			if( ptState->ulVerbose!=0U )
			{
				if( (iConditions&I2C_START_COND)!=0 )
				{
					uprintf("START\n");
				}
				if( (iConditions&I2C_CONTINUE)!=0 )
				{
					uprintf("CONTINUE\n");
				}
				uprintf("WRITE to address 0x%02x, %d retries, %d bytes\n", ptCmd->s.ucAddress, uiAckPoll, ulDataSize);
				hexdump(ptState->pucCmdCnt + sizeof(I2C_SEQ_COMMAND_RW_T), ulDataSize);
			}

			/* Run the command. */
			iResult = ptHandle->tI2CFn.fnSend(ptHandle, iConditions, uiAckPoll, ulDataSize, ptState->pucCmdCnt + sizeof(I2C_SEQ_COMMAND_RW_T));
			if( iResult!=0 )
			{
				if( ptState->ulVerbose!=0U )
				{
					uprintf("The I2C send operation failed.\n");
				}
			}
			else
			{
				if( ptState->ulVerbose!=0U )
				{
					if( (iConditions&I2C_STOP_COND)!=0 )
					{
						uprintf("STOP\n");
					}
				}
				ptState->pucCmdCnt += sizeof(I2C_SEQ_COMMAND_RW_T) + ulDataSize;
			}
		}
	}

	return iResult;
}



static int command_delay(CMD_STATE_T *ptState)
{
	int iResult;
	const I2C_SEQ_COMMAND_DELAY_T *ptCmd;


	if( (ptState->pucCmdCnt + sizeof(I2C_SEQ_COMMAND_DELAY_T))>ptState->pucCmdEnd )
	{
		if( ptState->ulVerbose!=0U )
		{
			uprintf("Not enough data for the delay command left.\n");
		}
		iResult = -1;
	}
	else
	{
		ptCmd = (const I2C_SEQ_COMMAND_DELAY_T*)(ptState->pucCmdCnt);

		if( ptState->ulVerbose!=0U )
		{
			uprintf("I2C_COMMAND_Delay %d ms\n", ptCmd->s.ulDelayInMs);
		}

		systime_delay_ms(ptCmd->s.ulDelayInMs);
		iResult = 0;
		ptState->pucCmdCnt += sizeof(I2C_SEQ_COMMAND_DELAY_T);
	}

	return iResult;
}



typedef struct I2C_INTERFACE_NAME_LOOKUP_STRUCT
{
	I2C_SETUP_CORE_T tID;
	const char *pcName;
} I2C_INTERFACE_NAME_LOOKUP_T;

static const I2C_INTERFACE_NAME_LOOKUP_T atI2cInterfaceNameLookup[] =
{
	{
		.tID = I2C_SETUP_CORE_RAPI2C0,
		.pcName = "RAP_I2C0"
	},
	{
		.tID = I2C_SETUP_CORE_RAPI2C1,
		.pcName = "RAP_I2C1"
	},
	{
		.tID = I2C_SETUP_CORE_RAPI2C2,
		.pcName = "RAP_I2C2"
	},
	{
		.tID = I2C_SETUP_CORE_RAPI2C3,
		.pcName = "RAP_I2C3"
	},
	{
		.tID = I2C_SETUP_CORE_RAPI2C4,
		.pcName = "RAP_I2C4"
	},
	{
		.tID = I2C_SETUP_CORE_RAPI2C5,
		.pcName = "RAP_I2C5"
	},
	{
		.tID = I2C_SETUP_CORE_I2C0,
		.pcName = "I2C0"
	},
	{
		.tID = I2C_SETUP_CORE_I2C1,
		.pcName = "I2C1"
	},
	{
		.tID = I2C_SETUP_CORE_I2C2,
		.pcName = "I2C2"
	}
};


static const char *getInterfaceName(I2C_SETUP_CORE_T tCore)
{
	const I2C_INTERFACE_NAME_LOOKUP_T *pcCnt;
	const I2C_INTERFACE_NAME_LOOKUP_T *pcEnd;
	const char *pcName;


	pcName = "Unknown";

	pcCnt = atI2cInterfaceNameLookup;
	pcEnd = atI2cInterfaceNameLookup + (sizeof(atI2cInterfaceNameLookup)/sizeof(atI2cInterfaceNameLookup[0]));
	while( pcCnt<pcEnd )
	{
		if( pcCnt->tID==tCore )
		{
			pcName = pcCnt->pcName;
			break;
		}
		++pcCnt;
	};

	return pcName;
}



static TEST_RESULT_T processCommandOpen(unsigned long ulVerbose, I2C_PARAMETER_OPEN_T *ptParameter)
{
	TEST_RESULT_T tResult;
	int iResult;
	I2C_SETUP_T tI2CSetup;
	I2C_SETUP_CORE_T tCore;
	I2C_HANDLE_T *ptHandle;
	const char *pcIfName;


	/* Check the parameter. */
	tResult = TEST_RESULT_ERROR;
	tCore = (I2C_SETUP_CORE_T)(ptParameter->usI2CCore);
	switch(tCore)
	{
	case I2C_SETUP_CORE_RAPI2C0:
	case I2C_SETUP_CORE_RAPI2C1:
	case I2C_SETUP_CORE_RAPI2C2:
	case I2C_SETUP_CORE_RAPI2C3:
	case I2C_SETUP_CORE_RAPI2C4:
	case I2C_SETUP_CORE_RAPI2C5:
	case I2C_SETUP_CORE_I2C0:
	case I2C_SETUP_CORE_I2C1:
	case I2C_SETUP_CORE_I2C2:
		tResult = TEST_RESULT_OK;
		break;
	}
	if( tResult!=TEST_RESULT_OK )
	{
		uprintf("Invalid core selected: %d\n", tCore);
	}
	else
	{
		tI2CSetup.tI2CCore = tCore;
		tI2CSetup.aucMmioIndex[I2C_SETUP_PIN_INDEX_SCL] = ptParameter->ucMMIOIndexSCL;
		tI2CSetup.aucMmioIndex[I2C_SETUP_PIN_INDEX_SDA] = ptParameter->ucMMIOIndexSDA;
		tI2CSetup.ausPortControl[I2C_SETUP_PIN_INDEX_SCL] = ptParameter->usPortcontrolSCL;
		tI2CSetup.ausPortControl[I2C_SETUP_PIN_INDEX_SDA] = ptParameter->usPortcontrolSDA;

		if( ulVerbose!=0 )
		{
			pcIfName = getInterfaceName(tCore);
			uprintf("Setup interface %s with MMIOs %d/%d and port control 0x%04x/0x%04x.\n",
			        pcIfName,
			        tI2CSetup.aucMmioIndex[I2C_SETUP_PIN_INDEX_SCL],
				tI2CSetup.aucMmioIndex[I2C_SETUP_PIN_INDEX_SDA],
				tI2CSetup.ausPortControl[I2C_SETUP_PIN_INDEX_SCL],
				tI2CSetup.ausPortControl[I2C_SETUP_PIN_INDEX_SDA]
			);
		}

		ptHandle = (I2C_HANDLE_T*)(ptParameter->ptHandle);
		iResult = i2c_core_hsoc_v2_init(&tI2CSetup, ptHandle);
		if( iResult!=0 )
		{
			uprintf("Failed to setup the I2C core.\n");
			tResult = TEST_RESULT_ERROR;
		}
	}

	return tResult;
}



static int processCommandSequence(unsigned long ulVerbose, I2C_PARAMETER_RUN_SEQUENCE_T *ptParameter)
{
	int iResult;
	CMD_STATE_T tState;
	unsigned char ucData;
	I2C_SEQ_COMMAND_T tCmd;
	unsigned int uiDataSize;
	I2C_HANDLE_T *ptHandle;


	/* An empty command is OK. */
	iResult = 0;

	/* Get the verbose flag. */
	tState.ulVerbose = ulVerbose;

	/* Get the handle. */
	ptHandle = (I2C_HANDLE_T*)(ptParameter->ptHandle);

	/* Loop over all commands. */
	tState.pucCmdCnt = ptParameter->pucCommand;
	tState.pucCmdEnd = tState.pucCmdCnt + ptParameter->sizCommand;
	tState.pucRecCnt = ptParameter->pucReceivedData;
	tState.pucRecEnd = tState.pucRecCnt + ptParameter->sizReceivedDataMax;
	if( tState.ulVerbose!=0U )
	{
		uprintf("Running command [0x%08x, 0x%08x[.\n", (unsigned long)tState.pucCmdCnt, (unsigned long)tState.pucCmdEnd);
	}

	while( tState.pucCmdCnt<tState.pucCmdEnd )
	{
		/* Get the next command. */
		iResult = -1;
		ucData = *(tState.pucCmdCnt++);
		tCmd = (I2C_SEQ_COMMAND_T)ucData;
		switch( tCmd )
		{
		case I2C_SEQ_COMMAND_Read:
		case I2C_SEQ_COMMAND_Write:
		case I2C_SEQ_COMMAND_Delay:
			iResult = 0;
			break;
		}
		if( iResult!=0 )
		{
			uprintf("Invalid command: 0x%02x\n", ucData);
			break;
		}
		else
		{
			switch( tCmd )
			{
			case I2C_SEQ_COMMAND_Read:
				iResult = command_read(&tState, ptHandle);
				break;

			case I2C_SEQ_COMMAND_Write:
				iResult = command_write(&tState, ptHandle);
				break;

			case I2C_SEQ_COMMAND_Delay:
				iResult = command_delay(&tState);
				break;
			}
			if( iResult!=0 )
			{
				if( tState.ulVerbose!=0U )
				{
					uprintf("The command failed. Stopping execution of the sequence.\n");
				}
				break;
			}
		}
	}

	if( iResult==0 )
	{
		/* Set the size of the result data. */
		uiDataSize = (unsigned int)(tState.pucRecCnt-ptParameter->pucReceivedData);
		if( uiDataSize<=ptParameter->sizReceivedDataMax )
		{
			ptParameter->sizReceivedData = uiDataSize;
		}
		else
		{
			iResult = -1;
		}
	}

	return iResult;
}



TEST_RESULT_T test(I2C_PARAMETER_T *ptTestParams)
{
	TEST_RESULT_T tResult;
	int iResult;
	unsigned long ulVerbose;
	I2C_CMD_T tCmd;

	systime_init();

	/* Set the verbose mode. */
	ulVerbose = ptTestParams->ulVerbose;
	if( ulVerbose!=0 )
	{
		uprintf("\f. *** I2C test by doc_bacardi@users.sourceforge.net ***\n");
		uprintf("V" VERSION_ALL "\n\n");

		/* Get the test parameter. */
		uprintf(". Parameters: 0x%08x\n", (unsigned long)ptTestParams);
		uprintf(".   Verbose: 0x%08x\n", ptTestParams->ulVerbose);
	}

	tCmd = (I2C_CMD_T)(ptTestParams->ulCommand);
	tResult = TEST_RESULT_ERROR;
	switch(tCmd)
	{
	case I2C_CMD_Open:
	case I2C_CMD_RunSequence:
	case I2C_CMD_Close:
		tResult = TEST_RESULT_OK;
		break;
	}
	if( tResult!=TEST_RESULT_OK )
	{
		uprintf("Invalid command: 0x%08x\n", tCmd);
	}
	else
	{
		switch(tCmd)
		{
		case I2C_CMD_Open:
			tResult = processCommandOpen(ulVerbose, &(ptTestParams->uParameter.tOpen));
			break;

		case I2C_CMD_RunSequence:
			iResult = processCommandSequence(ulVerbose, &(ptTestParams->uParameter.tRunSequence));
			if( iResult!=0 )
			{
				tResult = TEST_RESULT_ERROR;
			}
			break;

		case I2C_CMD_Close:
			uprintf("Not yet.\n");
			tResult = TEST_RESULT_ERROR;
			break;
		}
	}

	if( tResult==TEST_RESULT_OK )
	{
		rdy_run_setLEDs(RDYRUN_GREEN);
	}
	else
	{
		rdy_run_setLEDs(RDYRUN_YELLOW);
	}

	return tResult;
}

/*-----------------------------------*/
