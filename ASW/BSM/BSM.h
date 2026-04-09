/* Includes */


#ifndef _BSM_H_
#define _BSM_H_

/* Include Files */
#include "driverlib.h"

/* Enums */

typedef enum
{
    BL_STATE_INIT,
    BL_STATE_READ_METADATA,
    BL_STATE_DECIDE_BOOT,
    BL_STATE_WAIT_FOR_UPDATE,
    BL_STATE_RECEIVE_IMAGE,
    BL_STATE_VALIDATE_IMAGE,
    BL_STATE_MARK_PENDING,
    BL_STATE_SWAP_IMAGES,
    BL_STATE_LAUNCH_APP,
    BL_STATE_MONITOR_CONFIRM,
    BL_STATE_ROLLBACK,
    BL_STATE_ERROR
} BlState_t;

/* Structs */

typedef struct SmHandle_t
{
    void *self;
    SmState_t currState;
    SmState_t prevState;
    SmState_t nextState;
} SmHandle_t;

typedef struct BlImageMetadata_t
{
    uint32_t version;
    uint32_t imageSize;
    uint32_t crc;
    uint32_t startAddress;
    uint16_t  valid;
    uint16_t  pending;
    uint16_t  confirmed;
    uint16_t  bootAttempts;
} BlImageMetadata_t;

typedef struct
{
    BlImageMetadata_t slotA;
    BlImageMetadata_t slotB;
    uint16_t activeSlot;
} BlMetadataTable_t;    

#endif