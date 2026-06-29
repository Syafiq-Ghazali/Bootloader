/*
 * Bootloader state machine.
 */

/*=== INCLUDE FILES ==========================================================*/

#include <string.h>

#include "BL_sm.h"
#include "TMR.h"
#include "BL_ctx.h"
#include "BL_if.h"
#include "BL_clstr.h"
#include "BootloaderDefs.h"

/*=== #DEFINES ===============================================================*/

/*=== TYPE DEFINITIONS =======================================================*/

/*=== ENUMERATIONS ===========================================================*/

/*=== EXTERNALS ==============================================================*/

/*=== PRIVATE FUNCTION PROTOTYPES ============================================*/

static void F_stateTransition   (void);
static void F_menuStateProcess  (void);
static void F_updateStateProcess(void);

/*=== GLOBAL DATA ============================================================*/

/*=== PRIVATE DATA ===========================================================*/

static CtxData_t F_ctx;

static SmHandle_t F_sm =
{
    .self = (void *)&F_sm,
    .currState = BL_STATE_STARTUP,
    .prevState = BL_STATE_STARTUP,
    .nextState = BL_STATE_STARTUP
};

/*
|===============================================================================
|
| Function:         F_stateTransition
|
| Description:      Transitions to the next state
|
| Dependencies:
|
| Notes:
|
| Side Effects:
|
| Return Value:
|
|===============================================================================
| Variable     Access  Description  (I=input O=output I/O=in/out)
|-------------------------------------------------------------------------------
|=============================================================================*/

static void F_stateTransition
(
    void
)
{
    if (F_sm.nextState != F_sm.currState)
    {
        F_sm.prevState = F_sm.currState;
        F_sm.currState = F_sm.nextState;
    }
}

/*
|===============================================================================
|
| Function:         F_menuStateProcess
|
| Description:      Processes the BL_STATE_MENU operations
|
| Dependencies:
|
| Notes:            Should only be called inside the BL_STATE_MENU case of the 
|                   BL_exe() function. Do not use this functionn anywhere else.
|
| Side Effects:
|
| Return Value:
|
|===============================================================================
| Variable     Access  Description  (I=input O=output I/O=in/out)
|-------------------------------------------------------------------------------
|=============================================================================*/

static void F_menuStateProcess
(
    void
)
{
    // Handle menu related request
    if (F_ctx.currMsg.type == BL_IF_MESSAGE_REQUEST)
    {
        switch (F_ctx.currMsg.request.mux)
        {
            case BL_REQUEST_MUX_INFO:
            {
                // Send BL_REPLY_VERSION and BL_REPLY STATUS messages
                BL_ifReplyVersionTx(F_ctx.fMetadata.bootloaderVersion,
                                    F_ctx.vMetadata.appVersion,
                                    F_ctx.fMetadata.hardwareVersion);

                BL_ifReplySysStatusTx(F_ctx.vMetadata.appCrc, F_ctx.appValid, F_ctx.errorReason);
                
                // Stay in MENU state
                F_sm.nextState = BL_STATE_MENU;

                break;
            }

            case BL_REQUEST_MUX_APP_ERASE:
            {
                // Reply with IN_PROGRESS
                BL_ifReplyReqStatusTx(BL_REQUEST_STATUS_IN_PROGRESS, F_ctx.currMsg.request.mux);

                // Erase the app flash sectors and reply with COMPLETE or FAIL or RETRY OR ERROR
                if (BL_imgAppErase() == OK)
                {
                    BL_ifReplyReqStatusTx(BL_REQUEST_STATUS_COMPLETE, F_ctx.currMsg.request.mux);
                    
                    // Stay in MENU state
                    F_sm.nextState = BL_STATE_MENU;
                }
                else 
                {
                    BL_ifReplyReqStatusTx(BL_REQUEST_STATUS_FAILED, F_ctx.currMsg.request.mux);
                    
                    // A flash operation failure has occurred. Set the error reason and 
                    // go to the ERROR state
                    F_ctx.errorReason = BL_ERROR_FLASH;
                    F_sm.nextState = BL_STATE_ERROR;
                }

                break;
            }

            case BL_REQUEST_MUX_UPDATE_INITIALIZE:
            {
                // Reply with IN_PROGRESS
                BL_ifReplyReqStatusTx(BL_REQUEST_STATUS_IN_PROGRESS, F_ctx.currMsg.request.mux);
                
                // Update the app version and app size (size in words)
                F_ctx.vMetadata.appVersion = F_ctx.currMsg.request.appVersion;
                F_ctx.vMetadata.appSizeWords = F_ctx.currMsg.request.appSize;
                
                // Transition to UPDATE iff app will fit into flash memory
                if (F_ctx.vMetadata.appSizeWords <= BL_IMG_APPLICATION_REGION_SIZE_WORDS)
                {
                    BL_ifReplyReqStatusTx(BL_REQUEST_STATUS_COMPLETE, F_ctx.currMsg.request.mux);
                    F_sm.nextState = BL_STATE_UPDATE;
                }
                else
                {
                    BL_ifReplyReqStatusTx(BL_REQUEST_STATUS_FAILED, F_ctx.currMsg.request.mux);

                    // The app which the update is being initiated to is too large
                    F_ctx.errorReason = BL_ERROR_APP_SIZE;
                    F_sm.nextState = BL_STATE_ERROR;
                }
                
                break;
            }
            
            case BL_REQUEST_MUX_APP_RUN:
            {
                F_sm.nextState = BL_STATE_APP_RUN;

                break;
            }

            case BL_REQUEST_MUX_RESET:
            {
                F_sm.nextState = BL_STATE_RESET;
                break;
            }


            default: // Invalid request received
            {
                BL_ifReplyReqStatusTx(BL_REQUEST_STATUS_INVALID, F_ctx.currMsg.request.mux);
                F_sm.nextState = BL_STATE_MENU;

                break;
            }
        }
    }
    else
    {
        BL_ifReplyReqStatusTx(BL_REQUEST_STATUS_INVALID, BL_REQUEST_NUM_MUX_VALUES);
        F_sm.nextState = BL_STATE_MENU;
    }
}

