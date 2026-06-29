/*
 * Small fixed-size queue template.
 */

#ifndef _BQUEUE_H_
#define _BQUEUE_H_

#include <stdbool.h>
#include <stdint.h>

#define DEFINE_QUEUE_TYPE(NAME, TYPE)                                       \
typedef struct                                                              \
{                                                                           \
    TYPE *buf;                                                              \
    uint16_t count;                                                         \
    uint16_t head;                                                          \
    uint16_t tail;                                                          \
    uint16_t len;                                                           \
    uint32_t overflowCtr;                                                   \
} NAME##Queue_t;                                                            \
                                                                            \
static inline void NAME##Queue_init                                         \
(                                                                           \
    NAME##Queue_t *q,                                                       \
    TYPE *buffer,                                                           \
    uint16_t length                                                         \
)                                                                           \
{                                                                           \
    q->buf = buffer;                                                        \
    q->len = length;                                                        \
    q->head = 0u;                                                           \
    q->tail = 0u;                                                           \
    q->count = 0u;                                                          \
    q->overflowCtr = 0u;                                                     \
}                                                                           \
                                                                            \
static inline void NAME##Queue_push                                         \
(                                                                           \
    NAME##Queue_t *q,                                                       \
    const TYPE *in                                                          \
)                                                                           \
{                                                                           \
    if (q->count >= q->len)                                                 \
    {                                                                       \
        q->overflowCtr++;                                                   \
        return;                                                             \
    }                                                                       \
    q->buf[q->tail] = *in;                                                  \
    q->tail = (uint16_t)((q->tail + 1u) % q->len);                          \
    q->count++;                                                             \
}                                                                           \
                                                                            \
static inline void NAME##Queue_pop                                          \
(                                                                           \
    NAME##Queue_t *q,                                                       \
    TYPE *out                                                               \
)                                                                           \
{                                                                           \
    if (q->count == 0u)                                                     \
    {                                                                       \
        return;                                                             \
    }                                                                       \
    *out = q->buf[q->head];                                                 \
    q->head = (uint16_t)((q->head + 1u) % q->len);                          \
    q->count--;                                                             \
}                                                                           \
                                                                            \
static inline bool NAME##Queue_isFull                                       \
(                                                                           \
    const NAME##Queue_t *q                                                  \
)                                                                           \
{                                                                           \
    return (q->count >= q->len);                                            \
}                                                                           \
                                                                            \
static inline bool NAME##Queue_isEmpty                                      \
(                                                                           \
    const NAME##Queue_t *q                                                  \
)                                                                           \
{                                                                           \
    return (q->count == 0u);                                                \
}

#endif
