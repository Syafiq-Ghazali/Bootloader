/*
|===============================================================================
|
| File:         DCAN.h
|
| Project:      Agnostic
|
| Processor:    TI TMS320F28003x
|
| Compiler:     TI C2000 compiler 22.6.0
|
| Component:    BSW Header File
|
| Description:  
|               
|
| Copyright:    Copyright (C) 2025 Daanaa Resolution Inc.
|
|               All Rights Reserved. Reproduction or disclosure of this file 
|               or its Contents without the prior written consent of Daanaa 
|               Resolution Inc is prohibited.
|===============================================================================
| Version   Date          Author  Description
|-------------------------------------------------------------------------------
| 1.00      DD-MMM-2025   AP      Initial Release.
|=============================================================================*/

#ifndef _DCAN_H_
#define _DCAN_H_    /* make sure header is not included again  */

/*=== INCLUDE FILES ==========================================================*/

#include "driverlib.h"
#include "BQueue.h"

/*=== #DEFINES ===============================================================*/

#define CAN_DEFAULT_LEN                     (8u)

#define CAN_ID_TX_DUMMY                     (0x7FEuL)

#define CAN_TX_MSG_OBJ_ID_START             (1u)
#define CAN_TX_MSG_OBJ_SIZE                 (16u)
#define CAN_TX_MSG_OBJ_ID_END               (CAN_TX_MSG_OBJ_ID_START + CAN_TX_MSG_OBJ_SIZE - (1u))

#define CAN_RX_MSG_OBJ_ID_START             (17u)
#define CAN_RX_MSG_OBJ_SIZE                 (16u)
#define CAN_RX_MSG_OBJ_ID_END               (CAN_RX_MSG_OBJ_ID_START + CAN_RX_MSG_OBJ_SIZE - (1u))

#define CAN_DEFAULT_ID_MASK                 (0x00000000uL)

#define CAN_MAX_RX_QUEUE_SIZE               (16u)

/*=== TYPE DEFINITIONS =======================================================*/

/*=== ENUMERATIONS ===========================================================*/

/* Can Status states */
typedef enum CanStatus_t
{
    CAN_STATUS_RX_OK,    
    CAN_STATUS_RX_FAILED,
    CAN_STATUS_RX_EMPTY,
    CAN_STATUS_TX_OK,
    CAN_STATUS_TX_FULL,
    CAN_STATUS_TX_FAILED,    
} CanStatus_t;

/*=== STRUCTURES =============================================================*/

/* CAN message struct */
typedef struct CanMsg_t
{
    Uint32_t  id;
    Uint16_t  buf[CAN_DEFAULT_LEN];
    Uint16_t  bufLen;
} CanMsg_t;

DEFINE_QUEUE_TYPE       (CanMsg, CanMsg_t);

/* CAN handle struct */
typedef struct CanHandle_t
{
    const Uint32_t                      base;                                                   /* CAN base address */
    const GpioConfig_t                 *txGpioConfig;                                           /* CAN Tx pin GPIO config */
    const GpioConfig_t                 *rxGpioConfig;                                           /* CAN Rx pin GPIO config */
    const Uint32_t                      bitRate;                                                /* CAN bitrate */
    const Uint16_t                      bitTime;                                                /* CAN bittime */
    const CAN_MsgFrameType              frameType;                                              /* Extended or standard frame type */
    const Uint32_t                      msgIdTemplate;                                          /* Template to accept what is being taken in */
    const Uint32_t                      mask;                                                   /* mask for filtering */
    const Uint32_t                      interruptNumber;                                        /* CAN interrupt number */
    void                       (* const interruptHandler)(void);                                /* ISR installed with Interrupt_register() */
    void                             (* interruptCallback)(struct CanHandle_t *);               /* Callback function called inside ISR */                     
    CanMsgQueue_t                       rxQueue;                                                /* CAN rx queue */
    volatile Uint32_t                   interruptCause;                                         /* Most recent interrupts cause */ 
    volatile Uint32_t                   interruptStatus;                                        /* The interrupts status indicator for CAN */
    /* Status counters for debugging */
    volatile Uint32_t                   parityErrorCounter;                                     /* Counter for a parity error */         
    volatile Uint32_t                   busOffCounter;                                          /* Counter for a bus off state */
    volatile Uint32_t                   busErrorWarningCounter;                                 /* Counter for reaching a warning level */
    volatile Uint32_t                   busErrorPassiveCounter;                                 /* Counter for reaching a error passive level */
    volatile Uint32_t                   txMsgCounter;                                           /* Counter for transmit messages */
    volatile Uint32_t                   rxMsgCounter;                                           /* Counter for receive messages */
    volatile Uint32_t                   overFlowCounter;                                        /* Counter for if it overflows */
} CanHandle_t;


/*=== EXTERNAL FUNCTION PROTOTYPES ===========================================*/

extern      void             DCAN_init                   (CanHandle_t *handle);
extern      CanStatus_t      DCAN_tx                     (CanHandle_t *handle,
                                                          CanMsg_t *canMsg);
extern      CanStatus_t      DCAN_rx                     (CanHandle_t *handle,
                                                          CanMsg_t *canMsg);

/*=== EXTERNAL VARIABLE DEFINITIONS ==========================================*/

#endif // don't include file twice
