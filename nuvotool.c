//
// nuvotool
//
// Nuvoton ISP serial programmer
// Write HEX/BIN file to APROM
//
// https://github.com/matveyt/nuvotool
//

#include "stdz.h"
#include "ihx.h"
#include "isp.h"
#include "ucomm.h"

enum {
    CONFIG_LOCK, CONFIG_RPD, CONFIG_OCDEN, CONFIG_OCDPWM, CONFIG_CBS, CONFIG_LDSIZE,
    CONFIG_CBORST, CONFIG_BOIAP, CONFIG_CBOV, CONFIG_CBODEN, CONFIG_WDTEN
};

static void list_ports(void);
static size_t nuvoton_flashsize(uint32_t id);
static size_t nuvoton_pagesize(uint32_t id);
static size_t nuvoton_ldromsize(uint8_t ldsize);
static uint8_t nuvoton_ldsize(size_t ldsz);
static void print_config(const CONFIG* configp);
static int str2bit(const char* str, int value_on);
static int str2int(const char* str, const char* const* tokens, const int* numbers);

// user options
static struct {
    char* file;
    char* port;
    bool erase;
    unsigned config_flags;  // 1 << CONFIG_XXX
    CONFIG config;
} opt = {0};

/*noreturn*/
static void usage(int status)
{
    if (status != 0)
        fprintf(stderr, "Try '%s --help' for more information.\n", z_getprogname());
    else
        printf(
"Usage: %s [OPTION]... [FILE]\n"
"Nuvoton ISP serial programmer. Write HEX/BIN file to APROM.\n"
"\n"
"-p, --port=PORT        Select serial device\n"
"-x, --erase            Erase APROM first\n"
"-c, --config=X[,X...]  Setup CONFIG\n"
"-l, --list-ports       List available ports only\n"
"-h, --help             Show this message and exit\n"
"\n"
"Valid CONFIG fields: lock, rpd, ocden, ocdpwm, cbs, ldsize=0,1024,2048,3072,4096,\n"
"\tcborst, boiap, cboden, cbov=2.2,2.7,3.7,4.4, wdten=disable,enable,always\n"
"Note that '--config rpd' or '--config rpd=yes' stands for '--config rpd=0',\n"
"\twhile '--config cborst' for '--config cborst=1', etc.\n",
        z_getprogname());
    exit(status);
}

static void parse_args(int argc, char* argv[])
{
    z_setprogname(argv[0]);

    static struct z_option lopts[] = {
        { "port", z_required_argument, NULL, 'p' },
        { "erase", z_no_argument, NULL, 'x' },
        { "config", z_required_argument, NULL, 'c' },
        { "list-ports", z_no_argument, NULL, 'l' },
        { "help", z_no_argument, NULL, 'h' },
        {0}
    };

    static char* const subopts[] = {
        [CONFIG_LOCK] = "lock",
        [CONFIG_RPD] = "rpd",
        [CONFIG_OCDEN] = "ocden",
        [CONFIG_OCDPWM] = "ocdpwm",
        [CONFIG_CBS] = "cbs",
        [CONFIG_LDSIZE] = "ldsize",
        [CONFIG_CBORST] = "cborst",
        [CONFIG_BOIAP] = "boiap",
        [CONFIG_CBOV] = "cbov",
        [CONFIG_CBODEN] = "cboden",
        [CONFIG_WDTEN] = "wdten",
        NULL
    };

    int c;
    while ((c = z_getopt_long(argc, argv, "p:xc:lh", lopts, NULL)) != -1) {
        switch (c) {
        case 'p':
            free(opt.port);
            opt.port = z_strdup(z_optarg);
        break;
        case 'x':
            opt.erase = true;
        break;
        case 'c':
            do {
                char* subarg;
                int subopt = z_getsubopt(&z_optarg, subopts, &subarg);
                if (subopt < 0)
                    continue;
                opt.config_flags |= 1 << subopt;
                switch (subopt) {
                case CONFIG_LOCK:
                    opt.config.bit.LOCK = str2bit(subarg, 0);
                break;
                case CONFIG_RPD:
                    opt.config.bit.RPD = str2bit(subarg, 0);
                break;
                case CONFIG_OCDEN:
                    opt.config.bit.OCDEN = str2bit(subarg, 0);
                break;
                case CONFIG_OCDPWM:
                    opt.config.bit.OCDPWM = str2bit(subarg, 0);
                break;
                case CONFIG_CBS:
                    opt.config.bit.CBS = str2bit(subarg, 0);
                break;
                case CONFIG_LDSIZE:
                    opt.config.bit.LDSIZE = nuvoton_ldsize(strtoul(subarg, NULL, 0));
                break;
                case CONFIG_CBORST:
                    opt.config.bit.CBORST = str2bit(subarg, 1);
                break;
                case CONFIG_BOIAP:
                    opt.config.bit.BOIAP = str2bit(subarg, 1);
                break;
                case CONFIG_CBOV:
                    opt.config.bit.CBOV = str2int(subarg,
                        (const char*[]){ "2.2", "2.7", "3.7", "4.4", NULL },
                        (const int[]){ 3, 2, 1, 0 });
                break;
                case CONFIG_CBODEN:
                    opt.config.bit.CBODEN = str2bit(subarg, 1);
                break;
                case CONFIG_WDTEN:
                    opt.config.bit.WDTEN = str2int(subarg,
                        (const char*[]){ "disable", "enable", "always", NULL },
                        (const int[]){ 15, 5, 0 } );
                break;
                default:
                break;
                }
            } while (*z_optarg != 0);
        break;
        case 'l':
            list_ports();
            exit(EXIT_SUCCESS);
        break;
        case 'h':
            usage(EXIT_SUCCESS);
        break;
        case '?':
            usage(EXIT_FAILURE);
        break;
        }
    }

    if (z_optind < argc)
        opt.file = z_strdup(argv[z_optind]);
}

