#include <CAENDigitizer.h>
#include "wavedump.h"
#include "WDconfig.h"
#include "WDplot.h"
#include "fft.h"
#include "keyb.h"
#include "X742CorrectionRoutines.h"
#include "hdf5.h"
#include "unistd.h" /* for access(). */
#include "signal.h" /* for SIGINT. */

#define WF_SIZE 10000

char *GitSHA1(void);
char *GitDirty(void);

extern int dc_file[MAX_CH];
extern int thr_file[MAX_CH];
int cal_ok[MAX_CH] = { 0 };
char path[128];

int stop = 0;

void sigint_handler(int dummy)
{
    stop = 1;
}

/* Error messages */
typedef enum  {
    ERR_NONE= 0,
    ERR_CONF_FILE_NOT_FOUND,
    ERR_DGZ_OPEN,
    ERR_BOARD_INFO_READ,
    ERR_INVALID_BOARD_TYPE,
    ERR_DGZ_PROGRAM,
    ERR_MALLOC,
    ERR_RESTART,
    ERR_INTERRUPT,
    ERR_READOUT,
    ERR_EVENT_BUILD,
    ERR_HISTO_MALLOC,
    ERR_UNHANDLED_BOARD,
    ERR_OUTFILE_WRITE,
	ERR_OVERTEMP,
	ERR_BOARD_FAILURE,

    ERR_DUMMY_LAST,
} ERROR_CODES;
static char ErrMsg[ERR_DUMMY_LAST][100] = {
    "No Error",                                         /* ERR_NONE */
    "Configuration File not found",                     /* ERR_CONF_FILE_NOT_FOUND */
    "Can't open the digitizer",                         /* ERR_DGZ_OPEN */
    "Can't read the Board Info",                        /* ERR_BOARD_INFO_READ */
    "Can't run WaveDump for this digitizer",            /* ERR_INVALID_BOARD_TYPE */
    "Can't program the digitizer",                      /* ERR_DGZ_PROGRAM */
    "Can't allocate the memory for the readout buffer", /* ERR_MALLOC */
    "Restarting Error",                                 /* ERR_RESTART */
    "Interrupt Error",                                  /* ERR_INTERRUPT */
    "Readout Error",                                    /* ERR_READOUT */
    "Event Build Error",                                /* ERR_EVENT_BUILD */
    "Can't allocate the memory fro the histograms",     /* ERR_HISTO_MALLOC */
    "Unhandled board type",                             /* ERR_UNHANDLED_BOARD */
    "Output file write error",                          /* ERR_OUTFILE_WRITE */
	"Over Temperature",									/* ERR_OVERTEMP */
	"Board Failure",									/* ERR_BOARD_FAILURE */

};


#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

static CAEN_DGTZ_IRQMode_t INTERRUPT_MODE = CAEN_DGTZ_IRQ_MODE_ROAK;

/*! \fn      int GetMoreBoardNumChannels(CAEN_DGTZ_BoardInfo_t BoardInfo,  WaveDumpConfig_t *WDcfg)
*   \brief   calculate num of channels, num of bit and sampl period according to the board type
*
*   \param   BoardInfo   Board Type
*   \param   WDcfg       pointer to the config. struct
*   \return  0 = Success; -1 = unknown board type
*/
int GetMoreBoardInfo(int handle, CAEN_DGTZ_BoardInfo_t BoardInfo, WaveDumpConfig_t *WDcfg)
{
    int ret;
    switch(BoardInfo.FamilyCode) {
        CAEN_DGTZ_DRS4Frequency_t freq;

    case CAEN_DGTZ_XX724_FAMILY_CODE:
    case CAEN_DGTZ_XX781_FAMILY_CODE:
    case CAEN_DGTZ_XX782_FAMILY_CODE:
    case CAEN_DGTZ_XX780_FAMILY_CODE:
        WDcfg->Nbit = 14; WDcfg->Ts = 10.0; break;
    case CAEN_DGTZ_XX720_FAMILY_CODE: WDcfg->Nbit = 12; WDcfg->Ts = 4.0;  break;
    case CAEN_DGTZ_XX721_FAMILY_CODE: WDcfg->Nbit =  8; WDcfg->Ts = 2.0;  break;
    case CAEN_DGTZ_XX731_FAMILY_CODE: WDcfg->Nbit =  8; WDcfg->Ts = 2.0;  break;
    case CAEN_DGTZ_XX751_FAMILY_CODE: WDcfg->Nbit = 10; WDcfg->Ts = 1.0;  break;
    case CAEN_DGTZ_XX761_FAMILY_CODE: WDcfg->Nbit = 10; WDcfg->Ts = 0.25;  break;
    case CAEN_DGTZ_XX740_FAMILY_CODE: WDcfg->Nbit = 12; WDcfg->Ts = 16.0; break;
    case CAEN_DGTZ_XX725_FAMILY_CODE: WDcfg->Nbit = 14; WDcfg->Ts = 4.0; break;
    case CAEN_DGTZ_XX730_FAMILY_CODE: WDcfg->Nbit = 14; WDcfg->Ts = 2.0; break;
    case CAEN_DGTZ_XX742_FAMILY_CODE: 
        WDcfg->Nbit = 12; 
        if ((ret = CAEN_DGTZ_GetDRS4SamplingFrequency(handle, &freq)) != CAEN_DGTZ_Success) return CAEN_DGTZ_CommError;
        switch (freq) {
        case CAEN_DGTZ_DRS4_1GHz:
            WDcfg->Ts = 1.0;
            break;
        case CAEN_DGTZ_DRS4_2_5GHz:
            WDcfg->Ts = (float)0.4;
            break;
        case CAEN_DGTZ_DRS4_5GHz:
            WDcfg->Ts = (float)0.2;
            break;
		case CAEN_DGTZ_DRS4_750MHz:
            WDcfg->Ts = (float)(1.0/750.0)*1000.0;
            break;
        }
        switch(BoardInfo.FormFactor) {
        case CAEN_DGTZ_VME64_FORM_FACTOR:
        case CAEN_DGTZ_VME64X_FORM_FACTOR:
            WDcfg->MaxGroupNumber = 4;
            break;
        case CAEN_DGTZ_DESKTOP_FORM_FACTOR:
        case CAEN_DGTZ_NIM_FORM_FACTOR:
        default:
            WDcfg->MaxGroupNumber = 2;
            break;
        }
        break;
    default: return -1;
    }
    if (((BoardInfo.FamilyCode == CAEN_DGTZ_XX751_FAMILY_CODE) ||
        (BoardInfo.FamilyCode == CAEN_DGTZ_XX731_FAMILY_CODE) ) && WDcfg->DesMode)
        WDcfg->Ts /= 2;

    switch(BoardInfo.FamilyCode) {
    case CAEN_DGTZ_XX724_FAMILY_CODE:
    case CAEN_DGTZ_XX781_FAMILY_CODE:
    case CAEN_DGTZ_XX782_FAMILY_CODE:
    case CAEN_DGTZ_XX780_FAMILY_CODE:
    case CAEN_DGTZ_XX720_FAMILY_CODE:
    case CAEN_DGTZ_XX721_FAMILY_CODE:
    case CAEN_DGTZ_XX751_FAMILY_CODE:
    case CAEN_DGTZ_XX761_FAMILY_CODE:
    case CAEN_DGTZ_XX731_FAMILY_CODE:
        switch(BoardInfo.FormFactor) {
        case CAEN_DGTZ_VME64_FORM_FACTOR:
        case CAEN_DGTZ_VME64X_FORM_FACTOR:
            WDcfg->Nch = 8;
            break;
        case CAEN_DGTZ_DESKTOP_FORM_FACTOR:
        case CAEN_DGTZ_NIM_FORM_FACTOR:
            WDcfg->Nch = 4;
            break;
        }
        break;
    case CAEN_DGTZ_XX725_FAMILY_CODE:
    case CAEN_DGTZ_XX730_FAMILY_CODE:
        switch(BoardInfo.FormFactor) {
        case CAEN_DGTZ_VME64_FORM_FACTOR:
        case CAEN_DGTZ_VME64X_FORM_FACTOR:
            WDcfg->Nch = 16;
            break;
        case CAEN_DGTZ_DESKTOP_FORM_FACTOR:
        case CAEN_DGTZ_NIM_FORM_FACTOR:
            WDcfg->Nch = 8;
            break;
        }
        break;
    case CAEN_DGTZ_XX740_FAMILY_CODE:
        switch( BoardInfo.FormFactor) {
        case CAEN_DGTZ_VME64_FORM_FACTOR:
        case CAEN_DGTZ_VME64X_FORM_FACTOR:
            WDcfg->Nch = 64;
            break;
        case CAEN_DGTZ_DESKTOP_FORM_FACTOR:
        case CAEN_DGTZ_NIM_FORM_FACTOR:
            WDcfg->Nch = 32;
            break;
        }
        break;
    case CAEN_DGTZ_XX742_FAMILY_CODE:
        switch( BoardInfo.FormFactor) {
        case CAEN_DGTZ_VME64_FORM_FACTOR:
        case CAEN_DGTZ_VME64X_FORM_FACTOR:
            WDcfg->Nch = 36;
            break;
        case CAEN_DGTZ_DESKTOP_FORM_FACTOR:
        case CAEN_DGTZ_NIM_FORM_FACTOR:
            WDcfg->Nch = 16;
            break;
        }
        break;
    default:
        return -1;
    }
    return 0;
}

/*! \fn      int WriteRegisterBitmask(int32_t handle, uint32_t address, uint32_t data, uint32_t mask)
*   \brief   writes 'data' on register at 'address' using 'mask' as bitmask
*
*   \param   handle :   Digitizer handle
*   \param   address:   Address of the Register to write
*   \param   data   :   Data to Write on the Register
*   \param   mask   :   Bitmask to use for data masking
*   \return  0 = Success; negative numbers are error codes
*/
int WriteRegisterBitmask(int32_t handle, uint32_t address, uint32_t data, uint32_t mask) {
    int32_t ret = CAEN_DGTZ_Success;
    uint32_t d32 = 0xFFFFFFFF;

    ret = CAEN_DGTZ_ReadRegister(handle, address, &d32);
    if(ret != CAEN_DGTZ_Success)
        return ret;

    data &= mask;
    d32 &= ~mask;
    d32 |= data;
    ret = CAEN_DGTZ_WriteRegister(handle, address, d32);
    return ret;
}

static int CheckBoardFailureStatus(int handle, CAEN_DGTZ_BoardInfo_t BoardInfo) {

	int ret = 0;
	uint32_t status = 0;
	ret = CAEN_DGTZ_ReadRegister(handle, 0x8104, &status);
	if (ret != 0) {
		printf("Error: Unable to read board failure status.\n");
		return -1;
	}
#ifdef _WIN32
	Sleep(200);
#else
	usleep(200000);
#endif
	//read twice (first read clears the previous status)
	ret = CAEN_DGTZ_ReadRegister(handle, 0x8104, &status);
	if (ret != 0) {
		printf("Error: Unable to read board failure status.\n");
		return -1;
	}

	if(!(status & (1 << 7))) {
		printf("Board error detected: PLL not locked.\n");
		return -1;
	}

	return 0;
}


