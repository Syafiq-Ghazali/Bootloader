/*
 * Bootloader CAN protocol interface.
 */

#ifndef _BL_IF_H_
#define _BL_IF_H_

#include "BootloaderDefs.h"
#include "BCAN.h"

#define BL_IF_CAN_ID_REPLY      (0x200uL)
#define BL_IF_CAN_ID_REQUEST    (0x201uL)
#define BL_IF_CAN_ID_DATA       (0x202uL)

#define BL_IF_CAN_LEN           (8u)
#define BL_IF_CAN_LEN_REPLY     (BL_IF_CAN_LEN)
#define BL_IF_CAN_LEN_REQUEST   (BL_IF_CAN_LEN)
#define BL_IF_CAN_LEN_DATA      (BL_IF_CAN_LEN)
#define BL_IF_DATA_LEN_WORDS    (BL_IF_CAN_LEN / 2u)

typedef enum IfMessageType_t
{
    BL_IF_MESSAGE_REQUEST,
    BL_IF_MESSAGE_DATA,
    BL_IF_MESSAGE_REPLY,
    BL_IF_MESSAGE_UNKNOWN,
} IfMessageType_t;

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
    BL_REQUEST_MUX_DATA,
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

typedef struct IfReplyMessage_t
{
    IfMux_t mux;
    IfRequestStatus_t requestStatus;
    IfMux_t requestMux;
    Uint16_t bootloaderVersion;
    Uint16_t appVersion;
    Uint16_t hardwareVersion;
    Uint32_t imageCrc;
    IfErrorReason_t errorReason;
    Bool_t appValid;
} IfReplyMessage_t;

typedef struct IfRequestMessage_t
{
    IfMux_t mux;
    Uint32_t appSize;
    Uint16_t appVersion;
    Uint32_t clusterAddress;
    Uint16_t clusterLength;
    Uint32_t clusterCrc;
    Uint32_t appCrc;
} IfRequestMessage_t;

typedef struct IfDataMessage_t
{
    Uint16_t dataWords[BL_IF_DATA_LEN_WORDS];
} IfDataMessage_t;

typedef struct IfMessage_t
{
    IfMessageType_t type;
    IfReplyMessage_t reply;
    IfRequestMessage_t request;
    IfDataMessage_t data;
    Uint16_t lenBytes;
} IfMessage_t;

extern void     BL_ifInit              (void);
extern Uint16_t BL_ifTx                (const IfMessage_t *msg);
extern Uint16_t BL_ifRx                (IfMessage_t *msg);
extern Bool_t   BL_ifTxPending         (void);
extern Uint16_t BL_ifReplyReqStatusTx  (IfRequestStatus_t requestStatus,
                                        IfMux_t requestMux);
extern Uint16_t BL_ifReplyVersionTx    (Uint16_t bootloaderVersion,
                                        Uint16_t appVersion,
                                        Uint16_t hardwareVersion);
extern Uint16_t BL_ifReplySysStatusTx  (Uint32_t imageCrc,
                                        Bool_t appValid,
                                        IfErrorReason_t errorReason);

#endif
