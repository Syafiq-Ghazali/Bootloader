/*
|===============================================================================
|
| File:         DS_Queue.h
|
| Project:      ORION
|
| Processor:    TI TMS320F28003x
| Compiler:     TI C2000 compiler 22.6.2
|
| Component:    ORION Project Header File
|
| Description:  Queue Software Component Defines Constants and Function
|               prototypes
|
| Copyright:    Copyright (C) 2024-2025 Daanaa Resolution Inc.
|
|               All Rights Reserved. Reproduction or disclosure of this file
|               or its Contents without the prior written consent of Daanaa
|               Resolution Inc is prohibited.
|===============================================================================
| Version   Date        Author  Description
|-------------------------------------------------------------------------------
|  1.00   28-Aug-2025   JC     Initial Release.
|=============================================================================*/

#ifndef _DS_QUEUE_H_
#define _DS_QUEUE_H_    // make sure header is not included again

/*=== INCLUDE FILES ==========================================================*/

/*=== #DEFINES ===============================================================*/

/* Queue Template */
#define DEFINE_QUEUE_TYPE(NAME, TYPE)                                       \
typedef struct                                                              \
{                                                                           \
    TYPE*      buf;                                                         \
    Uint16_t   count;                                                       \
    Uint16_t   head;                                                        \
    Uint16_t   tail;                                                        \
    Uint16_t   len;                                                         \
    uint32_t   overflowCtr;                                                 \
} NAME##Queue_t;                                                            \
                                                                            \
static inline void NAME##Queue_init                                         \
(                                                                           \
        NAME##Queue_t*  q,                                                  \
        TYPE*           buffer,                                             \
        Uint16_t        length                                              \
)                                                                           \
{                                                                           \
    q->buf = buffer;                                                        \
    q->len = length;                                                        \
    q->head = 0;                                                            \
    q->tail = 0;                                                            \
    q->count = 0;                                                           \
    q->overflowCtr = 0;                                                     \
}                                                                           \
                                                                            \
static inline void NAME##Queue_push                                         \
(                                                                           \
    NAME##Queue_t*  q,                                                      \
    const TYPE*     in                                                      \
)                                                                           \
{                                                                           \
    if (q->count >= q->len)                                                 \
    {                                                                       \
        q->overflowCtr++;                                                   \
        return;                                                             \
    }                                                                       \
    q->buf[q->tail] = *in;                                                  \
    q->tail = (q->tail + 1) % q->len;                                       \
    q->count++;                                                             \
}                                                                           \
                                                                            \
static inline void NAME##Queue_pop                                          \
(                                                                           \
    NAME##Queue_t*  q,                                                      \
    TYPE*           out                                                     \
)                                                                           \
{                                                                           \
    if (q->count == 0) return;                                              \
    *out = q->buf[q->head];                                                 \
    q->head = (q->head + 1) % q->len;                                       \
    q->count--;                                                             \
}                                                                           \
                                                                            \
static inline Bool_t NAME##Queue_isFull                                     \
(                                                                           \
    const NAME##Queue_t* q                                                  \
)                                                                           \
{                                                                           \
    return (q->count >= q->len);                                            \
}                                                                           \
                                                                            \
static inline Bool_t NAME##Queue_isEmpty                                    \
(                                                                           \
    const NAME##Queue_t* q                                                  \
)                                                                           \
{                                                                           \
    return (q->count == 0);                                                 \
}

/*=== TYPE DEFINITIONS =======================================================*/

/*=== STRUCTURES =============================================================*/

/*=== ENUMERATIONS ===========================================================*/

/*=== EXTERNAL FUNCTION PROTOTYPES ===========================================*/

/*=== EXTERNAL VARIABLE DEFINITIONS ==========================================*/

#endif // _DS_QUEUE_H_
