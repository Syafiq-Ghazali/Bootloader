/*
|===============================================================================
|
| File:         template.c
|
| Project:      DAANAA C2000 BOOTLOADER
|
| Processor:    TI TMS320F28003x
| Compiler:     TI C2000 compiler 22.6.0
|
| Component:    Bootloader Project Component
|
| Description:
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

/*=== INCLUDE FILES ==========================================================*/

#include "BL_prcs.h"
#include "BL_ctx.h"
#include "BL_if.h"
#include "BL_img.h"
#include "BL_clstr.h"

/*=== #DEFINES ===============================================================*/

/*=== TYPE DEFINITIONS =======================================================*/

/*=== ENUMERATIONS ===========================================================*/

/*=== EXTERNALS ==============================================================*/

/*=== PRIVATE FUNCTION PROTOTYPES ============================================*/

/*=== GLOBAL DATA ============================================================*/

/*=== PRIVATE DATA ===========================================================*/

/*
|===============================================================================
|
| Function:         BL_prcsAppErase
|
| Description:      Processes a BL_APP_ERASE request
|
| Dependencies:
|
| Notes:
|
| Side Effects:
|
| Return Value:     The last request status that is sent back to the host during
|                   the request handling process
|
|===============================================================================
| Variable     Access  Description  (I=input O=output I/O=in/out)
|-------------------------------------------------------------------------------
| ctx          I/O     Bootloader context struct
|=============================================================================*/

IfRequestStatus_t BL_prcsAppErase
(
    CtxData_t *ctx
)
{
    return BL_REQUEST_STATUS_COMPLETE;
}

/*
|===============================================================================
|
| Function:
|
| Description:
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

/*=== End of File ============================================================*/