/*! \fn      int ProgramDigitizer(int handle, WaveDumpConfig_t WDcfg)
*   \brief   configure the digitizer according to the parameters read from
*            the cofiguration file and saved in the WDcfg data structure
*
*   \param   handle   Digitizer handle
*   \param   WDcfg:   WaveDumpConfig data structure
*   \return  0 = Success; negative numbers are error codes
*/
int ProgramDigitizer(int handle, WaveDumpConfig_t WDcfg, CAEN_DGTZ_BoardInfo_t BoardInfo)
{
    int i, j, ret = 0;

    /* reset the digitizer */
    ret |= CAEN_DGTZ_Reset(handle);
    if (ret != 0) {
        printf("Error: Unable to reset digitizer.\nPlease reset digitizer manually then restart the program\n");
        return -1;
    }

    // Set the waveform test bit for debugging
    if (WDcfg.TestPattern)
        ret |= CAEN_DGTZ_WriteRegister(handle, CAEN_DGTZ_BROAD_CH_CONFIGBIT_SET_ADD, 1<<3);
    // custom setting for X742 boards
    if (BoardInfo.FamilyCode == CAEN_DGTZ_XX742_FAMILY_CODE) {
        ret |= CAEN_DGTZ_SetFastTriggerDigitizing(handle,WDcfg.FastTriggerEnabled);
        ret |= CAEN_DGTZ_SetFastTriggerMode(handle,WDcfg.FastTriggerMode);
    }
    if ((BoardInfo.FamilyCode == CAEN_DGTZ_XX751_FAMILY_CODE) || (BoardInfo.FamilyCode == CAEN_DGTZ_XX731_FAMILY_CODE)) {
        ret |= CAEN_DGTZ_SetDESMode(handle, WDcfg.DesMode);
    }
    ret |= CAEN_DGTZ_SetRecordLength(handle, WDcfg.RecordLength);
    ret |= CAEN_DGTZ_GetRecordLength(handle, &WDcfg.RecordLength);

    if (BoardInfo.FamilyCode == CAEN_DGTZ_XX740_FAMILY_CODE || BoardInfo.FamilyCode == CAEN_DGTZ_XX724_FAMILY_CODE) {
        ret |= CAEN_DGTZ_SetDecimationFactor(handle, WDcfg.DecimationFactor);
    }

    ret |= CAEN_DGTZ_SetPostTriggerSize(handle, WDcfg.PostTrigger);
    if(BoardInfo.FamilyCode != CAEN_DGTZ_XX742_FAMILY_CODE) {
        uint32_t pt;
        ret |= CAEN_DGTZ_GetPostTriggerSize(handle, &pt);
        WDcfg.PostTrigger = pt;
    }
    ret |= CAEN_DGTZ_SetIOLevel(handle, WDcfg.FPIOtype);
    if( WDcfg.InterruptNumEvents > 0) {
        // Interrupt handling
        if( ret |= CAEN_DGTZ_SetInterruptConfig( handle, CAEN_DGTZ_ENABLE,
            VME_INTERRUPT_LEVEL, VME_INTERRUPT_STATUS_ID,
            (uint16_t)WDcfg.InterruptNumEvents, INTERRUPT_MODE)!= CAEN_DGTZ_Success) {
                printf( "\nError configuring interrupts. Interrupts disabled\n\n");
                WDcfg.InterruptNumEvents = 0;
        }
    }
	
    ret |= CAEN_DGTZ_SetMaxNumEventsBLT(handle, WDcfg.NumEvents);
    ret |= CAEN_DGTZ_SetAcquisitionMode(handle, CAEN_DGTZ_SW_CONTROLLED);
    ret |= CAEN_DGTZ_SetExtTriggerInputMode(handle, WDcfg.ExtTriggerMode);

    if ((BoardInfo.FamilyCode == CAEN_DGTZ_XX740_FAMILY_CODE) || (BoardInfo.FamilyCode == CAEN_DGTZ_XX742_FAMILY_CODE)){
        ret |= CAEN_DGTZ_SetGroupEnableMask(handle, WDcfg.EnableMask);
        for(i=0; i<(WDcfg.Nch/8); i++) {
            if (WDcfg.EnableMask & (1<<i)) {
                if (BoardInfo.FamilyCode == CAEN_DGTZ_XX742_FAMILY_CODE) {
                    for(j=0; j<8; j++) {
                        if (WDcfg.DCoffsetGrpCh[i][j] != -1)
                            ret |= CAEN_DGTZ_SetChannelDCOffset(handle,(i*8)+j, WDcfg.DCoffsetGrpCh[i][j]);
						else
                            ret |= CAEN_DGTZ_SetChannelDCOffset(handle, (i * 8) + j, WDcfg.DCoffset[i]);

                    }
                }
                else {
                    if(WDcfg.Version_used[i] == 1)
						ret |= Set_calibrated_DCO(handle, i, &WDcfg, BoardInfo);
					else
						ret |= CAEN_DGTZ_SetGroupDCOffset(handle, i, WDcfg.DCoffset[i]);
                    ret |= CAEN_DGTZ_SetGroupSelfTrigger(handle, WDcfg.ChannelTriggerMode[i], (1<<i));
                    ret |= CAEN_DGTZ_SetGroupTriggerThreshold(handle, i, WDcfg.Threshold[i]);
                    ret |= CAEN_DGTZ_SetChannelGroupMask(handle, i, WDcfg.GroupTrgEnableMask[i]);
                } 
                ret |= CAEN_DGTZ_SetTriggerPolarity(handle, i, WDcfg.PulsePolarity[i]); //.TriggerEdge

            }
        }
    } else {
        ret |= CAEN_DGTZ_SetChannelEnableMask(handle, WDcfg.EnableMask);
        for (i = 0; i < WDcfg.Nch; i++) {
            if (WDcfg.EnableMask & (1<<i)) {
				if (WDcfg.Version_used[i] == 1)
					ret |= Set_calibrated_DCO(handle, i, &WDcfg, BoardInfo);
				else
					ret |= CAEN_DGTZ_SetChannelDCOffset(handle, i, WDcfg.DCoffset[i]);
                if (BoardInfo.FamilyCode != CAEN_DGTZ_XX730_FAMILY_CODE &&
                    BoardInfo.FamilyCode != CAEN_DGTZ_XX725_FAMILY_CODE)
                    ret |= CAEN_DGTZ_SetChannelSelfTrigger(handle, WDcfg.ChannelTriggerMode[i], (1<<i));
                ret |= CAEN_DGTZ_SetChannelTriggerThreshold(handle, i, WDcfg.Threshold[i]);
                ret |= CAEN_DGTZ_SetTriggerPolarity(handle, i, WDcfg.PulsePolarity[i]); //.TriggerEdge
            }
        }
        if (BoardInfo.FamilyCode == CAEN_DGTZ_XX730_FAMILY_CODE ||
            BoardInfo.FamilyCode == CAEN_DGTZ_XX725_FAMILY_CODE) {
            // channel pair settings for x730 boards
            for (i = 0; i < WDcfg.Nch; i += 2) {
                if (WDcfg.EnableMask & (0x3 << i)) {
                    CAEN_DGTZ_TriggerMode_t mode = WDcfg.ChannelTriggerMode[i];
                    uint32_t pair_chmask = 0;

                    // Build mode and relevant channelmask. The behaviour is that,
                    // if the triggermode of one channel of the pair is DISABLED,
                    // this channel doesn't take part to the trigger generation.
                    // Otherwise, if both are different from DISABLED, the one of
                    // the even channel is used.
                    if (WDcfg.ChannelTriggerMode[i] != CAEN_DGTZ_TRGMODE_DISABLED) {
                        if (WDcfg.ChannelTriggerMode[i + 1] == CAEN_DGTZ_TRGMODE_DISABLED)
                            pair_chmask = (0x1 << i);
                        else
                            pair_chmask = (0x3 << i);
                    }
                    else {
                        mode = WDcfg.ChannelTriggerMode[i + 1];
                        pair_chmask = (0x2 << i);
                    }

                    pair_chmask &= WDcfg.EnableMask;
                    ret |= CAEN_DGTZ_SetChannelSelfTrigger(handle, mode, pair_chmask);
                }
            }
        }
    }
    if (BoardInfo.FamilyCode == CAEN_DGTZ_XX742_FAMILY_CODE) {
        for(i=0; i<(WDcfg.Nch/8); i++) {
            ret |= CAEN_DGTZ_SetDRS4SamplingFrequency(handle, WDcfg.DRS4Frequency);
            ret |= CAEN_DGTZ_SetGroupFastTriggerDCOffset(handle,i,WDcfg.FTDCoffset[i]);
            ret |= CAEN_DGTZ_SetGroupFastTriggerThreshold(handle,i,WDcfg.FTThreshold[i]);
        }
    }

    /* execute generic write commands */
    for(i=0; i<WDcfg.GWn; i++)
        ret |= WriteRegisterBitmask(handle, WDcfg.GWaddr[i], WDcfg.GWdata[i], WDcfg.GWmask[i]);

    if (ret)
        printf("Warning: errors found during the programming of the digitizer.\nSome settings may not be executed\n");

    return 0;
}

/*! \fn      void GoToNextEnabledGroup(WaveDumpRun_t *WDrun, WaveDumpConfig_t *WDcfg)
*   \brief   selects the next enabled group for plotting
*
*   \param   WDrun:   Pointer to the WaveDumpRun_t data structure
*   \param   WDcfg:   Pointer to the WaveDumpConfig_t data structure
*/
void GoToNextEnabledGroup(WaveDumpRun_t *WDrun, WaveDumpConfig_t *WDcfg) {
    if ((WDcfg->EnableMask) && (WDcfg->Nch>8)) {
        int orgPlotIndex = WDrun->GroupPlotIndex;
        do {
            WDrun->GroupPlotIndex = (++WDrun->GroupPlotIndex)%(WDcfg->Nch/8);
        } while( !((1 << WDrun->GroupPlotIndex)& WDcfg->EnableMask));
        if( WDrun->GroupPlotIndex != orgPlotIndex) {
            printf("Plot group set to %d\n", WDrun->GroupPlotIndex);
        }
    }
    ClearPlot();
}

/*! \brief   return TRUE if board descriped by 'BoardInfo' supports
*            calibration or not.
*
*   \param   BoardInfo board descriptor
*/
int32_t BoardSupportsCalibration(CAEN_DGTZ_BoardInfo_t BoardInfo) {
    return
		BoardInfo.FamilyCode == CAEN_DGTZ_XX761_FAMILY_CODE ||
        BoardInfo.FamilyCode == CAEN_DGTZ_XX751_FAMILY_CODE ||
        BoardInfo.FamilyCode == CAEN_DGTZ_XX730_FAMILY_CODE ||
        BoardInfo.FamilyCode == CAEN_DGTZ_XX725_FAMILY_CODE;
}

/*! \brief   return TRUE if board descriped by 'BoardInfo' supports
*            temperature read or not.
*
*   \param   BoardInfo board descriptor
*/
int32_t BoardSupportsTemperatureRead(CAEN_DGTZ_BoardInfo_t BoardInfo) {
    return
        BoardInfo.FamilyCode == CAEN_DGTZ_XX751_FAMILY_CODE ||
        BoardInfo.FamilyCode == CAEN_DGTZ_XX730_FAMILY_CODE ||
        BoardInfo.FamilyCode == CAEN_DGTZ_XX725_FAMILY_CODE;
}

