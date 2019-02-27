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
#include "rdy_run.h"
#include "systime.h"
#include "uprintf.h"
#include "version.h"


/*-------------------------------------------------------------------------*/

/* FIXME: remove this, only for easy debug. */
#include "serial_vectors.h"
static I2C_PARAMETER_T tDemoParams;

/*
static void setup_ios(void)
{

}
*/


TEST_RESULT_T test(I2C_PARAMETER_T *ptTestParams)
{
	TEST_RESULT_T tResult;
	int iResult;
	unsigned long ulVerbose;
	unsigned int uiCoreIdx;
	const I2C_FUNCTIONS_T *ptI2CFn;
	unsigned char aucCmd[4];
	unsigned char aucData[8];


	/* FIXME: remove this, only for easy debug. */
	tDemoParams.ulVerbose = 0;
	ptTestParams = &tDemoParams;
	tSerialVectors.fn.fnPut = NULL;


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

	tResult = TEST_RESULT_ERROR;
	uiCoreIdx = 0U;

	ptI2CFn = i2c_core_hsoc_v2_init(uiCoreIdx);
	if( ptI2CFn==NULL )
	{
		uprintf("Failed to setup the I2C core.\n");
	}
	else
	{
		while(1)
		{
			memset(aucData, 0, sizeof(aucData));

			aucCmd[0] = 0x00;    /* Register address 0. */
			iResult = ptI2CFn->fnSend(I2C_START_COND|0x51, 16U, 1, aucCmd);
			if( iResult==0 )
			{
				iResult = ptI2CFn->fnRecv(I2C_START_COND|I2C_STOP_COND|0x51, 16U, 8, aucData);
			}
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
