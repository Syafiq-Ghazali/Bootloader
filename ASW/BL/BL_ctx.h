/*
|===============================================================================
|
| File:         BL_ctx.h
|
| Project:      DAANAA C2000 BOOTLOADER
|
| Processor:    TI TMS320F28003x
| Compiler:     TI C2000 compiler 22.6.0
|
| Component:    Bootloader Project Header File
|
| Description:  Header file for bootloader context and data API
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

#ifndef _BL_CTX_H_
#define _BL_CTX_H_    /* make sure header is not included again  */

/*=== INCLUDE FILES ==========================================================*/

#include "Datatype.h"
#include "BL_if.h"
#include "BL_img.h"
#include "BL_clstr.h"

/*=== #DEFINES ===============================================================*/

/*=== TYPE DEFINITIONS =======================================================*/

/*=== STRUCTURES =============================================================*/

// The context struct is used in the state machine to keep all bootloader-related
// data and context together. All data and context that relate to bootloader 
// operation must be defined inside this struct. A static instantiation of this
// struct will then be used in the bootloader state machine file.
typedef struct CtxData_t
{
    // Enum denoting any possible error reason
    IfErrorReason_t errorReason;

    // Bool denoting whether current app is valid or not
    Bool_t appValid;

    // Variable metadata (loaded in and updated and written with this variable)
    // App crc in this struct should always be the app crc that is calculated
    // Do not store the crc received from CAN in this struct
    ImgVariableMetadata_t vMetadata;

    // Fixed metadata currently in flash memory (should never change)
    ImgFixedMetadata_t fMetadata;

    // Most recent received message
    IfMessage_t currMsg;

    // Cluster management
    // Cluster crc in this struct should always be the cluster crc that is calculated
    // Do not store the crc received from CAN in this struct
    ClstrConfig_t clstr;

    // Cluster CRC retry counter
    Uint16_t clstrCrcRetryCount;
} CtxData_t;

/*=== ENUMERATIONS ===========================================================*/

/*=== EXTERNAL FUNCTION PROTOTYPES ===========================================*/

/*=== EXTERNAL VARIABLE DEFINITIONS ==========================================*/

#endif // don't include file twice
