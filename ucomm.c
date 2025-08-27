//
// uComm
// A very minimal cross-platform serial port library
//
// https://github.com/matveyt/ucomm
//

#include "ucomm.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__unix__)
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#endif

#if defined(__unix__)
static unsigned unix_baudrate(unsigned baud)
{
    if (baud <= 9600)
        return B9600;
    if (baud <= 19200)
        return B19200;
    if (baud <= 38400)
        return B38400;
    if (baud <= 57600)
        return B57600;
    return B115200;
}
#endif

intptr_t ucomm_open(const char* port, unsigned baud)
{
#if defined(_WIN32)
    char fullname[11];  // \\\\.\\COMxxx
    if (port == NULL) {
        // arbitrary value
        port = "\\\\.\\COM3";
    } else if (lstrlenA(port) <= 6) {
        // COMx => \\\\.\\COMx
        lstrcpyA(fullname, "\\\\.\\");
        port = lstrcatA(fullname, port);
    }

    HANDLE fd = CreateFileA(port, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH, NULL);
    if (fd != INVALID_HANDLE_VALUE) {
        DCB dcb;
        GetCommState(fd, &dcb);
        dcb.BaudRate = baud;
        dcb.fDtrControl = DTR_CONTROL_DISABLE;
        dcb.fRtsControl = RTS_CONTROL_DISABLE;
        dcb.ByteSize = 8;
        dcb.Parity = NOPARITY;
        dcb.StopBits = ONESTOPBIT;
        SetCommState(fd, &dcb);
        ucomm_timeout((intptr_t)fd, 0);
    }
#elif defined(__unix__)
    // arbitrary value
    if (port == NULL)
        port = "/dev/ttyUSB0";

    int fd = open(port, O_RDWR | O_NOCTTY);
    if (fd != -1) {
        struct termios tio = {0};
        tio.c_iflag = IGNPAR;
        tio.c_cflag = CS8 | CREAD | CLOCAL;
        cfsetospeed(&tio, unix_baudrate(baud));
        tcsetattr(fd, TCSANOW, &tio);
    }
#endif

    return (intptr_t)fd;
}

bool ucomm_timeout(intptr_t fd, unsigned ms)
{
#if defined(_WIN32)
    COMMTIMEOUTS ct = {
        .ReadIntervalTimeout = ms ? ms : MAXDWORD,
        .ReadTotalTimeoutConstant = ms,
        .ReadTotalTimeoutMultiplier = 0,
        .WriteTotalTimeoutConstant = 0,
        .WriteTotalTimeoutMultiplier = 0,
    };
    return SetCommTimeouts((HANDLE)fd, &ct);
#elif defined(__unix__)
    struct termios tio;
    tcgetattr(fd, &tio);
    tio.c_cc[VTIME] = (ms / 100) + !!(ms % 100);
    return (tcsetattr(fd, TCSANOW, &tio) == 0);
#endif
}

bool ucomm_close(intptr_t fd)
{
#if defined(_WIN32)
    return CloseHandle((HANDLE)fd);
#elif defined(__unix__)
    return (close(fd) == 0);
#endif
}

bool ucomm_drain(intptr_t fd)
{
#if defined(_WIN32)
    return FlushFileBuffers((HANDLE)fd);
#elif defined(__unix__)
    return (tcdrain(fd) == 0);
#endif
}

bool ucomm_purge(intptr_t fd)
{
#if defined(_WIN32)
    return PurgeComm((HANDLE)fd, PURGE_RXCLEAR | PURGE_TXCLEAR);
#elif defined(__unix__)
    return (tcflush(fd, TCIOFLUSH) == 0);
#endif
}

bool ucomm_ready(intptr_t fd, int dtr_rts)
{
#if defined(_WIN32)
    return EscapeCommFunction((HANDLE)fd, (dtr_rts & 1) ? SETDTR : CLRDTR)
        && EscapeCommFunction((HANDLE)fd, (dtr_rts & 2) ? SETRTS : CLRRTS);
#elif defined(__unix__)
    int status;
    ioctl(fd, TIOCMGET, &status);
    if (dtr_rts & 1)
        status |= TIOCM_DTR;
    else
        status &= ~TIOCM_DTR;
    if (dtr_rts & 2)
        status |= TIOCM_RTS;
    else
        status &= ~TIOCM_RTS;
    return (ioctl(fd, TIOCMSET, &status) == 0);
#endif
}

size_t ucomm_read(intptr_t fd, void* buffer, size_t length)
{
    size_t sz = 0;

    do {
#if defined(_WIN32)
        DWORD part;
        ReadFile((HANDLE)fd, (uint8_t*)buffer + sz, length - sz, &part, NULL);
#elif defined(__unix__)
        ssize_t part = read(fd, (uint8_t*)buffer + sz, length - sz);
#endif
        if (part > 0)
            sz += part;
        else
            break;
    } while (sz < length);

    return sz;
}

size_t ucomm_write(intptr_t fd, const void* buffer, size_t length)
{
    size_t sz = 0;

    do {
#if defined(_WIN32)
        DWORD part;
        WriteFile((HANDLE)fd, (uint8_t*)buffer + sz, length - sz, &part, NULL);
#elif defined(__unix__)
        ssize_t part = write(fd, (uint8_t*)buffer + sz, length - sz);
#endif
        if (part > 0)
            sz += part;
        else
            break;
    } while (sz < length);

    return sz;
}
