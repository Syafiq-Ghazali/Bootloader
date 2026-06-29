/*
 * Request processing helpers.
 */

/*=== INCLUDE FILES ==========================================================*/

#include "BL_prcs.h"
#include "BL_ctx.h"
#include "BL_if.h"
#include "BL_img.h"
#include "BL_clstr.h"

/*=== #DEFINES ===============================================================*/

/*=== TYPE DEFINITIONS =======================================================*/

/*=== ENUMERATIONS ===========================================================*/

/*=== EXTERNALS ==============================================================*/

/*=== PRIVATE FUNCTION PROTOTYPES ============================================*/

/*=== GLOBAL DATA ============================================================*/

/*=== PRIVATE DATA ===========================================================*/

/*
|===============================================================================
|
| Function:         BL_prcsAppErase
|
| Description:      Processes a BL_APP_ERASE request
|
| Dependencies:
|
| Notes:
|
| Side Effects:
|
| Return Value:     The last request status that is sent back to the host during
|                   the request handling process
|
|===============================================================================
| Variable     Access  Description  (I=input O=output I/O=in/out)
|-------------------------------------------------------------------------------
| ctx          I/O     Bootloader context struct
|=============================================================================*/

IfRequestStatus_t BL_prcsAppErase
(
    CtxData_t *ctx
)
{
    return BL_REQUEST_STATUS_COMPLETE;
}

/*
|===============================================================================
|
| Function:
|
| Description:
|
| Dependencies:
|
| Notes:
|
| Side Effects:
|
| Return Value:
|
|===============================================================================
| Variable     Access  Description  (I=input O=output I/O=in/out)
|-------------------------------------------------------------------------------
|=============================================================================*/

/*=== End of File ============================================================*/
