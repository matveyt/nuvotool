#if !defined(ISP_H)
#define ISP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    ISP_PACKET_SIZE = 64,
    ISP_DATA_SIZE = ISP_PACKET_SIZE - 8,

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

typedef union {
    uint8_t raw[5];
    struct { uint8_t CONFIG0, CONFIG1, CONFIG2, CONFIG3, CONFIG4; } byte;
    struct {
        uint8_t :1, LOCK:1, RPD:1, :1, OCDEN:1, OCDPWM:1, :1, CBS:1;
        uint8_t LDSIZE:3, :5;
        uint8_t :2, CBORST:1, BOIAP:1, CBOV:2, :1, CBODEN:1;
        uint8_t :8;
        uint8_t :4, WDTEN:4;
    } bit;
} CONFIG;

bool isp_command(uint32_t code, void* data, intptr_t fd);
bool isp_write(uint32_t address, uint8_t* image, size_t length, intptr_t fd);

#endif // ISP_H