/*! \brief   Write the event data on x742 boards into the output files
*
*   \param   WDrun Pointer to the WaveDumpRun data structure
*   \param   WDcfg Pointer to the WaveDumpConfig data structure
*   \param   EventInfo Pointer to the EventInfo data structure
*   \param   Event Pointer to the Event to write
*/
void calibrate(int handle, WaveDumpRun_t *WDrun, CAEN_DGTZ_BoardInfo_t BoardInfo) {
    printf("\n");
    if (BoardSupportsCalibration(BoardInfo)) {
        if (WDrun->AcqRun == 0) {
            int32_t ret = CAEN_DGTZ_Calibrate(handle);
            if (ret == CAEN_DGTZ_Success) {
                printf("ADC Calibration check: the board is calibrated.\n");
            }
            else {
                printf("ADC Calibration failed. CAENDigitizer ERR %d\n", ret);
            }
            printf("\n");
        }
        else {
            printf("Can't run ADC calibration while acquisition is running.\n");
        }
    }
    else {
        printf("ADC Calibration not needed for this board family.\n");
    }
}


/*! \fn      void Calibrate_XX740_DC_Offset(int handle, WaveDumpConfig_t WDcfg, CAEN_DGTZ_BoardInfo_t BoardInfo)
*   \brief   calibrates DAC of enabled channel groups (only if BASELINE_SHIFT is in use)
*
*   \param   handle   Digitizer handle
*   \param   WDcfg:   Pointer to the WaveDumpConfig_t data structure
*   \param   BoardInfo: structure with the board info
*/
void Calibrate_XX740_DC_Offset(int handle, WaveDumpConfig_t *WDcfg, CAEN_DGTZ_BoardInfo_t BoardInfo){
	float cal[MAX_CH];
	float offset[MAX_CH] = { 0 };
	int i = 0, acq = 0, k = 0, p=0, g = 0;
	for (i = 0; i < MAX_CH; i++)
		cal[i] = 1;
	CAEN_DGTZ_ErrorCode ret;
	CAEN_DGTZ_AcqMode_t mem_mode;
	uint32_t  AllocatedSize;

	ERROR_CODES ErrCode = ERR_NONE;
	uint32_t BufferSize;
	CAEN_DGTZ_EventInfo_t       EventInfo;
	char *buffer = NULL;
	char *EventPtr = NULL;
	CAEN_DGTZ_UINT16_EVENT_t    *Event16 = NULL;

	float avg_value[NPOINTS][MAX_CH] = { 0 };
	uint32_t dc[NPOINTS] = { 25,75 }; //test values (%)
	uint32_t groupmask = 0;

	ret = CAEN_DGTZ_GetAcquisitionMode(handle, &mem_mode);//chosen value stored
	if (ret)
		printf("Error trying to read acq mode!!\n");
	ret = CAEN_DGTZ_SetAcquisitionMode(handle, CAEN_DGTZ_SW_CONTROLLED);
	if (ret)
		printf("Error trying to set acq mode!!\n");
	ret = CAEN_DGTZ_SetExtTriggerInputMode(handle, CAEN_DGTZ_TRGMODE_DISABLED);
	if (ret)
		printf("Error trying to set ext trigger!!\n");
	ret = CAEN_DGTZ_SetMaxNumEventsBLT(handle, 1);
	if (ret)
		printf("Warning: error setting max BLT number\n");
	ret = CAEN_DGTZ_SetDecimationFactor(handle, 1);
	if (ret)
		printf("Error trying to set decimation factor!!\n");
	for (g = 0; g< (int32_t)BoardInfo.Channels; g++) //BoardInfo.Channels is number of groups for x740 boards
		groupmask |= (1 << g);
	ret = CAEN_DGTZ_SetGroupSelfTrigger(handle, CAEN_DGTZ_TRGMODE_DISABLED, groupmask);
	if (ret)
		printf("Error disabling self trigger\n");
	ret = CAEN_DGTZ_SetGroupEnableMask(handle, groupmask);
	if (ret)
		printf("Error enabling channel groups.\n");
	///malloc
	ret = CAEN_DGTZ_MallocReadoutBuffer(handle, &buffer, &AllocatedSize);
	if (ret) {
		ErrCode = ERR_MALLOC;
		goto QuitProgram;
	}

	ret = CAEN_DGTZ_AllocateEvent(handle, (void**)&Event16);
	if (ret != CAEN_DGTZ_Success) {
		ErrCode = ERR_MALLOC;
		goto QuitProgram;
	}

	printf("Starting DAC calibration...\n");

	for (p = 0; p < NPOINTS; p++){
		for (i = 0; i < (int32_t)BoardInfo.Channels; i++) { //BoardInfo.Channels is number of groups for x740 boards
				ret = CAEN_DGTZ_SetGroupDCOffset(handle, (uint32_t)i, (uint32_t)((float)(abs(dc[p] - 100))*(655.35)));
				if (ret)
					printf("Error setting group %d test offset\n", i);
		}
	#ifdef _WIN32
				Sleep(200);
	#else
				usleep(200000);
	#endif

	CAEN_DGTZ_ClearData(handle);

	ret = CAEN_DGTZ_SWStartAcquisition(handle);
	if (ret) {
		printf("Error starting X740 acquisition\n");
		goto QuitProgram;
	}

	int value[NACQS][MAX_CH] = { 0 }; //baseline values of the NACQS
	for (acq = 0; acq < NACQS; acq++) {
		CAEN_DGTZ_SendSWtrigger(handle);

		ret = CAEN_DGTZ_ReadData(handle, CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT, buffer, &BufferSize);
		if (ret) {
			ErrCode = ERR_READOUT;
			goto QuitProgram;
		}
		if (BufferSize == 0)
			continue;
		ret = CAEN_DGTZ_GetEventInfo(handle, buffer, BufferSize, 0, &EventInfo, &EventPtr);
		if (ret) {
			ErrCode = ERR_EVENT_BUILD;
			goto QuitProgram;
		}
		// decode the event //
		ret = CAEN_DGTZ_DecodeEvent(handle, EventPtr, (void**)&Event16);
		if (ret) {
			ErrCode = ERR_EVENT_BUILD;
			goto QuitProgram;
		}
		for (g = 0; g < (int32_t)BoardInfo.Channels; g++) {
				for (k = 1; k < 21; k++) //mean over 20 samples
					value[acq][g] += (int)(Event16->DataChannel[g * 8][k]);

				value[acq][g] = (value[acq][g] / 20);
		}

	}//for acq

	///check for clean baselines
	for (g = 0; g < (int32_t)BoardInfo.Channels; g++) {
			int max = 0;
			int mpp = 0;
			int size = (int)pow(2, (double)BoardInfo.ADC_NBits);
			int *freq = calloc(size, sizeof(int));
			//find the most probable value mpp
			for (k = 0; k < NACQS; k++) {
				if (value[k][g] > 0 && value[k][g] < size) {
					freq[value[k][g]]++;
					if (freq[value[k][g]] > max) {
						max = freq[value[k][g]];
						mpp = value[k][g];
					}
				}
			}
			free(freq);
			//discard values too far from mpp
			int ok = 0;
			for (k = 0; k < NACQS; k++) {
				if (value[k][g] >= (mpp - 5) && value[k][g] <= (mpp + 5)) {
					avg_value[p][g] = avg_value[p][g] + (float)value[k][g];
					ok++;
				}
			}
			avg_value[p][g] = (avg_value[p][g] / (float)ok)*100. / (float)size;
	}

	CAEN_DGTZ_SWStopAcquisition(handle);
	}//close for p

	for (g = 0; g < (int32_t)BoardInfo.Channels; g++) {
			cal[g] = ((float)(avg_value[1][g] - avg_value[0][g]) / (float)(dc[1] - dc[0]));
			offset[g] = (float)(dc[1] * avg_value[0][g] - dc[0] * avg_value[1][g]) / (float)(dc[1] - dc[0]);
			printf("Group %d DAC calibration ready.\n",g);
			printf("Cal %f   offset %f\n", cal[g], offset[g]);

			WDcfg->DAC_Calib.cal[g] = cal[g];
			WDcfg->DAC_Calib.offset[g] = offset[g];
	}

	CAEN_DGTZ_ClearData(handle);

	///free events e buffer
	CAEN_DGTZ_FreeReadoutBuffer(&buffer);

	CAEN_DGTZ_FreeEvent(handle, (void**)&Event16);

	ret |= CAEN_DGTZ_SetMaxNumEventsBLT(handle, WDcfg->NumEvents);
	ret |= CAEN_DGTZ_SetDecimationFactor(handle,WDcfg->DecimationFactor);
	ret |= CAEN_DGTZ_SetPostTriggerSize(handle, WDcfg->PostTrigger);
	ret |= CAEN_DGTZ_SetAcquisitionMode(handle, mem_mode);
	ret |= CAEN_DGTZ_SetExtTriggerInputMode(handle, WDcfg->ExtTriggerMode);
	ret |= CAEN_DGTZ_SetGroupEnableMask(handle, WDcfg->EnableMask);
	for (i = 0; i < BoardInfo.Channels; i++) {
		if (WDcfg->EnableMask & (1 << i))
			ret |= CAEN_DGTZ_SetGroupSelfTrigger(handle, WDcfg->ChannelTriggerMode[i], (1 << i));
	}
	if (ret)
		printf("Error setting recorded parameters\n");

	Save_DAC_Calibration_To_Flash(handle, *WDcfg, BoardInfo);

QuitProgram:
		if (ErrCode) {
			printf("\a%s\n", ErrMsg[ErrCode]);
#ifdef WIN32
			printf("Press a key to quit\n");
			getch();
#endif
		}
}


