/*
|===============================================================================
|
| File:         BL_if.h
|
| Project:      DAANAA C2000 BOOTLOADER
|
| Processor:    TI TMS320F28003x
| Compiler:     TI C2000 compiler 22.6.0
|
| Component:    Bootloader Project Header File
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

#ifndef _BL_IF_H_
#define _BL_IF_H_    /* make sure header is not included again  */

/*=== INCLUDE FILES ==========================================================*/

#include "DS_Queue.h"
#include "MSG.h"

/*=== #DEFINES ===============================================================*/

#define BL_IF_CAN_ID_REPLY      (0x200uL)
#define BL_IF_CAN_ID_REQUEST    (0x201uL)
#define BL_IF_CAN_ID_DATA       (0x202uL)

#define BL_IF_CAN_LEN           (8uL)
#define BL_IF_CAN_LEN_REPLY     (BL_IF_CAN_LEN)
#define BL_IF_CAN_LEN_REQUEST   (BL_IF_CAN_LEN)
#define BL_IF_CAN_LEN_DATA      (BL_IF_CAN_LEN)
#define BL_IF_DATA_LEN_WORDS    (BL_IF_CAN_LEN / (2uL))

#define BL_IF_RX_QUEUE_LENGTH   (10u)

/*=== TYPE DEFINITIONS =======================================================*/

/*=== ENUMERATIONS ===========================================================*/

// Create a message type enum to use instead of CAN IDs
// Allows to store CAN payload if needed as enum values are small
// Easier to track when debugging over JTAG
typedef enum IfMessageType_t
{
    BL_IF_MESSAGE_REQUEST,
    BL_IF_MESSAGE_DATA,
    BL_IF_MESSAGE_REPLY,
    BL_IF_MESSAGE_UNKNOWN,
} IfMessageType_t;

// Enum for all message multiplexor values
typedef enum IfMux_t
{
    BL_REPLY_MUX_REQSTATUS = 0,
    BL_REPLY_MUX_VERSION,
    BL_REPLY_MUX_SYSSTATUS,
    BL_REPLY_NUM_MUX_VALUES,

    BL_REQUEST_MUX_APP_ERASE = 0,
    BL_REQUEST_MUX_UPDATE_INITIALIZE,
    BL_REQUEST_MUX_LOAD_CONFIGURE,
    BL_REQUEST_MUX_BUFFER_FLASH,
    BL_REQUEST_MUX_UPDATE_FINISH,
    BL_REQUEST_MUX_APP_RUN,
    BL_REQUEST_MUX_RESET,
    BL_REQUEST_MUX_INFO,
    BL_REQUEST_MUX_DATA,  // Technically not a request but needed for acknowledgement
    BL_REQUEST_NUM_MUX_VALUES,
} IfMux_t;

typedef enum IfRequestStatus_t
{
    BL_REQUEST_STATUS_IN_PROGRESS,
    BL_REQUEST_STATUS_COMPLETE,
    BL_REQUEST_STATUS_FAILED,
    BL_REQUEST_STATUS_RETRY,
    BL_REQUEST_STATUS_INVALID,
} IfRequestStatus_t;

typedef enum IfErrorReason_t
{
    BL_ERROR_NONE,
    BL_ERROR_TARGET_UPDATE_TIMEOUT,
    BL_ERROR_FLASH,
    BL_ERROR_CLUSTER_CRC,
    BL_ERROR_DATA,
    BL_ERROR_LOAD_CONFIGURE,
    BL_ERROR_APP_SIZE,
} IfErrorReason_t;

/*=== STRUCTURES =============================================================*/

// Define a struct for each message
typedef struct IfReplyMessage_t
{
    // Message mux
    IfMux_t mux;
    
    // Message is muxed, so use a union to allow for different payload possibilities
    union
    {
        // BL_REPLY_MUX_REQSTATUS
        struct
        {
            IfRequestStatus_t requestStatus;
            IfMux_t requestMux;
        } reqStatus;
        
        // BL_REPLY_MUX_VERSION
        struct
        {
            Uint16_t bootloaderVersion;
            Uint16_t appVersion;
            Uint16_t hardwareVersion;
        } version;
        
        // BL_REPLY_MUX_SYSSTATUS
        struct
        {
            Uint32_t imageCrc;
            IfErrorReason_t errorReason;
            Bool_t appValid;
        } sysStatus;
    };

} IfReplyMessage_t;

typedef struct IfRequestMessage_t
{
    // Message mux
    IfMux_t mux;

    // Use a union to allow for different payload possibilities
    union
    {
        // BL_REQUEST_MUX_UPDATE_INITIALIZE
        struct
        {
            Uint32_t appSize;
            Uint16_t appVersion;
        } updateInitialize;
        
        // BL_REQUEST_MUX_LOAD_CONFIGURE
        struct
        {
            Uint32_t clusterAddress;
            Uint16_t clusterLength;
        } loadConfigure;
        
        // BL_REQUEST_MUX_BUFFER_FLASH
        struct
        {
            Uint32_t clusterCrc;
        } bufferFlash;
        
        // BL_REQUEST_MUX_UPDATE_FINISH
        struct
        {
            Uint32_t appCrc;
        } updateFinish;
        
        // BL_REQUEST_MUX_APP_ERASE
        // BL_REQUEST_MUX_APP_RUN
        // BL_REQUEST_MUX_RESET
        // BL_REQUEST_MUX_INFO
    };

} IfRequestMessage_t;

typedef struct IfDataMessage_t
{
    // Note that this data is fully packed (each index holds 16 bits not 8 bits)
    Uint16_t dataWords[BL_IF_DATA_LEN_WORDS];
} IfDataMessage_t;

// Define a general purpose message struct which
// unionizes different possible payloads using the structs
// defined above
typedef struct IfMessage_t
{   
    // Message type enum corrseponding to a specific CAN ID
    IfMessageType_t type;

    // Message payload can either be a reply or request or data 
    union
    {
        IfReplyMessage_t reply;
        IfRequestMessage_t request;
        IfDataMessage_t data;
    };
    
    // Message length in bytes (up to 8 bytes)
    Uint16_t lenBytes;

} IfMessage_t;

/*=== EXTERNAL FUNCTION PROTOTYPES ===========================================*/

extern void     BL_ifInit               (void);
extern Uint16_t BL_ifTx                 (const IfMessage_t *msg);
extern Uint16_t BL_ifRx                 (IfMessage_t *msg);
extern Bool_t   BL_ifMsgRxHandler       (const can_t *frame);
extern Uint16_t BL_ifReplyReqStatusTx   (IfRequestStatus_t requestStatus, IfMux_t requestMux);
extern Uint16_t BL_ifReplyVersionTx     (Uint16_t bootloaderVersion, Uint16_t appVersion, Uint16_t hardwareVersion);
extern Uint16_t BL_ifReplySysStatusTx   (Uint32_t imageCrc, Bool_t appValid, IfErrorReason_t errorReason);

/*=== EXTERNAL VARIABLE DEFINITIONS ==========================================*/

// This macro will create a queue API with prefix IfQueue and for type IfMessage_t
// Needs to be placed here as it depends on the IfMessage_t typedef
DEFINE_QUEUE_TYPE(F_if, IfMessage_t);

#endif // don't include file twice
