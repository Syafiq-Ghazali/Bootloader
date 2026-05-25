/*
|===============================================================================
|
| File:         BL_clstr.h
|
| Project:      DAANAA C2000 BOOTLOADER
|
| Processor:    TI TMS320F28003x
| Compiler:     TI C2000 compiler 22.6.0
|
| Component:    Bootloader Project Header File
|
| Description:  Bootloader cluster processing API header
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

#ifndef _BL_CLSTR_H_
#define _BL_CLSTR_H_    /* make sure header is not included again  */

/*=== INCLUDE FILES ==========================================================*/

#include "Datatype.h"
#include "BFLASH.h"

/*=== #DEFINES ===============================================================*/

#define BL_BLOCK_BUFFER_LENGTH  (128uL)
#define BL_CLSTR_INITIAL_CRC    (0)

/*=== TYPE DEFINITIONS =======================================================*/

/*=== STRUCTURES =============================================================*/

typedef struct ClstrConfig_t
{
    Uint32_t address;   // Cluster start address
    Uint16_t length;    // Cluster length
    Uint32_t crc;       // Cluster CRC-32
} ClstrConfig_t;

/*=== ENUMERATIONS ===========================================================*/

/*=== EXTERNAL FUNCTION PROTOTYPES ===========================================*/

extern void     BL_clstrInit        (void);
extern Uint16_t BL_clstrConfig      (ClstrConfig_t *clstr);
extern Uint16_t BL_clstrLoad        (ClstrConfig_t *clstr, Uint16_t *dataWords, Uint16_t dataLen);
extern void     BL_clstrCrcCompute  (ClstrConfig_t *clstr);
extern Uint16_t BL_clstrFlash       (void);

/*=== EXTERNAL VARIABLE DEFINITIONS ==========================================*/

#endif // don't include file twice