/*! \fn      void Set_relative_Threshold(int handle, WaveDumpConfig_t *WDcfg, CAEN_DGTZ_BoardInfo_t BoardInfo)
*   \brief   sets the threshold relative to the baseline (only if BASELINE_SHIFT is in use)
*
*   \param   handle   Digitizer handle
*   \param   WDcfg:   Pointer to the WaveDumpConfig_t data structure
*   \param   BoardInfo: structure with the board info
*/
void Set_relative_Threshold(int handle, WaveDumpConfig_t *WDcfg, CAEN_DGTZ_BoardInfo_t BoardInfo){
	int ch = 0, i = 0;

	//preliminary check: if baseline shift is not enabled for any channel quit
	int should_start = 0;
	for (ch = 0; ch < (int32_t)BoardInfo.Channels; ch++) {
		if (WDcfg->EnableMask & (1 << ch) && WDcfg->Version_used[ch] == 1) {
			should_start = 1;
			break;
		}
	}
	if (!should_start)
		return;

	CAEN_DGTZ_ErrorCode ret;
	uint32_t  AllocatedSize;
	ERROR_CODES ErrCode = ERR_NONE;
	uint32_t BufferSize;
	CAEN_DGTZ_EventInfo_t       EventInfo;
	char *buffer = NULL;
	char *EventPtr = NULL;
	CAEN_DGTZ_UINT16_EVENT_t    *Event16 = NULL;
	CAEN_DGTZ_UINT8_EVENT_t     *Event8 = NULL;
	uint32_t custom_posttrg = 50, dco, custom_thr;
	float expected_baseline;
	float dco_percent;
	int baseline[MAX_CH] = { 0 }, size = 0, samples = 0;
	int no_self_triggered_event[MAX_CH] = {0};
	int sw_trigger_needed = 0;
	int event_ch;

	///malloc
	ret = CAEN_DGTZ_MallocReadoutBuffer(handle, &buffer, &AllocatedSize);
	if (ret) {
		ErrCode = ERR_MALLOC;
		goto QuitProgram;
	}
	if (WDcfg->Nbit == 8)
		ret = CAEN_DGTZ_AllocateEvent(handle, (void**)&Event8);
	else {
		ret = CAEN_DGTZ_AllocateEvent(handle, (void**)&Event16);
	}
	if (ret != CAEN_DGTZ_Success) {
		ErrCode = ERR_MALLOC;
		goto QuitProgram;
	}

	//some custom settings
	ret = CAEN_DGTZ_SetPostTriggerSize(handle, custom_posttrg);
	if (ret) {
		printf("Threshold calc failed. Error trying to set post trigger!!\n");
		return;
	}
	//try to set a small threshold to get a self triggered event
	for (ch = 0; ch < (int32_t)BoardInfo.Channels; ch++) {
		if (WDcfg->EnableMask & (1 << ch) && WDcfg->Version_used[ch] == 1) {
			if (BoardInfo.FamilyCode == CAEN_DGTZ_XX740_FAMILY_CODE)
				ret = CAEN_DGTZ_GetGroupDCOffset(handle, ch, &dco);
			else
				ret = CAEN_DGTZ_GetChannelDCOffset(handle, ch, &dco);
			if (ret) {
				printf("Threshold calc failed. Error trying to get DCoffset values!!\n");
				return;
			}
			dco_percent = (float)dco / 65535.;
			expected_baseline = pow(2, (double)BoardInfo.ADC_NBits) * (1.0 - dco_percent);

			custom_thr = (WDcfg->PulsePolarity[ch] == CAEN_DGTZ_PulsePolarityPositive) ? ((uint32_t)expected_baseline + 100) : ((uint32_t)expected_baseline - 100);
			
			if (BoardInfo.FamilyCode == CAEN_DGTZ_XX740_FAMILY_CODE)
				ret = CAEN_DGTZ_SetGroupTriggerThreshold(handle, ch, custom_thr);
			else
				ret = CAEN_DGTZ_SetChannelTriggerThreshold(handle, ch, custom_thr);
			if (ret) {
				printf("Threshold calc failed. Error trying to set custom threshold value!!\n");
				return;
			}
		}
	}

	CAEN_DGTZ_SWStartAcquisition(handle);
#ifdef _WIN32
	Sleep(300);
#else
	usleep(300000);
#endif

	ret = CAEN_DGTZ_ReadData(handle, CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT, buffer, &BufferSize);
	if (ret) {
		ErrCode = ERR_READOUT;
		goto QuitProgram;
	}
	//we have some self-triggered event 
	if (BufferSize > 0) {
		ret = CAEN_DGTZ_GetEventInfo(handle, buffer, BufferSize, 0, &EventInfo, &EventPtr);
		if (ret) {
			ErrCode = ERR_EVENT_BUILD;
			goto QuitProgram;
		}
		// decode the event //
		if (WDcfg->Nbit == 8)
			ret = CAEN_DGTZ_DecodeEvent(handle, EventPtr, (void**)&Event8);
		else
			ret = CAEN_DGTZ_DecodeEvent(handle, EventPtr, (void**)&Event16);

		if (ret) {
			ErrCode = ERR_EVENT_BUILD;
			goto QuitProgram;
		}

		for (ch = 0; ch < (int32_t)BoardInfo.Channels; ch++) {
			if (WDcfg->EnableMask & (1 << ch) && WDcfg->Version_used[ch] == 1) {
				event_ch = (BoardInfo.FamilyCode == CAEN_DGTZ_XX740_FAMILY_CODE) ? (ch * 8) : ch;//for x740 boards shift to channel 0 of next group
				size = (WDcfg->Nbit == 8) ? Event8->ChSize[event_ch] : Event16->ChSize[event_ch];
				if (size == 0) {//no data from channel ch
					no_self_triggered_event[ch] = 1;
					sw_trigger_needed = 1;
					continue;
				}
				else {//use only one tenth of the pre-trigger samples to calculate the baseline
					samples = (int)(size * ((100 - custom_posttrg) / 2) / 100);

					for (i = 0; i < samples; i++) //mean over some pre trigger samples
					{
						if (WDcfg->Nbit == 8)
							baseline[ch] += (int)(Event8->DataChannel[event_ch][i]);
						else
							baseline[ch] += (int)(Event16->DataChannel[event_ch][i]);
					}
					baseline[ch] = (baseline[ch] / samples);
				}

				if (WDcfg->PulsePolarity[ch] == CAEN_DGTZ_PulsePolarityPositive)
					WDcfg->Threshold[ch] = (uint32_t)baseline[ch] + thr_file[ch];
				else 	if (WDcfg->PulsePolarity[ch] == CAEN_DGTZ_PulsePolarityNegative)
					WDcfg->Threshold[ch] = (uint32_t)baseline[ch] - thr_file[ch];

				if (WDcfg->Threshold[ch] < 0) WDcfg->Threshold[ch] = 0;
				size = (int)pow(2, (double)BoardInfo.ADC_NBits);
				if (WDcfg->Threshold[ch] > (uint32_t)size) WDcfg->Threshold[ch] = size;

				if (BoardInfo.FamilyCode == CAEN_DGTZ_XX740_FAMILY_CODE)
					ret = CAEN_DGTZ_SetGroupTriggerThreshold(handle, ch, WDcfg->Threshold[ch]);
				else
					ret = CAEN_DGTZ_SetChannelTriggerThreshold(handle, ch, WDcfg->Threshold[ch]);
				if (ret)
					printf("Warning: error setting ch %d corrected threshold\n", ch);
			}
		}
	}
	else {
		sw_trigger_needed = 1;
		for(ch = 0; ch < (int32_t)BoardInfo.Channels; ch++)
			no_self_triggered_event[ch] = 1;
	}

	CAEN_DGTZ_ClearData(handle);

	//if from some channels we had no self triggered event, we send a software trigger
	if (sw_trigger_needed) {
		CAEN_DGTZ_SendSWtrigger(handle);

		ret = CAEN_DGTZ_ReadData(handle, CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT, buffer, &BufferSize);
		if (ret) {
			ErrCode = ERR_READOUT;
			goto QuitProgram;
		}
		if (BufferSize == 0)
			return;

		ret = CAEN_DGTZ_GetEventInfo(handle, buffer, BufferSize, 0, &EventInfo, &EventPtr);
		if (ret) {
			ErrCode = ERR_EVENT_BUILD;
			goto QuitProgram;
		}
		// decode the event //
		if (WDcfg->Nbit == 8)
			ret = CAEN_DGTZ_DecodeEvent(handle, EventPtr, (void**)&Event8);
		else
			ret = CAEN_DGTZ_DecodeEvent(handle, EventPtr, (void**)&Event16);

		if (ret) {
			ErrCode = ERR_EVENT_BUILD;
			goto QuitProgram;
		}

		for (ch = 0; ch < (int32_t)BoardInfo.Channels; ch++) {
			if (WDcfg->EnableMask & (1 << ch) && WDcfg->Version_used[ch] == 1) {
				event_ch = (BoardInfo.FamilyCode == CAEN_DGTZ_XX740_FAMILY_CODE) ? (ch * 8) : ch;//for x740 boards shift to channel 0 of next group
				size = (WDcfg->Nbit == 8) ? Event8->ChSize[event_ch] : Event16->ChSize[event_ch];
				if (!no_self_triggered_event[ch])//we already have a good baseline for channel ch
					continue;

				//use some samples to calculate the baseline
				for (i = 1; i < 11; i++){ //mean over 10 samples
					if (WDcfg->Nbit == 8)
						baseline[ch] += (int)(Event8->DataChannel[event_ch][i]);
					else
						baseline[ch] += (int)(Event16->DataChannel[event_ch][i]);
				}
					baseline[ch] = (baseline[ch] / 10);
			}

			if (WDcfg->PulsePolarity[ch] == CAEN_DGTZ_PulsePolarityPositive)
				WDcfg->Threshold[ch] = (uint32_t)baseline[ch] + thr_file[ch];
			else 	if (WDcfg->PulsePolarity[ch] == CAEN_DGTZ_PulsePolarityNegative)
				WDcfg->Threshold[ch] = (uint32_t)baseline[ch] - thr_file[ch];

			if (WDcfg->Threshold[ch] < 0) WDcfg->Threshold[ch] = 0;
				size = (int)pow(2, (double)BoardInfo.ADC_NBits);
			if (WDcfg->Threshold[ch] > (uint32_t)size) WDcfg->Threshold[ch] = size;

			if (BoardInfo.FamilyCode == CAEN_DGTZ_XX740_FAMILY_CODE)
				ret = CAEN_DGTZ_SetGroupTriggerThreshold(handle, ch, WDcfg->Threshold[ch]);
			else
				ret = CAEN_DGTZ_SetChannelTriggerThreshold(handle, ch, WDcfg->Threshold[ch]);
			if (ret)
					printf("Warning: error setting ch %d corrected threshold\n", ch);
		}
	}//end sw trigger event analysis

	CAEN_DGTZ_SWStopAcquisition(handle);

	//reset posttrigger
	ret = CAEN_DGTZ_SetPostTriggerSize(handle, WDcfg->PostTrigger);
	if (ret)
		printf("Error resetting post trigger.\n");

	CAEN_DGTZ_ClearData(handle);

	CAEN_DGTZ_FreeReadoutBuffer(&buffer);
	if (WDcfg->Nbit == 8)
		CAEN_DGTZ_FreeEvent(handle, (void**)&Event8);
	else
		CAEN_DGTZ_FreeEvent(handle, (void**)&Event16);


QuitProgram:
	if (ErrCode) {
		printf("\a%s\n", ErrMsg[ErrCode]);
#ifdef WIN32
		printf("Press a key to quit\n");
		getch();
#endif
	}
}

