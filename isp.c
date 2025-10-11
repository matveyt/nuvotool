#include "stdz.h"
#include "isp.h"
#include "bswap.h"
#include "ucomm.h"

// Nuvoton ISP: send one command and read response
bool isp_command(uint32_t code, void* data, intptr_t fd)
{
    static uint32_t packno = 1;

    union {
        uint8_t raw[ISP_PACKET_SIZE];
        struct {
            uint32_t code, packno;
            uint8_t data[ISP_DATA_SIZE];
        } cookie;
    } pack;
    pack.cookie.code = lsb32(code);
    pack.cookie.packno = lsb32(packno);
    memcpy(pack.cookie.data, data, ISP_DATA_SIZE);

    // calc checksum
    uint32_t checksum = 0;
    for (size_t i = 0; i < ISP_PACKET_SIZE; ++i)
        checksum += pack.raw[i];

    // send packet
    if (ucomm_write(fd, pack.raw, ISP_PACKET_SIZE) != ISP_PACKET_SIZE)
        return false;

    // read response unless mcu is reset
    if (code < ISP_RUN_APROM || code > ISP_RESET) {
        if (ucomm_read(fd, pack.raw, ISP_PACKET_SIZE) != ISP_PACKET_SIZE)
            return false;
        if (pack.cookie.code != lsb32(checksum))
            return false;
        // save response data, except APROM update
        if (code > 0)
            memcpy(data, pack.cookie.data, ISP_DATA_SIZE);
    }

    // success
    ++packno;
    return true;
}

// Nuvoton ISP: write bytes to APROM
bool isp_write(uint32_t address, uint8_t* image, size_t length, intptr_t fd)
{
    uint8_t data[ISP_DATA_SIZE];

    // first part
    size_t cnt = min(length, ISP_DATA_SIZE - 8);
    ((uint32_t*)data)[0] = lsb32(address);
    ((uint32_t*)data)[1] = lsb32(length);
    memcpy(&data[8], image, cnt);
    if (!isp_command(ISP_UPDATE_APROM, data, fd))
        return false;

    for (; cnt + ISP_DATA_SIZE <= length; cnt += ISP_DATA_SIZE)
        if (!isp_command(0, &image[cnt], fd))
            return false;

    // last incomplete part
    if (cnt < length) {
        memcpy(data, &image[cnt], length - cnt);
        if (!isp_command(0, data, fd))
            return false;
    }

    return true;
}
