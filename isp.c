#include "isp.h"
#include "stdz.h"

// send one command and read response
bool isp_command(uint32_t code, ISP_DATA* data, ISP_CONNECTION* conn)
{
    static uint32_t packno = 1;

    union {
        struct {
            uint32_t code, packno;
            ISP_DATA data;
        } cookie;
        uint8_t raw[64];
    } pack;
    pack.cookie.code = code;
    pack.cookie.packno = packno;
    memcpy(&pack.cookie.data, data, sizeof(*data));

    // calc checksum
    uint32_t checksum = 0;
    for (size_t i = 0; i < sizeof(pack.raw); ++i)
        checksum += pack.raw[i];

    // send packet
    if (!conn->write(conn->handle, pack.raw, sizeof(pack.raw)))
        return false;

    // read response unless mcu is reset
    if (code < ISP_RUN_APROM || code > ISP_RESET) {
        if (conn->read(conn->handle, pack.raw, sizeof(pack.raw)) !=
            sizeof(pack.raw))
            return false;
        if (pack.cookie.code != checksum)
            return false;
        // save response data, except APROM update
        if (code > 0)
            memcpy(data, &pack.cookie.data, sizeof(*data));
    }

    // success
    ++packno;
    return true;
}

// write bytes to APROM
bool isp_write(uint32_t address, uint8_t* image, size_t length, ISP_CONNECTION* conn)
{
    ISP_DATA data;

    // first part
    size_t cnt = min(length, sizeof(data) - 8);
    data.l[0] = address;
    data.l[1] = length;
    memcpy(&data.b[8], image, cnt);
    if (!isp_command(ISP_UPDATE_APROM, &data, conn))
        return false;

    for (; cnt <= length - sizeof(data); cnt += sizeof(data))
        if (!isp_command(0, (ISP_DATA*)&image[cnt], conn))
            return false;

    // last incomplete part
    if (cnt < length) {
        memcpy(data.b, &image[cnt], length - cnt);
        if (!isp_command(0, &data, conn))
            return false;
    }

    return true;
}
