//
// nuvotool
//
// Nuvoton ISP serial programmer
// Write HEX/BIN file to APROM
//
// https://github.com/matveyt/nuvotool
//

#include "stdz.h"
#include <errno.h>
#include <getopt.h>
#include "ihex.h"
#include "isp.h"
#include "ucomm.h"

const char program_name[] = "nuvotool";

// user options
struct {
    char* file;
    char* port;
    bool erase, config;
    uint8_t config_bytes[5];
} opt = {0};

/*noreturn*/
void help(void)
{
    fprintf(stdout,
"Usage: %s [OPTION]... [FILE]\n"
"Nuvoton ISP serial programmer. Write HEX/BIN file to APROM.\n"
"\n"
"-p, --port=PORT    Select serial device\n"
"-x, --erase        Erase APROM first\n"
"-c, --config=XX    Program CONFIG bytes\n"
"-h, --help         Show this message and exit\n",
        program_name);
    exit(EXIT_SUCCESS);
}

void parse_args(int argc, char* argv[])
{
    static struct option lopts[] = {
        { "port", required_argument, NULL, 'p' },
        { "erase", no_argument, NULL, 'x' },
        { "config", required_argument, NULL, 'c' },
        { "help", no_argument, NULL, 'h' },
        {0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "p:xc:h", lopts, NULL)) != -1) {
        switch (c) {
        case 'p':
            free(opt.port);
            opt.port = z_strdup(optarg);
        break;
        case 'x':
            opt.erase = true;
        break;
        case 'c':
            if (ihex_blob(opt.config_bytes, sizeof(opt.config_bytes), optarg)
                == sizeof(opt.config_bytes)) {
                opt.config_bytes[3] = UINT8_MAX;
                opt.config = true;
            } else {
                errno = EILSEQ;
                z_die("CONFIG");
            }
        break;
        case 'h':
            help();
        break;
        case '?':
            fprintf(stderr, "Try '%s --help' for more information.\n", program_name);
            exit(EXIT_FAILURE);
        break;
        }
    }

    if (optind == argc - 1)
        opt.file = z_strdup(argv[optind]);
}

// Nuvoton ID => Flash Size
size_t nuvoton_flashsize(uint32_t id)
{
    unsigned nib1 = (uint8_t)id >> 4;
    return (nib1 > 4) ? (18 * 1024) : (4096 << nib1);
}

// Nuvoton ID => Page Size
size_t nuvoton_pagesize(uint32_t id)
{
    return (id == 0x2f50/*N76E616*/) ? 256 : 128;
}

// CONFIG => CBS bit
bool nuvoton_cbs(const uint8_t config[5])
{
    return !!(config[0] & 0x80);
}

// CONFIG => LDROM size
size_t nuvoton_ldsize(const uint8_t config[])
{
    unsigned ldsize = (7 - (config[1] & 7)) * 1024;
    return min(ldsize, 4096);
}

int main(int argc, char* argv[])
{
    parse_args(argc, argv);

    // ISP connection
    uint8_t data[ISP_DATA_SIZE];
    ISP_CONNECTION conn = {
        .fd = ucomm_open(opt.port, 115200, 0x801/*8-N-1*/),
        .read = ucomm_read,
        .write = ucomm_write,
    };
    if (conn.fd < 0) {
        if (opt.port == NULL)
            help();
        z_die("ucomm_open");
    }
    free(opt.port);

    // assert RTS then DTR (aka. nodemcu reset)
    ucomm_rts(conn.fd, 1);
    ucomm_dtr(conn.fd, 1);
    ucomm_rts(conn.fd, 0);
    ucomm_dtr(conn.fd, 0);

    // wait for connect
    puts("Wait for connection...");
    do {
        // ISP_CONNECT
    } while (!isp_command(ISP_CONNECT, data, &conn));
    ucomm_purge(conn.fd);

    // Chip Info
    uint32_t did;
    size_t fsz, psz;
    uint8_t fw_version;
    uint8_t config[5];
    bool cbs;
    size_t ldsz;

#define ISP(code)                               \
    if (!isp_command(ISP_##code, data, &conn))  \
        z_die(#code)

    // some bootloaders expect this
    ISP(SYNC_PACKNO);

    ISP(GET_DEVICEID);
    did = (data[1] << 8) | (data[0]);
    fsz = nuvoton_flashsize(did);
    psz = nuvoton_pagesize(did);

    ISP(GET_FWVER);
    fw_version = data[0];

    ISP(READ_CONFIG);
    memcpy(config, data, sizeof(config));
    cbs = nuvoton_cbs(config);
    ldsz = nuvoton_ldsize(config);

    printf("Device ID: 0x%x\n", did);
    printf("Flash Memory: %zuKB,%zup,x%zu\n", fsz / 1024, fsz / psz, psz);
    printf("FW Version: 0x%x\n", fw_version);
    printf("CONFIG: %02x%02x%02x%02x%02x\n",
        config[0], config[1], config[2], config[3], config[4]);
    printf("LDROM: %zuKB, %sactive\n", ldsz / 1024, cbs ? "in" : "");

    // Erase
    if (opt.erase) {
        ucomm_timeout(conn.fd, 3000);
        puts("Erase APROM");
        ISP(ERASE_ALL);
        ucomm_timeout(conn.fd, 300);
    }

    // Write
    if (opt.file != NULL) {
        FILE* fin = z_fopen(opt.file, "rb");
        size_t sz, base, entry;
        uint8_t* image = ihex_load8(&sz, &base, &entry, fin);

        if (image == NULL)
            errno = ENOEXEC;
        else if (base > 0 || entry > 0)
            errno = EFAULT;
        else if (sz > fsz - ldsz)
            errno = EFBIG;
        if (errno != 0)
            z_die("ihex");

        printf("Write APROM[%zu]\n", sz);
        if (!isp_write(0, image, sz, &conn))
            z_die("isp_write");

        free(image);
        fclose(fin);
    }

    // CONFIG
    if (opt.config) {
        memcpy(data, opt.config_bytes, sizeof(opt.config_bytes));
        puts("Update CONFIG");
        ISP(UPDATE_CONFIG);
    }

    ISP(RUN_APROM);
    ucomm_close(conn.fd);
    exit(EXIT_SUCCESS);
}