/*
|===============================================================================
|
| Function:         F_updateStateProcess         
|
| Description:      Processes the BL_STATE_UPDATE operations
|
| Dependencies:
|
| Notes:            Should only be called inside the BL_STATE_UPDATE case of the
|                   BL_exe() function. Do not use this function anywhere else.
|
| Side Effects:
|
| Return Value:
|
|===============================================================================
| Variable     Access  Description  (I=input O=output I/O=in/out)
|-------------------------------------------------------------------------------
|=============================================================================*/

static void F_updateStateProcess
(
    void
)
{
    if (F_ctx.currMsg.type == BL_IF_MESSAGE_REQUEST)
    {
        switch (F_ctx.currMsg.request.mux)
        {
            case BL_REQUEST_MUX_LOAD_CONFIGURE:
            {
                // Reply with IN_PROGRESS
                BL_ifReplyReqStatusTx(BL_REQUEST_STATUS_IN_PROGRESS, F_ctx.currMsg.request.mux);

                // Store the address and length of the cluster
                F_ctx.clstr.address = F_ctx.currMsg.request.clusterAddress;
                F_ctx.clstr.length = F_ctx.currMsg.request.clusterLength;
                
                // Configure the cluster load process and reply with COMPLETE or FAILED
                if (BL_clstrConfig(&F_ctx.clstr) == OK)
                {
                    BL_ifReplyReqStatusTx(BL_REQUEST_STATUS_COMPLETE, F_ctx.currMsg.request.mux);
                    
                    // Stay in the UPDATE state
                    F_sm.nextState = BL_STATE_UPDATE;
                }
                else
                {
                    BL_ifReplyReqStatusTx(BL_REQUEST_STATUS_FAILED, F_ctx.currMsg.request.mux);

                    // A data loading operation failed. This almost certainly means that the 
                    // cluster is larger than the block buffer. This is non-recoverable, as 
                    // it means that the input hex file was not correctly generated.
                    F_ctx.errorReason = BL_ERROR_LOAD_CONFIGURE;
                    F_sm.nextState = BL_STATE_ERROR;
                }

                break;
            }

            case BL_REQUEST_MUX_BUFFER_FLASH:
            {  
                // Reply with IN_PROGRESS
                BL_ifReplyReqStatusTx(BL_REQUEST_STATUS_IN_PROGRESS, F_ctx.currMsg.request.mux);

                // Compute the crc of the cluster inside the block buffer
                BL_clstrCrcCompute(&F_ctx.clstr);
                
                // If the computed crc is correct, proceed with flashing
                if (F_ctx.clstr.crc == F_ctx.currMsg.request.clusterCrc)
                {
                    if (BL_clstrFlash() == OK)
                    {
                        BL_ifReplyReqStatusTx(BL_REQUEST_STATUS_COMPLETE, F_ctx.currMsg.request.mux);

                        // Stay in UPDATE state
                        F_sm.nextState = BL_STATE_UPDATE;
                    }
                    else
                    {
                        BL_ifReplyReqStatusTx(BL_REQUEST_STATUS_FAILED, F_ctx.currMsg.request.mux);

                        // A flash operation failure has occurred. Set the error reason and 
                        // go to the ERROR state
                        F_ctx.errorReason = BL_ERROR_FLASH;
                        F_sm.nextState = BL_STATE_ERROR;
                    }
                }
                else
                {
                    // If there is a crc mismatch, and the number of cluster retries has not 
                    // exceeded, then send a reply saying that a cluster retry is permitted,
                    // and then increment the cluster retry counter. Otherwise, error out
                    // with error reason BL_ERROR_CLUSTER_CRC
                    if (F_ctx.clstrCrcRetryCount < BL_CLUSTER_CRC_MAX_RETRIES)
                    {
                        BL_ifReplyReqStatusTx(BL_REQUEST_STATUS_RETRY, F_ctx.currMsg.request.mux);
                        F_ctx.clstrCrcRetryCount++;

                        // Stay in UPDATE state
                        F_sm.nextState = BL_STATE_UPDATE;
                    }
                    else
                    {
                        // Update the error reason and error out
                        BL_ifReplyReqStatusTx(BL_REQUEST_STATUS_FAILED, F_ctx.currMsg.request.mux);
                        F_ctx.errorReason = BL_ERROR_CLUSTER_CRC;
                        F_sm.nextState = BL_STATE_ERROR;
                    }
                }

                break;
            }

            case BL_REQUEST_MUX_UPDATE_FINISH:
            {
                BL_ifReplyReqStatusTx(BL_REQUEST_STATUS_IN_PROGRESS, F_ctx.currMsg.request.mux);
                F_ctx.vMetadata.appCrc = F_ctx.currMsg.request.appCrc;
                
                if (BL_imgVariableMetadataErase() == OK)
                {
                    if (BL_imgVariableMetadataWrite(&F_ctx.vMetadata) == OK)
                    {
                        BL_ifReplyReqStatusTx(BL_REQUEST_STATUS_COMPLETE, F_ctx.currMsg.request.mux);
                        
                        F_sm.nextState = BL_STATE_RESET;
                    }
                    else
                    {
                        // Variable metadata flash write failed
                        BL_ifReplyReqStatusTx(BL_REQUEST_STATUS_FAILED, F_ctx.currMsg.request.mux);

                        // A flash operation failure has occurred. Set the error reason and 
                        // go to the ERROR state
                        F_ctx.errorReason = BL_ERROR_FLASH;
                        F_sm.nextState = BL_STATE_ERROR;
                    }
                }
                else
                {
                    // Variable metadata flash erase failed
                    BL_ifReplyReqStatusTx(BL_REQUEST_STATUS_FAILED, F_ctx.currMsg.request.mux);

                    // A flash operation failere has occurred. Set the error reason and
                    // go to the ERROR state
                    F_ctx.errorReason = BL_ERROR_FLASH;
                    F_sm.nextState = BL_STATE_ERROR;
                }

                break;
            }

            case BL_REQUEST_MUX_RESET:
            {
                F_sm.nextState = BL_STATE_RESET;
                break;
            }

            default: // Invalid state received
            {
                BL_ifReplyReqStatusTx(BL_REQUEST_STATUS_INVALID, F_ctx.currMsg.request.mux);
                F_sm.nextState = BL_STATE_UPDATE;
                break;
            }
        }
    }
    else if (F_ctx.currMsg.type == BL_IF_MESSAGE_DATA)
    {
        BL_ifReplyReqStatusTx(BL_REQUEST_STATUS_IN_PROGRESS, BL_REQUEST_MUX_DATA);
        
        Uint16_t lenWords = (F_ctx.currMsg.lenBytes + 1) / 2;
        if (BL_clstrLoad(&F_ctx.clstr, F_ctx.currMsg.data.dataWords, lenWords) == OK)
        {
            BL_ifReplyReqStatusTx(BL_REQUEST_STATUS_COMPLETE, BL_REQUEST_MUX_DATA);
            F_sm.nextState = BL_STATE_UPDATE;
        }
        else
        {
            BL_ifReplyReqStatusTx(BL_REQUEST_STATUS_FAILED, BL_REQUEST_MUX_DATA);
            
            // A data loading operation failed. This is a critical error that cannot
            // be recovered by retries, hence we error out here.
            // The conditions of this error are also checked in the LOAD_CONFIGURE state,
            // hence this error serves as a backup.
            F_ctx.errorReason = BL_ERROR_DATA;
            F_sm.nextState = BL_STATE_ERROR;
        }
    }
    else
    {
        BL_ifReplyReqStatusTx(BL_REQUEST_STATUS_INVALID, BL_REQUEST_NUM_MUX_VALUES);
        F_sm.nextState = BL_STATE_UPDATE;
    }

}