/*! \fn      void Calibrate_DC_Offset(int handle, WaveDumpConfig_t WDcfg, CAEN_DGTZ_BoardInfo_t BoardInfo)
*   \brief   calibrates DAC of enabled channels (only if BASELINE_SHIFT is in use)
*
*   \param   handle   Digitizer handle
*   \param   WDcfg:   Pointer to the WaveDumpConfig_t data structure
*   \param   BoardInfo: structure with the board info
*/
void Calibrate_DC_Offset(int handle, WaveDumpConfig_t *WDcfg, CAEN_DGTZ_BoardInfo_t BoardInfo){
	float cal[MAX_CH];
	float offset[MAX_CH] = { 0 };
	int i = 0, k = 0, p = 0, acq = 0, ch = 0;
	for (i = 0; i < MAX_CH; i++)
		cal[i] = 1;
	CAEN_DGTZ_ErrorCode ret;
	CAEN_DGTZ_AcqMode_t mem_mode;
	uint32_t  AllocatedSize;

	ERROR_CODES ErrCode = ERR_NONE;
	uint32_t BufferSize;
	CAEN_DGTZ_EventInfo_t       EventInfo;
	char *buffer = NULL;
	char *EventPtr = NULL;
	CAEN_DGTZ_UINT16_EVENT_t    *Event16 = NULL;
	CAEN_DGTZ_UINT8_EVENT_t     *Event8 = NULL;

	float avg_value[NPOINTS][MAX_CH] = { 0 };
	uint32_t dc[NPOINTS] = { 25,75 }; //test values (%)
	uint32_t chmask = 0;

	ret = CAEN_DGTZ_GetAcquisitionMode(handle, &mem_mode);//chosen value stored
	if (ret)
		printf("Error trying to read acq mode!!\n");
	ret = CAEN_DGTZ_SetAcquisitionMode(handle, CAEN_DGTZ_SW_CONTROLLED);
	if (ret)
		printf("Error trying to set acq mode!!\n");
	ret = CAEN_DGTZ_SetExtTriggerInputMode(handle, CAEN_DGTZ_TRGMODE_DISABLED);
	if (ret)
		printf("Error trying to set ext trigger!!\n");
	for (ch = 0; ch < (int32_t)BoardInfo.Channels; ch++)
		chmask |= (1 << ch);
	ret = CAEN_DGTZ_SetChannelSelfTrigger(handle, CAEN_DGTZ_TRGMODE_DISABLED, chmask);
	if (ret)
		printf("Warning: error disabling channels self trigger\n");
	ret = CAEN_DGTZ_SetChannelEnableMask(handle, chmask);
	if (ret)
		printf("Warning: error enabling channels.\n");
	ret = CAEN_DGTZ_SetMaxNumEventsBLT(handle, 1);
	if (ret)
		printf("Warning: error setting max BLT number\n");
	if (BoardInfo.FamilyCode == CAEN_DGTZ_XX740_FAMILY_CODE || BoardInfo.FamilyCode == CAEN_DGTZ_XX724_FAMILY_CODE) {
		ret = CAEN_DGTZ_SetDecimationFactor(handle, 1);
		if (ret)
			printf("Error trying to set decimation factor!!\n");
	}

	///malloc
	ret = CAEN_DGTZ_MallocReadoutBuffer(handle, &buffer, &AllocatedSize);
	if (ret) {
		ErrCode = ERR_MALLOC;
		goto QuitProgram;
	}
	if (WDcfg->Nbit == 8)
		ret = CAEN_DGTZ_AllocateEvent(handle, (void**)&Event8);
	else {
		ret = CAEN_DGTZ_AllocateEvent(handle, (void**)&Event16);
	}
	if (ret != CAEN_DGTZ_Success) {
		ErrCode = ERR_MALLOC;
		goto QuitProgram;
	}

	printf("Starting DAC calibration...\n");
	
	for (p = 0; p < NPOINTS; p++){
		//set new dco  test value to all channels
		for (ch = 0; ch < (int32_t)BoardInfo.Channels; ch++) {
				ret = CAEN_DGTZ_SetChannelDCOffset(handle, (uint32_t)ch, (uint32_t)((float)(abs(dc[p] - 100))*(655.35)));
				if (ret)
					printf("Error setting ch %d test offset\n", ch);
		}
#ifdef _WIN32
					Sleep(200);
#else
					usleep(200000);
#endif
		CAEN_DGTZ_ClearData(handle);

		ret = CAEN_DGTZ_SWStartAcquisition(handle);
		if (ret){
			printf("Error starting acquisition\n");
			goto QuitProgram;
		}
		
		int value[NACQS][MAX_CH] = { 0 };//baseline values of the NACQS
		for (acq = 0; acq < NACQS; acq++){
			CAEN_DGTZ_SendSWtrigger(handle);

			ret = CAEN_DGTZ_ReadData(handle, CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT, buffer, &BufferSize);
			if (ret) {
					ErrCode = ERR_READOUT;
					goto QuitProgram;
			}
			if (BufferSize == 0)
				continue;
			ret = CAEN_DGTZ_GetEventInfo(handle, buffer, BufferSize, 0, &EventInfo, &EventPtr);
			if (ret) {
					ErrCode = ERR_EVENT_BUILD;
					goto QuitProgram;
			}
			// decode the event //
			if (WDcfg->Nbit == 8)
				ret = CAEN_DGTZ_DecodeEvent(handle, EventPtr, (void**)&Event8);
				else
				ret = CAEN_DGTZ_DecodeEvent(handle, EventPtr, (void**)&Event16);

			if (ret) {
				ErrCode = ERR_EVENT_BUILD;
				goto QuitProgram;
			}

			for (ch = 0; ch < (int32_t)BoardInfo.Channels; ch++){
					for (i = 1; i < 21; i++) //mean over 20 samples
					{
						if (WDcfg->Nbit == 8)
							value[acq][ch] += (int)(Event8->DataChannel[ch][i]);
						else
							value[acq][ch] += (int)(Event16->DataChannel[ch][i]);
					}
					value[acq][ch] = (value[acq][ch] / 20);
			}

		}//for acq

		///check for clean baselines
		for (ch = 0; ch < (int32_t)BoardInfo.Channels; ch++) {
				int max = 0, ok = 0;
				int mpp = 0;
				int size = (int)pow(2, (double)BoardInfo.ADC_NBits);
				int *freq = calloc(size, sizeof(int));

				//find most probable value mpp
				for (k = 0; k < NACQS; k++) {
					if (value[k][ch] > 0 && value[k][ch] < size) {
						freq[value[k][ch]]++;
						if (freq[value[k][ch]] > max) {
							max = freq[value[k][ch]];
							mpp = value[k][ch];
						}
					}
				}
				free(freq);
				//discard values too far from mpp
				for (k = 0; k < NACQS; k++) {
					if (value[k][ch] >= (mpp - 5) && value[k][ch] <= (mpp + 5)) {
						avg_value[p][ch] = avg_value[p][ch] + (float)value[k][ch];
						ok++;
					}
				}
				//calculate final best average value
				avg_value[p][ch] = (avg_value[p][ch] / (float)ok)*100. / (float)size;
		}

		CAEN_DGTZ_SWStopAcquisition(handle);
	}//close for p

	for (ch = 0; ch < (int32_t)BoardInfo.Channels; ch++) {
			cal[ch] = ((float)(avg_value[1][ch] - avg_value[0][ch]) / (float)(dc[1] - dc[0]));
			offset[ch] = (float)(dc[1] * avg_value[0][ch] - dc[0] * avg_value[1][ch]) / (float)(dc[1] - dc[0]);
			printf("Channel %d DAC calibration ready.\n", ch);
			//printf("Channel %d --> Cal %f   offset %f\n", ch, cal[ch], offset[ch]);

			WDcfg->DAC_Calib.cal[ch] = cal[ch];
			WDcfg->DAC_Calib.offset[ch] = offset[ch];
	}

	CAEN_DGTZ_ClearData(handle);

	 ///free events e buffer
	CAEN_DGTZ_FreeReadoutBuffer(&buffer);
	if (WDcfg->Nbit == 8)
		CAEN_DGTZ_FreeEvent(handle, (void**)&Event8);
	else
		CAEN_DGTZ_FreeEvent(handle, (void**)&Event16);

	//reset settings
	ret |= CAEN_DGTZ_SetMaxNumEventsBLT(handle, WDcfg->NumEvents);
	ret |= CAEN_DGTZ_SetPostTriggerSize(handle, WDcfg->PostTrigger);
	ret |= CAEN_DGTZ_SetAcquisitionMode(handle, mem_mode);
	ret |= CAEN_DGTZ_SetExtTriggerInputMode(handle, WDcfg->ExtTriggerMode);
	ret |= CAEN_DGTZ_SetChannelEnableMask(handle, WDcfg->EnableMask);
	if (BoardInfo.FamilyCode == CAEN_DGTZ_XX740_FAMILY_CODE || BoardInfo.FamilyCode == CAEN_DGTZ_XX724_FAMILY_CODE)
		ret |= CAEN_DGTZ_SetDecimationFactor(handle, WDcfg->DecimationFactor);
	if (ret)
		printf("Error resetting some parameters after DAC calibration\n");

	//reset self trigger mode settings
	if (BoardInfo.FamilyCode == CAEN_DGTZ_XX730_FAMILY_CODE || BoardInfo.FamilyCode == CAEN_DGTZ_XX725_FAMILY_CODE) {
		// channel pair settings for x730 boards
		for (i = 0; i < WDcfg->Nch; i += 2) {
			if (WDcfg->EnableMask & (0x3 << i)) {
				CAEN_DGTZ_TriggerMode_t mode = WDcfg->ChannelTriggerMode[i];
				uint32_t pair_chmask = 0;

				if (WDcfg->ChannelTriggerMode[i] != CAEN_DGTZ_TRGMODE_DISABLED) {
					if (WDcfg->ChannelTriggerMode[i + 1] == CAEN_DGTZ_TRGMODE_DISABLED)
						pair_chmask = (0x1 << i);
					else
						pair_chmask = (0x3 << i);
				}
				else {
					mode = WDcfg->ChannelTriggerMode[i + 1];
					pair_chmask = (0x2 << i);
				}

				pair_chmask &= WDcfg->EnableMask;
				ret |= CAEN_DGTZ_SetChannelSelfTrigger(handle, mode, pair_chmask);
			}
		}
	}
	else {
		for (i = 0; i < WDcfg->Nch; i++) {
			if (WDcfg->EnableMask & (1 << i))
				ret |= CAEN_DGTZ_SetChannelSelfTrigger(handle, WDcfg->ChannelTriggerMode[i], (1 << i));
		}
	}
	if (ret)
		printf("Error resetting self trigger mode after DAC calibration\n");

	Save_DAC_Calibration_To_Flash(handle, *WDcfg, BoardInfo);

QuitProgram:
	if (ErrCode) {
		printf("\a%s\n", ErrMsg[ErrCode]);
#ifdef WIN32
		printf("Press a key to quit\n");
		getch();
#endif
	}

}

/*! \fn      void Set_calibrated_DCO(int handle, WaveDumpConfig_t *WDcfg, CAEN_DGTZ_BoardInfo_t BoardInfo)
*   \brief   sets the calibrated DAC value using calibration data (only if BASELINE_SHIFT is in use)
*
*   \param   handle   Digitizer handle
*   \param   WDcfg:   Pointer to the WaveDumpConfig_t data structure
*   \param   BoardInfo: structure with the board info
*/
int Set_calibrated_DCO(int handle, int ch, WaveDumpConfig_t *WDcfg, CAEN_DGTZ_BoardInfo_t BoardInfo) {
	int ret = CAEN_DGTZ_Success;
	if (WDcfg->Version_used[ch] == 0) //old DC_OFFSET config, skip calibration
		return ret;
	if (WDcfg->PulsePolarity[ch] == CAEN_DGTZ_PulsePolarityPositive) {
		WDcfg->DCoffset[ch] = (uint32_t)((float)(fabs((((float)dc_file[ch] - WDcfg->DAC_Calib.offset[ch]) / WDcfg->DAC_Calib.cal[ch]) - 100.))*(655.35));
		if (WDcfg->DCoffset[ch] > 65535) WDcfg->DCoffset[ch] = 65535;
		if (WDcfg->DCoffset[ch] < 0) WDcfg->DCoffset[ch] = 0;
	}
	else if (WDcfg->PulsePolarity[ch] == CAEN_DGTZ_PulsePolarityNegative) {
		WDcfg->DCoffset[ch] = (uint32_t)((float)(fabs(((fabs(dc_file[ch] - 100.) - WDcfg->DAC_Calib.offset[ch]) / WDcfg->DAC_Calib.cal[ch]) - 100.))*(655.35));
		if (WDcfg->DCoffset[ch] < 0) WDcfg->DCoffset[ch] = 0;
		if (WDcfg->DCoffset[ch] > 65535) WDcfg->DCoffset[ch] = 65535;
	}

	if (BoardInfo.FamilyCode == CAEN_DGTZ_XX740_FAMILY_CODE) {
		ret = CAEN_DGTZ_SetGroupDCOffset(handle, (uint32_t)ch, WDcfg->DCoffset[ch]);
		if (ret)
			printf("Error setting group %d offset\n", ch);
	}
	else {
		ret = CAEN_DGTZ_SetChannelDCOffset(handle, (uint32_t)ch, WDcfg->DCoffset[ch]);
		if (ret)
			printf("Error setting channel %d offset\n", ch);
	}

	return ret;
}

