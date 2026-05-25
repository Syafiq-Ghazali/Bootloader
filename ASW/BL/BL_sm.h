/*
|===============================================================================
|
| File:         BL_sm.h
|
| Project:      DAANAA C2000 BOOTLOADER
|
| Processor:    TI TMS320F28003x
| Compiler:     TI C2000 compiler 22.6.0
|
| Component:    Bootloader Project Header File
|
| Description:  Bootloader state machine constants, prototypes, and typedefs
|
| Copyright:    Copyright (C) 2025 Daanaa Resolution Inc.
|
|               All Rights Reserved. Reproduction or disclosure of this file 
|               or its Contents without the prior written consent of Daanaa 
|               Resolution Inc is prohibited.
|===============================================================================
| Version   Date        Author  Description
|-------------------------------------------------------------------------------
|  1.00   DD-MMM-2025   AP      Initial Release.
|=============================================================================*/

#ifndef _BL_SM_H_
#define _BL_SM_H_    /* make sure header is not included again  */

/*=== INCLUDE FILES ==========================================================*/

#include "Datatype.h"

/*=== #DEFINES ===============================================================*/

#define BL_CLUSTER_CRC_MAX_RETRIES      (3u)

#define BL_MENU_TIMEOUT_MS              (8000uL)
#define BL_TARGET_UPDATE_TIMEOUT_MS     (1000uL)
#define BL_CAN_TX_PENDING_TIMEOUT_MS    (100uL)

/*=== TYPE DEFINITIONS =======================================================*/
typedef enum
{
    BL_STATE_STARTUP,
    BL_STATE_MENU,
    BL_STATE_UPDATE,
    BL_STATE_APP_RUN,
    BL_STATE_RESET,
    BL_STATE_ERROR,
    BL_NUM_STATES,
} SmState_t;

/*=== STRUCTURES =============================================================*/

typedef struct SmHandle_t
{
    void *self;
    SmState_t currState;
    SmState_t prevState;
    SmState_t nextState;
} SmHandle_t;

/*=== ENUMERATIONS ===========================================================*/

/*=== EXTERNAL FUNCTION PROTOTYPES ===========================================*/

extern Uint16_t     BL_smInitialize     (void);
extern SmState_t    BL_smStateRetreive  (void);
extern Uint16_t     BL_smStart          (void);
extern Uint16_t     BL_smExe            (void);

/*=== EXTERNAL VARIABLE DEFINITIONS ==========================================*/

#endif // don't include file twice