/*
|===============================================================================
|
| Function:         BL_smInitialize
|
| Description:      Initializes the bootloader state machine
|
| Dependencies:     
|
| Notes:
|
| Side Effects:
|
| Return Value:    
|
|===============================================================================
| Variable     Access  Description  (I=input O=output I/O=in/out)
|-------------------------------------------------------------------------------
|=============================================================================*/

Uint16_t BL_smInitialize
(
    void
)
{
    // Initialize the F_ctx struct
    // The counters should be initialized to zero
    memset(&F_ctx, 0x0000, sizeof(CtxData_t));

    return OK;
}

/*
|===============================================================================
|
| Function:         BL_smStateRetreive
|
| Description:      Returns current bootloader state
|
| Dependencies:
|
| Notes:
|
| Side Effects:
|
| Return Value:
|
|===============================================================================
| Variable     Access  Description  (I=input O=output I/O=in/out)
|-------------------------------------------------------------------------------
|=============================================================================*/

SmState_t BL_smStateRetreive
(
    void
)
{
    return (F_sm.currState);
}

/*
|===============================================================================
|
| Function:         BL_smStart
|
| Description:      Starts the bootloader state machine
|
| Dependencies:
|
| Notes:            Function only returns if state machine encounters an 
|                   error which requires it to return
|
| Side Effects:
|
| Return Value:     FAIL or OK
|
|===============================================================================
| Variable     Access  Description  (I=input O=output I/O=in/out)
|-------------------------------------------------------------------------------
|=============================================================================*/

