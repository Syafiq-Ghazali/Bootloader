/*
 * CRC-32 helper for bootloader images and data clusters.
 */

#ifndef _BCRC_H_
#define _BCRC_H_

#include "Datatype.h"

#define BCRC_POLYNOMIAL      (0xEDB88320uL)
#define BCRC_TABLE_LENGTH    (256uL)

/*
 * Reflected CRC-32 (polynomial 0xEDB88320, initial register and final XOR
 * 0xFFFFFFFF). Start with crc = 0; pass a previous result to continue a CRC.
 * Each 16-bit word is processed low byte first, then high byte. Counts are
 * in 16-bit words, independent of the target's sizeof(char).
 * A zero count returns crc unchanged and permits data == NULL.
 */
extern Uint32_t BCRC_compute     (const Uint16_t *data, Uint32_t wordCount, Uint32_t crc);
extern Uint32_t BCRC_wordAppend  (Uint16_t dataWord, Uint32_t crc);
/* Append wordCount erased flash words (0xFFFF) without allocating a buffer. */
extern Uint32_t BCRC_ffffAdd     (Uint32_t wordCount, Uint32_t crc);

#endif
