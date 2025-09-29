#if !defined(BSWAP_H)
#define BSWAP_H

#include <stdint.h>

static uint16_t lsb16(uint16_t num);
static uint16_t msb16(uint16_t num);
static uint32_t lsb32(uint32_t num);
static uint32_t msb32(uint32_t num);
static uint64_t lsb64(uint64_t num);
static uint64_t msb64(uint64_t num);
static uint16_t bswap16(uint16_t num);
static uint32_t bswap32(uint32_t num);
static uint64_t bswap64(uint64_t num);

// better compiled with -O2

inline uint16_t lsb16(uint16_t num)
{ return (*(uint8_t*)(int[]){1}) ? num : bswap16(num); }

inline uint16_t msb16(uint16_t num)
{ return (*(uint8_t*)(int[]){1}) ? bswap16(num) : num; }

inline uint32_t lsb32(uint32_t num)
{ return (*(uint8_t*)(int[]){1}) ? num : bswap32(num); }

inline uint32_t msb32(uint32_t num)
{ return (*(uint8_t*)(int[]){1}) ? bswap32(num) : num; }

inline uint64_t lsb64(uint64_t num)
{ return (*(uint8_t*)(int[]){1}) ? num : bswap64(num); }

inline uint64_t msb64(uint64_t num)
{ return (*(uint8_t*)(int[]){1}) ? bswap64(num) : num; }

inline uint16_t bswap16(uint16_t num)
{ return (num << 8) | (num >> 8); }

inline uint32_t bswap32(uint32_t num)
{
    num = ((num << 8) & 0xff00ff00) | ((num >> 8) & 0x00ff00ff);
    return (num << 16) | (num >> 16);
}

inline uint64_t bswap64(uint64_t num)
{
    num = ((num << 8) & 0xff00ff00ff00ff00) | ((num >> 8) & 0x00ff00ff00ff00ff);
    num = ((num << 16) & 0xffff0000ffff0000) | ((num >> 16) & 0x0000ffff0000ffff);
    return (num << 32) | (num >> 32);
}

#endif // BSWAP_H
