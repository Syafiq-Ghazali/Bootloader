/*
 * CRC-32 helper for bootloader images and data clusters.
 */

#ifndef _BCRC_H_
#define _BCRC_H_

#include "Datatype.h"

#define BCRC_POLYNOMIAL      (0xEDB88320uL)
#define BCRC_TABLE_LENGTH    (256uL)

extern Uint32_t BCRC_compute     (const Uint16_t *data, Uint32_t size, Uint32_t crc);
extern Uint32_t BCRC_wordAppend  (Uint16_t dataWord, Uint32_t crc);
extern Uint32_t BCRC_ffffAdd     (Uint32_t n, Uint32_t crc);

#endif