void get_baselines(float data[WF_SIZE][32][1024], float *baselines, int n, int chmask, int nsamples)
{
    int i, j, k;

    for (j = 0; j < 32; j++)
        baselines[j] = 0;

    for (i = 0; i < n; i++) {
        for (j = 0; j < 32; j++) {
            if (!(chmask & (1 << j))) {
                baselines[j] = 0;
                continue;
            }

            for (k = 0; k < nsamples; k++) {
                baselines[j] += data[i][j][k];
            }
        }
    }

    for (j = 0; j < 32; j++) {
        baselines[j] /= n*nsamples;
    }
}

/* Write events to an HDF5 output file. If the file doesn't exist, it will be
 * created and attributes such as the record_length, post_trigger, barcode, and
 * voltage will be written.
 *
 * The output file is set up to use gzip compression, but right now it's turned
 * off. The reason is that with the full compression (gzip level 9), it was too
 * slow and so the data taking time was dominated by the compression. There is
 * probably some way to speed this up, and if so, it can be re-enabled. */
int add_to_output_file(char *filename, float data[WF_SIZE][32][1024], int n, int chmask, int nsamples, WaveDumpConfig_t *WDcfg)
{
    hid_t file, space, dset, dcpl, mem_space, file_space;
    herr_t status;
    htri_t avail;
    int ndims;
    hsize_t dims[2], chunk[2], extdims[2], maxdims[2];
    char dset_name[256];
    hsize_t start[2], count[2];
    int i, j, k;
    unsigned int filter_info;
    hid_t aid, attr;

    chunk[0] = 100;
    chunk[1] = 1024;

    /* Check if file exists. */
    if (access(filename, F_OK) != 0) {
        /* File doesn't exist. Create it. */

        /* Check if gzip compression is available and can be used for both
         * compression and decompression.  Normally we do not perform error
         * checking in these examples for the sake of clarity, but in this
         * case we will make an exception because this filter is an
         * optional part of the hdf5 library. */
        avail = H5Zfilter_avail(H5Z_FILTER_DEFLATE);
        if (!avail) {
            fprintf(stderr, "gzip filter not available.\n");
            return 1;
        }

        status = H5Zget_filter_info(H5Z_FILTER_DEFLATE, &filter_info);

        if (!(filter_info & H5Z_FILTER_CONFIG_ENCODE_ENABLED) || !(filter_info & H5Z_FILTER_CONFIG_DECODE_ENABLED)) {
            fprintf(stderr, "gzip filter not available for encoding and decoding.\n");
            return 1;
        }

        /* Create a new file using the default properties. */
        file = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);

        aid = H5Screate(H5S_SCALAR);
        attr = H5Acreate2(file, "record_length", H5T_NATIVE_INT, aid, H5P_DEFAULT, H5P_DEFAULT);
        int record_length = WDcfg->RecordLength;
        int ret = H5Awrite(attr, H5T_NATIVE_INT, &record_length);

        if (ret) {
            fprintf(stderr, "failed to write record length to hdf5 file.\n");
            return 1;
        }

        H5Sclose(aid);
        H5Aclose(attr);

        aid = H5Screate(H5S_SCALAR);
        attr = H5Acreate2(file, "post_trigger", H5T_NATIVE_INT, aid, H5P_DEFAULT, H5P_DEFAULT);
        int post_trigger = WDcfg->PostTrigger;
        ret = H5Awrite(attr, H5T_NATIVE_INT, &post_trigger);

        if (ret) {
            fprintf(stderr, "failed to write post trigger to hdf5 file.\n");
            return 1;
        }

        H5Sclose(aid);
        H5Aclose(attr);

        aid = H5Screate(H5S_SCALAR);
        attr = H5Acreate2(file, "drs4_frequency", H5T_NATIVE_INT, aid, H5P_DEFAULT, H5P_DEFAULT);
        int drs4_frequency = 0;
        switch (WDcfg->DRS4Frequency) {
        case 0:
            drs4_frequency = 5000;
            break;
        case 1:
            drs4_frequency = 2500;
            break;
        case 2:
            drs4_frequency = 1000;
            break;
        case 3:
            drs4_frequency = 750;
            break;
        default:
            fprintf(stderr, "unknown DRS4 frequency %i\n", WDcfg->DRS4Frequency);
            return 1;
        }
        ret = H5Awrite(attr, H5T_NATIVE_INT, &drs4_frequency);

        if (ret) {
            fprintf(stderr, "failed to write DRS4 frequency to hdf5 file.\n");
            return 1;
        }

        H5Sclose(aid);
        H5Aclose(attr);

        aid = H5Screate(H5S_SCALAR);
        attr = H5Acreate2(file, "barcode", H5T_NATIVE_INT, aid, H5P_DEFAULT, H5P_DEFAULT);
        ret = H5Awrite(attr, H5T_NATIVE_INT, &WDcfg->barcode);

        if (ret) {
            fprintf(stderr, "failed to write barcode to hdf5 file.\n");
            return 1;
        }

        H5Sclose(aid);
        H5Aclose(attr);

        aid = H5Screate(H5S_SCALAR);
        attr = H5Acreate2(file, "voltage", H5T_NATIVE_FLOAT, aid, H5P_DEFAULT, H5P_DEFAULT);
        ret = H5Awrite(attr, H5T_NATIVE_FLOAT, &WDcfg->voltage);

        if (ret) {
            fprintf(stderr, "failed to write voltage to hdf5 file.\n");
            return 1;
        }

        H5Sclose(aid);
        H5Aclose(attr);

        aid = H5Screate(H5S_SCALAR);
        hid_t atype = H5Tcopy(H5T_C_S1);
        H5Tset_size(atype, 100);
        H5Tset_strpad(atype, H5T_STR_NULLTERM);
        attr = H5Acreate2(file, "git_sha1", atype, aid, H5P_DEFAULT, H5P_DEFAULT);
        ret = H5Awrite(attr, atype, GitSHA1());

        if (ret) {
            fprintf(stderr, "failed to write git_sha1 to hdf5 file.\n");
            return 1;
        }

        printf("git sha1 = %s\n", GitSHA1());

        H5Sclose(aid);
        H5Aclose(attr);
        H5Tclose(atype);

        aid = H5Screate(H5S_SCALAR);
        atype = H5Tcopy(H5T_C_S1);
        H5Tset_size(atype, 100);
        H5Tset_strpad(atype, H5T_STR_NULLTERM);
        attr = H5Acreate2(file, "git_dirty", atype, aid, H5P_DEFAULT, H5P_DEFAULT);
        ret = H5Awrite(attr, atype, GitDirty());

        if (ret) {
            fprintf(stderr, "failed to write git_sha1 to hdf5 file.\n");
            return 1;
        }

        H5Sclose(aid);
        H5Aclose(attr);
        H5Tclose(atype);

        for (i = 0; i < 32; i++) {
            if (!(chmask & (1 << i))) continue;
            /* Create dataspace with unlimited dimensions. */
            maxdims[0] = H5S_UNLIMITED;
            maxdims[1] = H5S_UNLIMITED;
            dims[0] = n;
            dims[1] = nsamples;
            space = H5Screate_simple(2, dims, maxdims);

            /* Create the dataset creation property list, add the gzip compression
             * filter and set the chunk size. */
            dcpl = H5Pcreate(H5P_DATASET_CREATE);

            /* Set gzip level. Right now we set it to 0 (no compression), since
             * it's *much* faster to write. In the future if we can make it
             * faster and/or need smaller files you can increase this to 9. */
            status = H5Pset_deflate(dcpl, 0);
            status = H5Pset_chunk(dcpl, 2, chunk);

            sprintf(dset_name, "ch%i", i);

            float *wdata = malloc(n*nsamples*sizeof(float));

            for (j = 0; j < n; j++)
                for (k = 0; k < nsamples; k++)
                    wdata[j*nsamples + k] = data[j][i][k];

            /* Create the compressed unlimited dataset. */
            dset = H5Dcreate(file, dset_name, H5T_NATIVE_FLOAT, space, H5P_DEFAULT, dcpl, H5P_DEFAULT);

            /* Write the data to the dataset. */
            status = H5Dwrite(dset, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, wdata);

            free(wdata);

            /* Close and release resources. */
            status = H5Pclose(dcpl);
            status = H5Dclose(dset);
            status = H5Sclose(space);

            if (status) {
                fprintf(stderr, "error closing hdf5 resources.\n");
                return 1;
            }
        }
        status = H5Fclose(file);

        if (status) {
            fprintf(stderr, "error closing hdf5 file.\n");
            return 1;
        }

        return 0;
    }

    /* Extend the dataset.
     *
     * Note: This is really confusing. I couldn't have done this without the
     * stack overflow answer here:
     * https://stackoverflow.com/questions/15379399/writing-appending-arrays-of-float-to-the-only-dataset-in-hdf5-file-in-c. */
    file = H5Fopen(filename, H5F_ACC_RDWR, H5P_DEFAULT);
    for (i = 0; i < 32; i++) {
        if (!(chmask & (1 << i))) continue;

        sprintf(dset_name, "ch%i", i);
        if ((dset = H5Dopen(file, dset_name, H5P_DEFAULT)) < 0) {
            fprintf(stderr, "couldn't find dataset for %s. skipping...\n", dset_name);
        }

        /* Get dataspace and allocate memory for read buffer. This is a two
         * dimensional dataset so the dynamic allocation must be done in steps. */
        file_space = H5Dget_space(dset);
        ndims = H5Sget_simple_extent_dims(file_space, dims, NULL);

        extdims[0] = n;
        extdims[1] = nsamples;

        /* Memory dataspace resized. */
        mem_space = H5Screate_simple(ndims, extdims, NULL);

        /* Update dims so it now has the new size. */
        dims[0] += extdims[0];

        /* Extend the dataset. */
        status = H5Dset_extent(dset, dims);

        if (status) {
            fprintf(stderr, "error extending dataset.\n");
            return 1;
        }

        /* Retrieve the dataspace for the newly extended dataset. */
        file_space = H5Dget_space(dset);

        /* Subtract a hyperslab reflecting the original dimensions from the
         * selection. The selection now contains only the newly extended
         * portions of the dataset. */
        start[0] = dims[0]-extdims[0];
        start[1] = 0;
        count[0] = extdims[0];
        count[1] = extdims[1];
        status = H5Sselect_hyperslab(file_space, H5S_SELECT_SET, start, NULL, count, NULL);

        if (status) {
            fprintf(stderr, "error selecting hyperslab.\n");
            return 1;
        }

        float *wdata = malloc(n*nsamples*sizeof(float));

        for (j = 0; j < n; j++)
            for (k = 0; k < nsamples; k++)
                wdata[j*nsamples + k] = data[j][i][k];

        /* Write the data to the selected portion of the dataset. */
        status = H5Dwrite(dset, H5T_NATIVE_FLOAT, mem_space, file_space, H5P_DEFAULT, wdata);

        if (status) {
            fprintf(stderr, "error writing to hdf5 file.\n");
            return 1;
        }

        /* Close and release resources. */
        free(wdata);
        status = H5Sclose(mem_space);
        status = H5Dclose(dset);
        status = H5Sclose(file_space);

        if (status) {
            fprintf(stderr, "error closing hdf5 resources.\n");
            return 1;
        }
    }

    status = H5Fclose(file);

    if (status) {
        fprintf(stderr, "error closing hdf5 file.\n");
        return 1;
    }

    return 0;
}