int main(int argc, char* argv[])
{
    parse_args(argc, argv);

    // ISP connection
    uint8_t data[ISP_DATA_SIZE];
    intptr_t isp = ucomm_open(opt.port, 115200, 0x801/*8-N-1*/);
    if (isp < 0) {
        if (opt.port != NULL)
            z_error(EXIT_FAILURE, errno, "ucomm_open(\"%s\")", opt.port);
        z_warnx(1, "missing port name");
        usage(EXIT_FAILURE);
    }
    free(opt.port);

    // assert RTS then DTR (aka nodemcu reset)
    ucomm_rts(isp, 1);
    ucomm_dtr(isp, 1);
    ucomm_rts(isp, 0);
    ucomm_dtr(isp, 0);

    // wait for connect
    puts("Wait for connection...");
    do {
        (void)ucomm_getc(isp);  // delay
        // ISP_CONNECT
    } while (!isp_command(ISP_CONNECT, data, isp));
    ucomm_purge(isp);

    // Chip Info
    uint32_t did;
    size_t fsz, psz, ldsz;
    uint8_t fw_version;
    CONFIG config;

#define ISP(code)                                       \
    if (!isp_command(ISP_##code, data, isp))            \
        z_error(EXIT_FAILURE, errno, "%s failed", #code)

    // some bootloaders expect this
    ISP(SYNC_PACKNO);

    ISP(GET_DEVICEID);
    did = (data[1] << 8) | (data[0]);
    fsz = nuvoton_flashsize(did);
    psz = nuvoton_pagesize(did);

    ISP(GET_FWVER);
    fw_version = data[0];

    ISP(READ_CONFIG);
    memcpy(config.raw, data, sizeof(CONFIG));
    ldsz = nuvoton_ldromsize(config.bit.LDSIZE);

    printf("Device ID: 0x%x\n", did);
    printf("Flash Memory: %zuKB,%zup,x%zu\n", fsz / 1024, fsz / psz, psz);
    printf("FW Version: 0x%x\n", fw_version);
    print_config(&config);

    // Erase
    if (opt.erase) {
        ucomm_timeout(isp, 3000);
        puts("Erase APROM");
        ISP(ERASE_ALL);
        ucomm_timeout(isp, UCOMM_DEFAULT_TIMEOUT);
    }

    // Write
    if (opt.file != NULL) {
        FILE* fin = z_fopen(opt.file, "rb");
        uint8_t* image;
        size_t sz, base, entry;
        if (ihx_load(&image, &sz, &base, &entry, fin) < 0)
            z_error(EXIT_FAILURE, errno, "ihx_load");
        if (base > 0 || entry > 0)
            z_error(EXIT_FAILURE, EFAULT, "ihx_load");
        if (sz > fsz - ldsz)
            z_error(EXIT_FAILURE, EFBIG, "ihx_load");

        printf("Write APROM[%zu]\n", sz);
        if (!isp_write(0, image, sz, isp))
            z_error(EXIT_FAILURE, errno, "isp_write(%zu)", sz);

        free(image);
        fclose(fin);
    }

    // CONFIG
    if (opt.config_flags != 0) {
#define MOVE_BIT(flag)                                  \
        if (opt.config_flags & (1 << CONFIG_##flag))    \
            config.bit.flag = opt.config.bit.flag
        MOVE_BIT(LOCK);
        MOVE_BIT(RPD);
        MOVE_BIT(OCDEN);
        MOVE_BIT(OCDPWM);
        MOVE_BIT(CBS);
        MOVE_BIT(LDSIZE);
        MOVE_BIT(CBORST);
        MOVE_BIT(BOIAP);
        MOVE_BIT(CBOV);
        MOVE_BIT(CBODEN);
        MOVE_BIT(WDTEN);
        memcpy(data, config.raw, sizeof(CONFIG));
        puts("Update CONFIG");
        ISP(UPDATE_CONFIG);
    }

    ISP(RUN_APROM);
    ucomm_close(isp);
    exit(EXIT_SUCCESS);
}

void list_ports(void)
{
    char** ports;
    size_t n = ucomm_ports(&ports);
    for (size_t i = 0; i < n; ++i)
        puts(ports[i]);
    printf("%zu ports found\n", n);
    free(ports);
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

// LDSIZE bits => LDROM Size
size_t nuvoton_ldromsize(uint8_t ldsz)
{
    unsigned sz = (7 - ldsz) * 1024;
    return min(sz, 4096);
}

// LDROM Size => LDSIZE bits
uint8_t nuvoton_ldsize(size_t ldsz)
{
    unsigned kb = (ldsz + 1023) / 1024;
    return (kb >= 4) ? 0 : (7 - kb);
}

void print_config(const CONFIG* configp)
{
    printf("CONFIG0 = 0x%02x\n", configp->byte.CONFIG0);
    printf("\tChip Lock\t\t%s\n", configp->bit.LOCK ? "no" : "yes");
    printf("\tReset Pin\t\t%s\n", configp->bit.RPD ? "enable" : "disable");
    printf("\tOn-Chip-Debugger\t%s\n", configp->bit.OCDEN ? "disable" : "enable");
    printf("\tPWM While OCD Halt\t%s\n", configp->bit.OCDPWM ? "tri-state" : "yes");
    printf("\tBoot Select\t\t%s\n", configp->bit.CBS ? "APROM" : "LDROM");
    printf("CONFIG1 = 0x%02x\n", configp->byte.CONFIG1);
    printf("\tLDROM Size\t\t%zu\n", nuvoton_ldromsize(configp->bit.LDSIZE));
    printf("CONFIG2 = 0x%02x\n", configp->byte.CONFIG2);
    printf("\tBrown-Out Reset\t\t%s\n", configp->bit.CBORST ? "yes" : "no");
    printf("\tBrown-Out Inhibit IAP\t%s\n", configp->bit.BOIAP ? "yes" : "no");
    printf("\tBrown-Out Voltage\t%s V\n",
        (const char*[]){ "2.2", "2.7", "3.7", "4.4" }[3 - configp->bit.CBOV]);
    printf("\tBrown-Out Detect\t%s\n", configp->bit.CBODEN ? "yes" : "no");
    printf("CONFIG3 = 0x%02x\n", configp->byte.CONFIG3);
    printf("CONFIG4 = 0x%02x\n", configp->byte.CONFIG4);
    printf("\tWatchdog Timer\t\t%s\n\n", configp->bit.WDTEN == 15 ? "disable" :
        configp->bit.WDTEN == 5 ? "enable" : "always");
}

int str2bit(const char* str, int value_on)
{
    if (!str || strcmp(str, "enable") == 0 || strcmp(str, "on") == 0
        || strcmp(str, "true") == 0 || strcmp(str, "yes") == 0)
        return value_on;
    if (strcmp(str, "disable") == 0 || strcmp(str, "off") == 0
        || strcmp(str, "false") == 0 || strcmp(str, "no") == 0)
        return !value_on;

    char* end;
    unsigned long value = strtoul(str, &end, 0);
    return (end == str) ? !value_on : !!value;
}

int str2int(const char* str, const char* const* tokens, const int* numbers)
{
    if (str) {
        for (int i = 0; tokens[i]; ++i)
            if (strcmp(str, tokens[i]) == 0)
                return numbers[i];
    }

    return numbers[0];
}
