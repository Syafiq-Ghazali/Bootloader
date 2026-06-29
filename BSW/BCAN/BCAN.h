/*
 * Basic CAN driver for the bootloader.
 */

#ifndef _BCAN_H_
#define _BCAN_H_

#include "driverlib.h"
#include "device.h"
#include "BootloaderDefs.h"
#include "BQueue.h"

#define CAN_DEFAULT_LEN             (8u)
#define CAN_ID_TX_DUMMY             (0x7FEuL)

#define CAN_TX_MSG_OBJ_ID_START     (1u)
#define CAN_TX_MSG_OBJ_SIZE         (16u)
#define CAN_TX_MSG_OBJ_ID_END       (CAN_TX_MSG_OBJ_ID_START + CAN_TX_MSG_OBJ_SIZE - (1u))

#define CAN_RX_MSG_OBJ_ID_START     (17u)
#define CAN_RX_MSG_OBJ_SIZE         (16u)
#define CAN_RX_MSG_OBJ_ID_END       (CAN_RX_MSG_OBJ_ID_START + CAN_RX_MSG_OBJ_SIZE - (1u))

#define CAN_DEFAULT_ID_MASK         (0x00000000uL)
#define CAN_MAX_RX_QUEUE_SIZE       (16u)

typedef enum CanStatus_t
{
    CAN_STATUS_RX_OK,
    CAN_STATUS_RX_FAILED,
    CAN_STATUS_RX_EMPTY,
    CAN_STATUS_TX_OK,
    CAN_STATUS_TX_FULL,
    CAN_STATUS_TX_FAILED,
} CanStatus_t;

typedef struct CanMsg_t
{
    Uint32_t id;
    Uint16_t buf[CAN_DEFAULT_LEN];
    Uint16_t bufLen;
} CanMsg_t;

DEFINE_QUEUE_TYPE(CanMsg, CanMsg_t);

typedef struct CanGpioConfig_t
{
    Uint32_t pinNumber;
    Uint32_t pinConfig;
    GPIO_QualificationMode qualification;
    Uint32_t pinType;
    GPIO_Direction direction;
} CanGpioConfig_t;

typedef struct CanHandle_t
{
    const Uint32_t base;
    const CanGpioConfig_t *txGpioConfig;
    const CanGpioConfig_t *rxGpioConfig;
    const Uint32_t bitRate;
    const Uint16_t bitTime;
    const CAN_MsgFrameType frameType;
    const Uint32_t msgIdTemplate;
    const Uint32_t mask;
    const Uint32_t interruptNumber;
    void (* const interruptHandler)(void);
    void (*interruptCallback)(struct CanHandle_t *handle);
    CanMsgQueue_t rxQueue;
    volatile Uint32_t interruptCause;
    volatile Uint32_t interruptStatus;
    volatile Uint32_t parityErrorCounter;
    volatile Uint32_t busOffCounter;
    volatile Uint32_t busErrorWarningCounter;
    volatile Uint32_t busErrorPassiveCounter;
    volatile Uint32_t txMsgCounter;
    volatile Uint32_t rxMsgCounter;
    volatile Uint32_t overFlowCounter;
} CanHandle_t;

extern void        BCAN_init       (CanHandle_t *handle);
extern CanStatus_t BCAN_tx         (CanHandle_t *handle, CanMsg_t *canMsg);
extern RAMFUNC CanStatus_t BCAN_rx (CanHandle_t *handle, CanMsg_t *canMsg);
extern Bool_t      BCAN_txPending  (CanHandle_t *handle);

#endif
