/*
 * Bootloader data cluster definitions.
 */

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
