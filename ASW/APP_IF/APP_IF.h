/* CAN command dispatch using the existing bootloader services. */
#ifndef APP_IF_H
#define APP_IF_H

#include "BL_ctx.h"
#include "BL_sm.h"

#define APP_IF_MAX_READ_WORDS (128u)
#define APP_IF_WRITE_ALIGNMENT_WORDS (8u)

extern void APP_IF_imageRefresh(CtxData_t *ctx);
extern SmState_t APP_IF_process(CtxData_t *ctx, SmState_t state);

#endif
