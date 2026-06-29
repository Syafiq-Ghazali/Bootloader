/*
 * Application image definitions.
 */

#ifndef _BL_IMG_H_
#define _BL_IMG_H_    /* make sure header is not included again  */

/*=== INCLUDE FILES ==========================================================*/

#include "Datatype.h"
#include "BFLASH.h"
#include "BL_if.h"
#include "BootloaderDefs.h"

/*=== #DEFINES ===============================================================*/

#define STRINGIFY(s)                            #s
#define STR_VALUE(s)                            STRINGIFY(s)

#define BL_IMG_APPLICATION_START_SECTOR         (BFLASH_BANK0_SECTOR5)
#define BL_IMG_APPLICATION_END_SECTOR           (BFLASH_NUM_SECTORS)
#define BL_IMG_APPLICATION_BEGIN_ADDR           (0x85000uL)
#define BL_IMG_APPLICATION_REGION_SIZE_WORDS    (BFLASH_END + 1uL - BL_IMG_APPLICATION_BEGIN_ADDR)

#define BL_IMG_VARIABLE_METADATA_SECTOR         (BFLASH_BANK0_SECTOR3)
#define BL_IMG_FIXED_METADATA_SECTOR            (BFLASH_BANK0_SECTOR4)

#define BL_IMG_INITIAL_CRC                      (0)

/*=== TYPE DEFINITIONS =======================================================*/

/*=== STRUCTURES =============================================================*/

typedef struct ImgVariableMetadata_t
{
    Uint16_t         appVersion;
    Uint32_t         appSizeWords;
    Uint32_t         appCrc;
} ImgVariableMetadata_t;

typedef struct ImgFixedMetadata_t
{
    Uint16_t bootloaderVersion;
    Uint16_t hardwareVersion;
    Uint16_t hardwareSerialNumber;
} ImgFixedMetadata_t;

/*=== ENUMERATIONS ===========================================================*/

/*=== EXTERNAL FUNCTION PROTOTYPES ===========================================*/

extern void     BL_imgVariableMetadataRead  (ImgVariableMetadata_t *variableMetadata);
extern void     BL_imgFixedMetadataRead     (ImgFixedMetadata_t *fixedMetadata);

extern Uint16_t BL_imgVariableMetadataWrite (ImgVariableMetadata_t *variableMetadata);
extern Uint16_t BL_imgFixedMetadataWrite    (ImgFixedMetadata_t *fixedMetadata);

extern Uint16_t BL_imgVariableMetadataErase (void);
extern Uint16_t BL_imgFixedMetadataErase    (void);

extern Uint16_t BL_imgAppErase              (void);
extern Uint32_t BL_imgCrcCompute            (void);

extern void     BL_imgAppStart              (void);

/*=== EXTERNAL VARIABLE DEFINITIONS ==========================================*/

#endif // don't include file twice