Uint16_t BL_smStart
(
    void
)
{
    Uint16_t ret = OK;

    while (ret == OK)
    {
        ret = BL_smExe();
    }

    return ret;
}

/*
|===============================================================================
|
| Function:         BL_smExe
|
| Description:      Executes one iteration of the bootloader state machine
|
| Dependencies:
|
| Notes:
|
| Side Effects:
|
| Return Value:     FAIL or OK
|
|===============================================================================
| Variable     Access  Description  (I=input O=output I/O=in/out)
|-------------------------------------------------------------------------------
|=============================================================================*/

Uint16_t BL_smExe
(
    void
)
{
    // Create a static variable for the timeout timer
    static TmrHandle_t timeoutTmr;

    if (F_sm.self != (void *)&F_sm)
    {
        F_sm.nextState = BL_STATE_ERROR;
    }

    switch(F_sm.currState)
    {
        case BL_STATE_STARTUP:
        {
            // Read in the variable and fixed metadata currently in flash memory
            BL_imgVariableMetadataRead(&F_ctx.vMetadata);
            BL_imgFixedMetadataRead(&F_ctx.fMetadata);

            // Compute and check app image crc
            Uint32_t appCrcComp = BL_imgCrcCompute();
            F_ctx.appValid = (appCrcComp == F_ctx.vMetadata.appCrc);
            F_ctx.errorReason = BL_ERROR_NONE;
            
            // Send BL_REPLY_VERSION and BL_REPLY STATUS messages
            BL_ifReplyVersionTx(F_ctx.fMetadata.bootloaderVersion,
                                F_ctx.vMetadata.appVersion,
                                F_ctx.fMetadata.hardwareVersion);

            BL_ifReplySysStatusTx(F_ctx.vMetadata.appCrc, F_ctx.appValid, F_ctx.errorReason);

            // Go to MENU
            F_sm.nextState = BL_STATE_MENU;

            break;
        }

        case BL_STATE_MENU:
        {
            // Start the menu timeout
            TMR_createAndStart(&timeoutTmr, BL_MENU_TIMEOUT_MS);
            Bool_t timerExpired = FALSE;
            while ((timerExpired == FALSE) &&
                   (BL_ifRx(&F_ctx.currMsg) == FAIL))
            {
                timerExpired = TMR_updateAndCheckExpiry(&timeoutTmr);
            }
            
            // If the timer is expired then attempt to run the app
            if (timerExpired == TRUE)
            {
                F_sm.nextState = BL_STATE_APP_RUN;
            }
            else
            {
                F_menuStateProcess(); 
            }

            break;
        }

        case BL_STATE_UPDATE:
        {
            // Poll for a update related request
            // - Add a timeout timer to wait for an update request. If the 
            //   timer expires, then update error reason to BL_ERROR_TARGET_UPDATE_TIMEOUT,
            //   and then go to ERROR state
            TMR_createAndStart(&timeoutTmr, BL_TARGET_UPDATE_TIMEOUT_MS);
            Bool_t timerExpired = FALSE;
            while ((timerExpired == FALSE) &&
                   (BL_ifRx(&F_ctx.currMsg) == FAIL))
            {
                timerExpired = TMR_updateAndCheckExpiry(&timeoutTmr);
            }

            // If the timer is expired then error out with BL_ERROR_TARGET_UPDATE_TIMEOUT
            if (timerExpired == TRUE)
            {
                F_ctx.errorReason = BL_ERROR_TARGET_UPDATE_TIMEOUT;
                F_sm.nextState = BL_STATE_ERROR;
            }
            else
            {
                F_updateStateProcess();           
            }

            break;
        }

        case BL_STATE_RESET:
        {
            // Wait for all CAN messages to be sent before reset
            // Implement a timeout in case of some sort of CAN failure
            // that would result in tranmissions to halt
            TMR_createAndStart(&timeoutTmr, BL_CAN_TX_PENDING_TIMEOUT_MS);
            Bool_t timerExpired = FALSE;
            while ((timerExpired == FALSE) &&
                   (BL_ifTxPending() == TRUE))
            {
                timerExpired = TMR_updateAndCheckExpiry(&timeoutTmr);
            }

            // Reset the device
            SysCtl_resetDevice();

            // Code should not reach here as it should have reset
            break;
        }
        
        case BL_STATE_APP_RUN:
        {
            if (F_ctx.appValid == TRUE)
            {
                // Should not return from this
                BL_imgAppStart();

                // If for some reason it does return, go to error state
                F_sm.nextState = BL_STATE_ERROR;
            }
            else
            {
                // Reset if you do not have a valid app
                F_sm.nextState = BL_STATE_RESET;
            }

            break;
        }

        default:
        case BL_STATE_ERROR:
        {
            // Reaching this state means that the update process has either
            // timed out, or has reached an unrecoverable state. As such,
            // it is best to send the current systemStatus, and, depending 
            // on the error, either reset, or wait for an explicit reset request.
            
            // Transmit whatever app CRC is currently in the variable metadata section
            BL_imgVariableMetadataRead(&F_ctx.vMetadata);
            Uint32_t appCrcComp = BL_imgCrcCompute();
            F_ctx.appValid = (appCrcComp == F_ctx.vMetadata.appCrc);

            BL_ifReplySysStatusTx(F_ctx.vMetadata.appCrc, F_ctx.appValid, F_ctx.errorReason);
            
            // Reset the device
            F_sm.nextState = BL_STATE_RESET;

            break;
        }
    }

    // Transition the state (if there is a state change)
    F_stateTransition();

    return OK;
}

/*=== End of File ============================================================*/

