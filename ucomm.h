//
// uComm
// A very minimal cross-platform serial port library
//
// https://github.com/matveyt/ucomm
//

#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// open port
// note: 8-N-1 non-blocking read
intptr_t ucomm_open(const char* port, unsigned baud);
// intptr_t fd = ucomm_open("com3", 9600);
// assert(fd != -1);

// close port
bool ucomm_close(intptr_t fd);
// assert(ucomm_close(fd));

// set read timeout
// note: on __unix__ timeout is rounded up to 100 ms
bool ucomm_timeout(intptr_t fd, unsigned ms);
// assert(ucomm_timeout(fd, 100));

// wait until output buffer is written
bool ucomm_drain(intptr_t fd);
// assert(ucomm_drain(fd));

// discard I/O buffers
bool ucomm_purge(intptr_t fd);
// assert(ucomm_purge(fd));

// set DTR and RTS (cf. "set" means pulldown)
// note: 1=DTR, 2=RTS, 3=Both
bool ucomm_ready(intptr_t fd, int dtr_rts);
// ucomm_ready(fd, 1);
// ucomm_ready(fd, 0);

// read data from port
size_t ucomm_read(intptr_t fd, void* buffer, size_t length);
// size_t cb = ucomm_read(fd, buf, sizeof(buf));
// if (cb == 0)
//     fputs("Timeout: no response\n", stderr);
// else if (cb < sizeof(buf))
//     fputs("Timeout: partial response\n", stderr);

// write data to port
size_t ucomm_write(intptr_t fd, const void* buffer, size_t length);
// assert(ucomm_write(fd, "Hello world!\n", 13) == 13);
