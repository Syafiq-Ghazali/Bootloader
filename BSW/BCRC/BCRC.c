/*
 * CRC-32 helper for bootloader images and data clusters.
 */

#include "BCRC.h"

static Uint32_t F_crcTable[BCRC_TABLE_LENGTH];
static Bool_t F_crcTableInitialized = FALSE;

static Bool_t F_crcTableInit
(
    void
)
{
    Uint32_t n;

    for (n = 0u; n < BCRC_TABLE_LENGTH; n++)
    {
        Uint32_t c = n;
        Uint32_t k;

        for (k = 0u; k < 8u; k++)
        {
            c = ((c & 1u) != 0u) ? (BCRC_POLYNOMIAL ^ (c >> 1u)) : (c >> 1u);
        }

        F_crcTable[n] = c;
    }

    return TRUE;
}

Uint32_t BCRC_compute
(
    const Uint16_t *data,
    Uint32_t size,
    Uint32_t crc
)
{
    Uint32_t i;

    if (F_crcTableInitialized == FALSE)
    {
        F_crcTableInitialized = F_crcTableInit();
    }

    crc = ~crc;

    for (i = 0u; i < size; i++)
    {
        Uint16_t lowerByte = data[i] & 0xFFu;
        Uint16_t upperByte = (data[i] >> 8u) & 0xFFu;

        crc = (crc >> 8u) ^ F_crcTable[(crc ^ lowerByte) & 0xFFu];
        crc = (crc >> 8u) ^ F_crcTable[(crc ^ upperByte) & 0xFFu];
    }

    return ~crc;
}

Uint32_t BCRC_wordAppend
(
    Uint16_t dataWord,
    Uint32_t crc
)
{
    if (F_crcTableInitialized == FALSE)
    {
        F_crcTableInitialized = F_crcTableInit();
    }

    crc = ~crc;
    crc = (crc >> 8u) ^ F_crcTable[(crc ^ (dataWord & 0xFFu)) & 0xFFu];
    crc = (crc >> 8u) ^ F_crcTable[(crc ^ ((dataWord >> 8u) & 0xFFu)) & 0xFFu];

    return ~crc;
}

Uint32_t BCRC_ffffAdd
(
    Uint32_t n,
    Uint32_t crc
)
{
    Uint32_t i;

    if (F_crcTableInitialized == FALSE)
    {
        F_crcTableInitialized = F_crcTableInit();
    }

    crc = ~crc;

    for (i = 0u; i < n; i++)
    {
        crc = (crc >> 8u) ^ F_crcTable[(crc ^ 0xFFu) & 0xFFu];
    }

    return ~crc;
}
