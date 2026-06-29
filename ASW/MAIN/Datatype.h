/*
 * Bootloader data type definitions.
 */

#ifndef _DATATYPE_H_
#define _DATATYPE_H_

#include <stdbool.h>

#ifndef TRUE
#define TRUE    ((Bool_t)1u)
#endif

#ifndef FALSE
#define FALSE   ((Bool_t)0u)
#endif

typedef bool                Bool_t;
typedef unsigned char       Uint8_t;
typedef unsigned int        Uint16_t;
typedef unsigned long       Uint32_t;
typedef unsigned long long  Uint64_t;
typedef signed char         Int8_t;
typedef signed int          Int16_t;
typedef signed long         Int32_t;
typedef signed long long    Int64_t;
typedef float               Float32_t;
typedef long double         Float64_t;

#endif