void print_help()
{
    fprintf(stderr, "usage: wavedump -o [OUTPUT] -n [NUMBER] [CONFIG_FILE]\n"
    "  -b, --barcode <barcode>    Barcode of the module being tested\n"
    "  -v, --voltage <voltage>    Voltage (V)"
    "  --help                     Output this help and exit.\n"
    "\n");
    exit(1);
}

int main(int argc, char *argv[])
{
    WaveDumpConfig_t WDcfg;
    WaveDumpRun_t WDrun;
    CAEN_DGTZ_ErrorCode ret = CAEN_DGTZ_Success;
    int handle = -1;
    ERROR_CODES ErrCode = ERR_NONE;
    int i, j, ch;
    uint32_t AllocatedSize, BufferSize, NumEvents;
    char *buffer = NULL;
    char *EventPtr = NULL;
    int MajorNumber;
    CAEN_DGTZ_BoardInfo_t BoardInfo;
    CAEN_DGTZ_EventInfo_t EventInfo;
    char *config_filename = NULL;
    char *output_filename = NULL;
    int nevents = 100;
    int total_events = 0;
    double voltage = -1;
    int barcode = 0;
    uint32_t data;

    CAEN_DGTZ_X742_EVENT_t *Event742 = NULL;

    FILE *f_ini;
    CAEN_DGTZ_DRS4Correction_t X742Tables[MAX_X742_GROUP_SIZE];
    sprintf(path, "");
    int ReloadCfgStatus = 0x7FFFFFFF; // Init to the bigger positive number

    memset(&WDrun, 0, sizeof(WDrun));
    memset(&WDcfg, 0, sizeof(WDcfg));

    SetDefaultConfiguration(&WDcfg);

    WDcfg.LinkType = 0;
    WDcfg.LinkNum = 0;
    WDcfg.ConetNode = 0;
    WDcfg.BaseAddress = 0;
    //int Nch;
    //int Nbit;
    //float Ts;
    //int NumEvents;
    WDcfg.RecordLength = 1024;
    WDcfg.PostTrigger = 50;
    //int InterruptNumEvents;

    /* Disable test pattern. */
    WDcfg.TestPattern = 0;
    //CAEN_DGTZ_EnaDis_t DesMode;
    //int TriggerEdge;
    //CAEN_DGTZ_IOLevel_t FPIOtype;

    /* Set to enable external triggers, so we can trigger on the laser if we
     * want. */
    WDcfg.ExtTriggerMode = CAEN_DGTZ_TRGMODE_ACQ_ONLY;

    /* Enable all channels. */
    WDcfg.EnableMask = 0xFF;

    /* Set to trigger on negative pulses. */
    for (i = 0; i < MAX_SET; i++)
	WDcfg.PulsePolarity[i] = CAEN_DGTZ_PulsePolarityNegative;

    /* Set the DC offset of the channels. Not sure exactly what the difference between DCoffset and DCoffsetGrpCh is. */
    int dc = 0;
    int val;
    for (i = 0; i < MAX_SET; i++) {
	val = (int)((dc+50) * 65535 / 100); ///DC offset (percent of the dynamic range, -50 to 50)
        WDcfg.DCoffset[i] = val;
        for (j = 0; j < MAX_SET; j++) {
            WDcfg.DCoffsetGrpCh[i][j] = val;
        }
    }

    /* Don't care about the threshold because we set it later. */
    //uint32_t Threshold[MAX_SET];

    /* Not sure what this is. */
    //int Version_used[MAX_SET];

    for (i = 0; i < MAX_SET; i++)
        WDcfg.GroupTrgEnableMask[i] = 0xff;

    //uint32_t MaxGroupNumber;
	
    /* Fast trigger offset and threshold. Currently unused. */
    //uint32_t FTDCoffset[MAX_SET];
    //uint32_t FTThreshold[MAX_SET];

    /* Disable fast trigger. */
    WDcfg.FastTriggerEnabled = 0;
    WDcfg.FastTriggerMode = CAEN_DGTZ_TRGMODE_DISABLED;

    /* Generic write stuff. Don't care about this. */
    //int GWn;
    //uint32_t GWaddr[MAX_GW];
    //uint32_t GWdata[MAX_GW];
    //uint32_t GWmask[MAX_GW];
    //OUTFILE_FLAGS OutFileFlags;
    //uint16_t DecimationFactor;

    /* Set corrections to "AUTO" */
    WDcfg.useCorrections = -1;
    //int UseManualTables;

    //char TablesFilenames[MAX_X742_GROUP_SIZE][1000];

    /* Set DRS4 Frequency.  Values:
     *   0 = 5 GHz
     *   1 = 2.5 GHz
     *   2 = 1 GHz
     *   3 = 750 MHz */
    WDcfg.DRS4Frequency = (CAEN_DGTZ_DRS4Frequency_t) 1;

    //int StartupCalibration;
    //DAC_Calibration_data DAC_Calib;
    //char ipAddress[25];

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i],"-n") && i < argc - 1) {
            nevents = atoi(argv[++i]);
        } else if (!strcmp(argv[i],"-o") && i < argc - 1) {
            output_filename = argv[++i];
        } else if (!strcmp(argv[i],"--help")) {
            print_help();
        } else if (!strcmp(argv[i],"-b") || !strcmp(argv[i],"--barcode")) {
            barcode = atoi(argv[++i]);
        } else if (!strcmp(argv[i],"-v") || !strcmp(argv[i],"--voltage")) {
            voltage = atof(argv[++i]);
        } else {
            config_filename = argv[i];
        }
    }

    if (!output_filename || barcode == 0 || voltage < 0)
        print_help();

    if (access(output_filename, F_OK) == 0) {
        fprintf(stderr, "removing existing file '%s'\n", output_filename);
        remove(output_filename);
    }

    signal(SIGINT, sigint_handler);

    if (config_filename) {
        printf("Opening Configuration File %s\n", config_filename);
        f_ini = fopen(config_filename, "r");
        if (f_ini == NULL) {
            fprintf(stderr, "couldn't find configuration file '%s'\n", config_filename);
            exit(1);
        }
        memset(&WDcfg, 0, sizeof(WDcfg));
        ParseConfigFile(f_ini, &WDcfg);
        fclose(f_ini);
    }

    WDcfg.voltage = voltage;
    WDcfg.barcode = barcode;

    /* Open the digitizer. */
    ret = CAEN_DGTZ_OpenDigitizer(0, 0, 0, 0, &handle);

    if (ret) {
        fprintf(stderr, "unable to open digitizer! Is it turned on?\n");
        exit(1);
    }

    ret = CAEN_DGTZ_GetInfo(handle, &BoardInfo);

    if (ret) {
        fprintf(stderr, "unable to get board info.\n");
        exit(1);
    }

    printf("Connected to CAEN Digitizer Model %s\n", BoardInfo.ModelName);
    printf("ROC FPGA Release is %s\n", BoardInfo.ROC_FirmwareRel);
    printf("AMC FPGA Release is %s\n", BoardInfo.AMC_FirmwareRel);

    /* Check firmware rivision (DPP firmwares cannot be used with WaveDump) */
    sscanf(BoardInfo.AMC_FirmwareRel, "%d", &MajorNumber);
    if (MajorNumber >= 128) {
        printf("This digitizer has a DPP firmware! quitting...\n");
        exit(1);
    }

    /* Get Number of Channels, Number of bits, Number of Groups of the board */
    ret = GetMoreBoardInfo(handle, BoardInfo, &WDcfg);

    if (ret) {
        fprintf(stderr, "invalid board type\n");
        exit(1);
    }

    /* Check for possible board internal errors */
    ret = CheckBoardFailureStatus(handle, BoardInfo);

    if (ret) {
        fprintf(stderr, "CheckBoardFailureStatus() returned 1\n");
        exit(1);
    }

    //set default DAC calibration coefficients
    for (i = 0; i < MAX_SET; i++) {
        WDcfg.DAC_Calib.cal[i] = 1;
        WDcfg.DAC_Calib.offset[i] = 0;
    }

    //load DAC calibration data (if present in flash)
    if (BoardInfo.FamilyCode != CAEN_DGTZ_XX742_FAMILY_CODE)//XX742 not considered
        Load_DAC_Calibration_From_Flash(handle, &WDcfg, BoardInfo);

    // Perform calibration (if needed).
    if (WDcfg.StartupCalibration)
        calibrate(handle, &WDrun, BoardInfo);

    // mask the channels not available for this model
    if ((BoardInfo.FamilyCode != CAEN_DGTZ_XX740_FAMILY_CODE) && (BoardInfo.FamilyCode != CAEN_DGTZ_XX742_FAMILY_CODE)){
        WDcfg.EnableMask &= (1<<WDcfg.Nch)-1;
    } else {
        WDcfg.EnableMask &= (1<<(WDcfg.Nch/8))-1;
    }
    if ((BoardInfo.FamilyCode == CAEN_DGTZ_XX751_FAMILY_CODE) && WDcfg.DesMode) {
        WDcfg.EnableMask &= 0xAA;
    }
    if ((BoardInfo.FamilyCode == CAEN_DGTZ_XX731_FAMILY_CODE) && WDcfg.DesMode) {
        WDcfg.EnableMask &= 0x55;
    }
    // Set plot mask
    if ((BoardInfo.FamilyCode != CAEN_DGTZ_XX740_FAMILY_CODE) && (BoardInfo.FamilyCode != CAEN_DGTZ_XX742_FAMILY_CODE)){
        WDrun.ChannelPlotMask = WDcfg.EnableMask;
    } else {
        WDrun.ChannelPlotMask = (WDcfg.FastTriggerEnabled == 0) ? 0xFF: 0x1FF;
    }

    if ((BoardInfo.FamilyCode == CAEN_DGTZ_XX730_FAMILY_CODE) || (BoardInfo.FamilyCode == CAEN_DGTZ_XX725_FAMILY_CODE)) {
            WDrun.GroupPlotSwitch = 0;
    }

    /* program the digitizer */
    ret = ProgramDigitizer(handle, WDcfg, BoardInfo);

    if (ret) {
        fprintf(stderr, "error calling ProgramDigitizer()\n");
        exit(1);
    }

    usleep(300000);

    //check for possible failures after programming the digitizer
    ret = CheckBoardFailureStatus(handle, BoardInfo);

    if (ret) {
        fprintf(stderr, "error calling CheckBoardFailureStatus()\n");
        exit(1);
    }

    // Select the next enabled group for plotting
    if ((WDcfg.EnableMask) && (BoardInfo.FamilyCode == CAEN_DGTZ_XX742_FAMILY_CODE || BoardInfo.FamilyCode == CAEN_DGTZ_XX740_FAMILY_CODE))
        if( ((WDcfg.EnableMask>>WDrun.GroupPlotIndex)&0x1)==0 )
            GoToNextEnabledGroup(&WDrun, &WDcfg);

    // Read again the board infos, just in case some of them were changed by the programming
    // (like, for example, the TSample and the number of channels if DES mode is changed)
    if(ReloadCfgStatus > 0) {
        ret = CAEN_DGTZ_GetInfo(handle, &BoardInfo);
        if (ret) {
            ErrCode = ERR_BOARD_INFO_READ;
            goto QuitProgram;
        }
        ret = GetMoreBoardInfo(handle,BoardInfo, &WDcfg);
        if (ret) {
            ErrCode = ERR_INVALID_BOARD_TYPE;
            goto QuitProgram;
        }

        // Reload Correction Tables if changed
        if(BoardInfo.FamilyCode == CAEN_DGTZ_XX742_FAMILY_CODE && (ReloadCfgStatus & (0x1 << CFGRELOAD_CORRTABLES_BIT)) ) {
            if(WDcfg.useCorrections != -1) { // Use Manual Corrections
                uint32_t GroupMask = 0;

                // Disable Automatic Corrections
                if ((ret = CAEN_DGTZ_DisableDRS4Correction(handle)) != CAEN_DGTZ_Success)
                    goto QuitProgram;

                // Load the Correction Tables from the Digitizer flash
                if ((ret = CAEN_DGTZ_GetCorrectionTables(handle, WDcfg.DRS4Frequency, (void*)X742Tables)) != CAEN_DGTZ_Success)
                    goto QuitProgram;

                if(WDcfg.UseManualTables != -1) { // The user wants to use some custom tables
                    uint32_t gr;
					int32_t clret;
					
                    GroupMask = WDcfg.UseManualTables;

                    for(gr = 0; gr < WDcfg.MaxGroupNumber; gr++) {
                        if (((GroupMask>>gr)&0x1) == 0)
                            continue;
                        if ((clret = LoadCorrectionTable(WDcfg.TablesFilenames[gr], &(X742Tables[gr]))) != 0)
                            printf("Error [%d] loading custom table from file '%s' for group [%u].\n", clret, WDcfg.TablesFilenames[gr], gr);
                    }
                }
                // Save to file the Tables read from flash
                GroupMask = (~GroupMask) & ((0x1<<WDcfg.MaxGroupNumber)-1);
                SaveCorrectionTables("X742Table", GroupMask, X742Tables);
            }
            else { // Use Automatic Corrections
                if ((ret = CAEN_DGTZ_LoadDRS4CorrectionData(handle, WDcfg.DRS4Frequency)) != CAEN_DGTZ_Success)
                    goto QuitProgram;
                if ((ret = CAEN_DGTZ_EnableDRS4Correction(handle)) != CAEN_DGTZ_Success)
                    goto QuitProgram;
            }
        }
    }

    // Allocate memory for the event data and readout buffer
    ret = CAEN_DGTZ_AllocateEvent(handle, (void**)&Event742);

    if (ret != CAEN_DGTZ_Success) {
        ErrCode = ERR_MALLOC;
        goto QuitProgram;
    }

    /* WARNING: This malloc must be done after the digitizer programming */
    ret = CAEN_DGTZ_MallocReadoutBuffer(handle, &buffer,&AllocatedSize);

    if (ret) {
        ErrCode = ERR_MALLOC;
        goto QuitProgram;
    }

    usleep(300000);

    CAEN_DGTZ_SWStopAcquisition(handle);

    /* First, in order to self-trigger we need to set the digitizer into
     * transparent mode and read some data from the board to get an idea of the
     * un-calibrated baseline.
     *
     * See page 35 of the DT5742 manual. */

    /* First, we get the register value of 0x8000, and then set the 13th bit to
     * set it in transparent mode. */
    ret = CAEN_DGTZ_ReadRegister(handle, 0x8000, &data);

    if (ret) {
        fprintf(stderr, "failed to read register 0x8000!\n");
        exit(1);
    }

    data |= 1 << 13;

    ret = CAEN_DGTZ_WriteRegister(handle, 0x8000, data);

    if (ret) {
        fprintf(stderr, "failed to write register 0x8000!\n");
        exit(1);
    }

    /* Now, we take some data. */
    CAEN_DGTZ_SWStartAcquisition(handle);

    for (i = 0; i < 10; i++) {
        CAEN_DGTZ_SendSWtrigger(handle);
        usleep(1000);
    }

    /* Read data from the board */
    ret = CAEN_DGTZ_ReadData(handle, CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT, buffer, &BufferSize);

    CAEN_DGTZ_SWStopAcquisition(handle);

    if (ret) {
        fprintf(stderr, "error reading data in transparent mode!\n");
        exit(1);
    }

    NumEvents = 0;
    if (BufferSize != 0) {
        ret = CAEN_DGTZ_GetNumEvents(handle, buffer, BufferSize, &NumEvents);

        if (ret) {
            fprintf(stderr, "error calling CAEN_DGTZ_GetNumEvents()!\n");
            exit(1);
        }
    } else {
        fprintf(stderr, "error: didn't get any events when in transparent mode! quitting...\n");
        exit(1);
    }

    static float wfdata[WF_SIZE][32][1024];
    float baselines[32];
    float thresholds[2];

    int chmask = 0;
    int nsamples = 0;

    if (NumEvents > WF_SIZE)
        NumEvents = WF_SIZE;

    /* Analyze data */
    for(i = 0; i < NumEvents; i++) {
        /* Get one event from the readout buffer */
        ret = CAEN_DGTZ_GetEventInfo(handle, buffer, BufferSize, i, &EventInfo, &EventPtr);

        if (ret) {
            fprintf(stderr, "error calling CAEN_DGTZ_GetEventInfo()!\n");
            exit(1);
        }

        ret = CAEN_DGTZ_DecodeEvent(handle, EventPtr, (void**)&Event742);

        if (ret) {
            fprintf(stderr, "error calling CAEN_DGTZ_DecodeEvent()!\n");
            exit(1);
        }

        for (int gr = 0; gr < (WDcfg.Nch/8); gr++) {
            if (Event742->GrPresent[gr]) {
                for (ch = 0; ch < 8; ch++) {
                    int Size = Event742->DataGroup[gr].ChSize[ch];

                    if (Size <= 0)
                        continue;

                    nsamples = Size;

                    chmask |= 1 << (gr*8 + ch);

                    for (int j = 0; j < Size; j++) {
                        wfdata[i][gr*8 + ch][j] = Event742->DataGroup[gr].DataChannel[ch][j];
                    }
                }
            }
        }
    }

    get_baselines(wfdata, baselines, NumEvents, chmask, nsamples);

    printf("Baselines for channels:\n");
    for (i = 0; i < 32; i++)
        printf("    ch %2i = %.0f\n", i, baselines[i]);

    for (i = 0; i < 2; i++)
        thresholds[i] = 1e99;

    /* Since we have to set the threshold for the whole group, we set the
     * threshold to the minimum baseline for all channels within a group. */
    for (i = 0; i < 32; i++) {
        if (chmask & (1 << i)) {
            if (baselines[i] < thresholds[i/8])
                thresholds[i/8] = baselines[i];
        }
    }

    for (i = 0; i < 2; i++) {
        /* Subtract 50 from the minimum baseline to get the threshold. */
        thresholds[i] -= 50;

        if (thresholds[i] < 1e99) {
            printf("setting trigger threshold for group %i to %i\n", i, (int) thresholds[i]);
            ret = CAEN_DGTZ_WriteRegister(handle, 0x1080 + 256*i, (int) thresholds[i]);

            if (ret) {
                fprintf(stderr, "failed to write register 0x%04x!\n", 0x1080 + 256*i);
                exit(1);
            }

            printf("setting channel mask for group %i to 0x%02x\n", i, (int) (chmask >> i*16) & 0xff);
            ret = CAEN_DGTZ_WriteRegister(handle, 0x10A8 + 256*i, (int) (chmask >> i*16) & 0xff);

            if (ret) {
                fprintf(stderr, "failed to write register 0x%04x!\n", 0x10A8 + 256*i);
                exit(1);
            }
        } else {
            ret = CAEN_DGTZ_WriteRegister(handle, 0x10A8 + 256*i, 0);

            if (ret) {
                fprintf(stderr, "failed to write register 0x%04x!\n", 0x10A8 + 256*i);
                exit(1);
            }
        }
    }

    /* Now, we switch back to output mode. */
    ret = CAEN_DGTZ_ReadRegister(handle, 0x8000, &data);

    if (ret) {
        fprintf(stderr, "failed to read register 0x8000!\n");
        exit(1);
    }

    data &= ~(1 << 13);

    ret = CAEN_DGTZ_WriteRegister(handle, 0x8000, data);

    if (ret) {
        fprintf(stderr, "failed to write register 0x8000!\n");
        exit(1);
    }

    CAEN_DGTZ_SWStartAcquisition(handle);

    /* Now, we go into the main loop where we get events. */

    int nread = 0;

    while (!stop && total_events < nevents) {
        /* FIXME: Here, we send software triggers just for testing the
         * software. In production, this line should be commented out. */
        printf("sending sw trigger\n");
        for (i = 0; i < 1000; i++) {
	    CAEN_DGTZ_SendSWtrigger(handle);
            usleep(100);
        }

        ret = CAEN_DGTZ_ReadData(handle, CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT, buffer, &BufferSize);

        if (ret) {
            fprintf(stderr, "error calling CAEN_DGTZ_ReadData()!\n");
            exit(1);
        }

        NumEvents = 0;
        if (BufferSize != 0) {
            ret = CAEN_DGTZ_GetNumEvents(handle, buffer, BufferSize, &NumEvents);

            if (ret) {
                fprintf(stderr, "error calling CAEN_DGTZ_GetNumEvents()!\n");
                exit(1);
            }
        }

        if (NumEvents > WF_SIZE)
            NumEvents = WF_SIZE;

        printf("got %i events\n", NumEvents);

        /* Analyze data */
        nread = 0;
        for (i = 0; i < NumEvents; i++) {
            /* Get one event from the readout buffer */
            ret = CAEN_DGTZ_GetEventInfo(handle, buffer, BufferSize, i, &EventInfo, &EventPtr);

            if (ret) {
                fprintf(stderr, "error calling CAEN_DGTZ_GetEventInfo()!\n");
                exit(1);
            }

            ret = CAEN_DGTZ_DecodeEvent(handle, EventPtr, (void**)&Event742);

            if (ret) {
                fprintf(stderr, "error calling CAEN_DGTZ_DecodeEvent()!\n");
                exit(1);
            }

            for (int gr = 0; gr < (WDcfg.Nch/8); gr++) {
                if (Event742->GrPresent[gr]) {
                    for (ch = 0; ch < 8; ch++) {
                        int Size = Event742->DataGroup[gr].ChSize[ch];

                        if (Size <= 0)
                            continue;

                        nsamples = Size;

                        chmask |= 1 << (gr*8 + ch);

                        for (int j = 0; j < Size; j++) {
                            wfdata[nread][gr*8 + ch][j] = Event742->DataGroup[gr].DataChannel[ch][j];
                        }
                    }
                }
            }
            nread += 1;
        }

        if (nread > 0) {
            printf("writing %i events to file\n", nread);
            if (add_to_output_file(output_filename, wfdata, nread, chmask, nsamples, &WDcfg)) {
                fprintf(stderr, "failed to write events to file! quitting...\n");
                exit(1);
            }
        }

        total_events += nread;

        usleep(100000);
    }

    if (stop)
        fprintf(stderr, "ctrl-c caught. writing out %i events\n", nread);

    if (nread > 0 && add_to_output_file(output_filename, wfdata, nread, chmask, nsamples, &WDcfg)) {
        fprintf(stderr, "failed to write events to file!\n");
    }

    CAEN_DGTZ_SWStopAcquisition(handle);

    return 0;

QuitProgram:

    return 1;
}
