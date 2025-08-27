#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    ISP_UPDATE_APROM = 0xa0,
    ISP_UPDATE_CONFIG = 0xa1,
    ISP_READ_CONFIG = 0xa2,
    ISP_ERASE_ALL = 0xa3,
    ISP_SYNC_PACKNO = 0xa4,
    ISP_GET_FWVER = 0xa6,
    ISP_RUN_APROM = 0xab,
    ISP_RUN_LDROM = 0xac,           // N/A
    ISP_RESET = 0xad,               // N/A
    ISP_CONNECT = 0xae,
    ISP_GET_DEVICEID = 0xb1,
    ISP_UPDATE_DATAFLASH = 0xc3,    // N/A
    ISP_GET_FLASHMODE = 0xca,       // N/A
    ISP_RESEND_PACKET = 0xff,       // N/A
};

typedef struct {
    intptr_t handle;
    size_t (*read)(intptr_t handle, void* buffer, size_t length);
    size_t (*write)(intptr_t handle, const void* buffer, size_t length);
} ISP_CONNECTION;

typedef union {
    uint64_t ll[7];
    uint32_t l[14];
    uint16_t s[28];
    uint8_t b[56];
} ISP_DATA;

bool isp_command(uint32_t code, ISP_DATA* data, ISP_CONNECTION* conn);
bool isp_write(uint32_t address, uint8_t* image, size_t length, ISP_CONNECTION* conn);
