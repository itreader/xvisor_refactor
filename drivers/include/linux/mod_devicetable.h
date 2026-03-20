/*
 * Device tables which are exported to userspace via
 * scripts/mod/file2alias.c.  You must keep that file in sync with this
 * header.
 */

#ifndef LINUX_MOD_DEVICETABLE_H
#define LINUX_MOD_DEVICETABLE_H

#include <linux/bitops.h>
#include <linux/types.h>

typedef uint64_t kernel_ulong_t;

#define PCI_ANY_ID (~0)

struct pci_device_id {
    uint32_t vendor;
    uint32_t devid;             /* Vendor and device ID or PCI_ANY_ID*/
    uint32_t subvendor;
    uint32_t subdevice;         /* Subsystem ID's or PCI_ANY_ID */
    uint32_t class;
    uint32_t       class_mask;  /* (class,subclass,prog-if) triplet */
    kernel_ulong_t driver_data; /* Data private to the driver */
};

#define IEEE1394_MATCH_VENDOR_ID    0x0001
#define IEEE1394_MATCH_MODEL_ID     0x0002
#define IEEE1394_MATCH_SPECIFIER_ID 0x0004
#define IEEE1394_MATCH_VERSION      0x0008

struct ieee1394_device_id {
    uint32_t       match_flags;
    uint32_t       vendor_id;
    uint32_t       model_id;
    uint32_t       specifier_id;
    uint32_t       version;
    kernel_ulong_t driver_data __attribute__((aligned(sizeof(kernel_ulong_t))));
};

/*
 * Device table entry for "new style" table-driven USB drivers.
 * User mode code can read these tables to choose which modules to load.
 * Declare the table as a MODULE_DEVICE_TABLE.
 *
 * A probe() parameter will point to a matching entry from this table.
 * Use the driver_info field for each match to hold information tied
 * to that match:  device quirks, etc.
 *
 * Terminate the driver's table with an all-zeroes entry.
 * Use the flag values to control which fields are compared.
 */

#define HID_ANY_ID (~0)

struct hid_device_id {
    uint16_t       bus;
    uint16_t       pad1;
    uint32_t       vendor;
    uint32_t       product;
    kernel_ulong_t driver_data __attribute__((aligned(sizeof(kernel_ulong_t))));
};

/* s390 CCW devices */
struct ccw_device_id {
    uint16_t match_flags; /* which fields to match against */

    uint16_t cu_type;     /* control unit type     */
    uint16_t dev_type;    /* device type           */
    uint8_t  cu_model;    /* control unit model    */
    uint8_t  dev_model;   /* device model          */

    kernel_ulong_t driver_info;
};

#define CCW_DEVICE_ID_MATCH_CU_TYPE      0x01
#define CCW_DEVICE_ID_MATCH_CU_MODEL     0x02
#define CCW_DEVICE_ID_MATCH_DEVICE_TYPE  0x04
#define CCW_DEVICE_ID_MATCH_DEVICE_MODEL 0x08

/* s390 AP bus devices */
struct ap_device_id {
    uint16_t       match_flags; /* which fields to match against */
    uint8_t        dev_type;    /* device type */
    uint8_t        pad1;
    uint32_t       pad2;
    kernel_ulong_t driver_info;
};

#define AP_DEVICE_ID_MATCH_DEVICE_TYPE 0x01

/* s390 css bus devices (subchannels) */
struct css_device_id {
    uint8_t        match_flags;
    uint8_t        type; /* subchannel type */
    uint16_t       pad2;
    uint32_t       pad3;
    kernel_ulong_t driver_data;
};

#define ACPI_ID_LEN 16 /* only 9 bytes needed here, 16 bytes are used */

/* to workaround crosscompile issues */

struct acpi_device_id {
    uint8_t        id[ACPI_ID_LEN];
    kernel_ulong_t driver_data;
};

#define PNP_ID_LEN      8
#define PNP_MAX_DEVICES 8

struct pnp_device_id {
    uint8_t        id[PNP_ID_LEN];
    kernel_ulong_t driver_data;
};

struct pnp_card_device_id {
    uint8_t        id[PNP_ID_LEN];
    kernel_ulong_t driver_data;

    struct {
        uint8_t id[PNP_ID_LEN];
    } devs[PNP_MAX_DEVICES];
};

#define SERIO_ANY 0xff

struct serio_device_id {
    uint8_t type;
    uint8_t extra;
    uint8_t id;
    uint8_t proto;
};

#if 0
/*
 * Struct used for matching a device
 */
struct of_device_id
{
    char    name[32];
    char    type[32];
    char    compatible[128];
    void    *data;
};
#endif

/* VIO */
struct vio_device_id {
    char type[32];
    char compat[32];
};

/* PCMCIA */

struct pcmcia_device_id {
    uint16_t match_flags;

    uint16_t manf_id;
    uint16_t card_id;

    uint8_t func_id;

    /* for real multi-function devices */
    uint8_t function;

    /* for pseudo multi-function devices */
    uint8_t device_no;

    uint32_t prod_id_hash[4] __attribute__((aligned(sizeof(uint32_t))));

    /* not matched against in kernelspace*/
    const char *prod_id[4];

    /* not matched against */
    kernel_ulong_t driver_info;
    char          *cisfile;
};

#define PCMCIA_DEVICE_ID_MATCH_MANF_ID      0x0001
#define PCMCIA_DEVICE_ID_MATCH_CARD_ID      0x0002
#define PCMCIA_DEVICE_ID_MATCH_FUNC_ID      0x0004
#define PCMCIA_DEVICE_ID_MATCH_FUNCTION     0x0008
#define PCMCIA_DEVICE_ID_MATCH_PROD_ID1     0x0010
#define PCMCIA_DEVICE_ID_MATCH_PROD_ID2     0x0020
#define PCMCIA_DEVICE_ID_MATCH_PROD_ID3     0x0040
#define PCMCIA_DEVICE_ID_MATCH_PROD_ID4     0x0080
#define PCMCIA_DEVICE_ID_MATCH_DEVICE_NO    0x0100
#define PCMCIA_DEVICE_ID_MATCH_FAKE_CIS     0x0200
#define PCMCIA_DEVICE_ID_MATCH_ANONYMOUS    0x0400

/* Input */
#define INPUT_DEVICE_ID_EV_MAX              0x1f
#define INPUT_DEVICE_ID_KEY_MIN_INTERESTING 0x71
#define INPUT_DEVICE_ID_KEY_MAX             0x2ff
#define INPUT_DEVICE_ID_REL_MAX             0x0f
#define INPUT_DEVICE_ID_ABS_MAX             0x3f
#define INPUT_DEVICE_ID_MSC_MAX             0x07
#define INPUT_DEVICE_ID_LED_MAX             0x0f
#define INPUT_DEVICE_ID_SND_MAX             0x07
#define INPUT_DEVICE_ID_FF_MAX              0x7f
#define INPUT_DEVICE_ID_SW_MAX              0x0f

#define INPUT_DEVICE_ID_MATCH_BUS           1
#define INPUT_DEVICE_ID_MATCH_VENDOR        2
#define INPUT_DEVICE_ID_MATCH_PRODUCT       4
#define INPUT_DEVICE_ID_MATCH_VERSION       8

#define INPUT_DEVICE_ID_MATCH_EVBIT         0x0010
#define INPUT_DEVICE_ID_MATCH_KEYBIT        0x0020
#define INPUT_DEVICE_ID_MATCH_RELBIT        0x0040
#define INPUT_DEVICE_ID_MATCH_ABSBIT        0x0080
#define INPUT_DEVICE_ID_MATCH_MSCIT         0x0100
#define INPUT_DEVICE_ID_MATCH_LEDBIT        0x0200
#define INPUT_DEVICE_ID_MATCH_SNDBIT        0x0400
#define INPUT_DEVICE_ID_MATCH_FFBIT         0x0800
#define INPUT_DEVICE_ID_MATCH_SWBIT         0x1000

struct input_deviceice_id {

    kernel_ulong_t flags;

    uint16_t bustype;
    uint16_t vendor;
    uint16_t product;
    uint16_t version;

    kernel_ulong_t evbit[INPUT_DEVICE_ID_EV_MAX / BITS_PER_LONG + 1];
    kernel_ulong_t keybit[INPUT_DEVICE_ID_KEY_MAX / BITS_PER_LONG + 1];
    kernel_ulong_t relbit[INPUT_DEVICE_ID_REL_MAX / BITS_PER_LONG + 1];
    kernel_ulong_t absbit[INPUT_DEVICE_ID_ABS_MAX / BITS_PER_LONG + 1];
    kernel_ulong_t mscbit[INPUT_DEVICE_ID_MSC_MAX / BITS_PER_LONG + 1];
    kernel_ulong_t ledbit[INPUT_DEVICE_ID_LED_MAX / BITS_PER_LONG + 1];
    kernel_ulong_t sndbit[INPUT_DEVICE_ID_SND_MAX / BITS_PER_LONG + 1];
    kernel_ulong_t ffbit[INPUT_DEVICE_ID_FF_MAX / BITS_PER_LONG + 1];
    kernel_ulong_t swbit[INPUT_DEVICE_ID_SW_MAX / BITS_PER_LONG + 1];

    kernel_ulong_t driver_info;
};

/* EISA */

#define EISA_SIG_LEN 8

/* The EISA signature, in ASCII form, null terminated */
struct eisa_device_id {
    char           sig[EISA_SIG_LEN];
    kernel_ulong_t driver_data;
};

#define EISA_DEVICE_MODALIAS_FMT "eisa:s%s"

struct parisc_device_id {
    uint8_t  hw_type;      /* 5 bits used */
    uint8_t  hversion_rev; /* 4 bits */
    uint16_t hversion;     /* 12 bits */
    uint32_t sversion;     /* 20 bits */
};

#define PA_HWTYPE_ANY_ID       0xff
#define PA_HVERSION_REV_ANY_ID 0xff
#define PA_HVERSION_ANY_ID     0xffff
#define PA_SVERSION_ANY_ID     0xffffffff

/* SDIO */

#define SDIO_ANY_ID            (~0)

struct sdio_device_id {
    uint8_t class;             /* Standard interface or SDIO_ANY_ID */
    uint16_t       vendor;     /* Vendor or SDIO_ANY_ID */
    uint16_t       device;     /* Device ID or SDIO_ANY_ID */
    kernel_ulong_t driver_data /* Data private to the driver */
        __attribute__((aligned(sizeof(kernel_ulong_t))));
};

/* SSB core, see drivers/ssb/ */
struct ssb_device_id {
    uint16_t vendor;
    uint16_t coreid;
    uint8_t  revision;
};

#define SSB_DEVICE(_vendor, _coreid, _revision)                                                                                                      \
    {                                                                                                                                                \
        .vendor = _vendor, .coreid = _coreid, .revision = _revision,                                                                                 \
    }
#define SSB_DEVTABLE_END                                                                                                                             \
    {                                                                                                                                                \
        0,                                                                                                                                           \
    },

#define SSB_ANY_VENDOR 0xFFFF
#define SSB_ANY_ID     0xFFFF
#define SSB_ANY_REV    0xFF

/* Broadcom's specific AMBA core, see drivers/bcma/ */
struct bcma_device_id {
    uint16_t manuf;
    uint16_t id;
    uint8_t  rev;
    uint8_t class;
};

#define BCMA_CORE(_manuf, _id, _rev, _class)                                                                                                         \
    {                                                                                                                                                \
        .manuf = _manuf, .id = _id, .rev = _rev, .class = _class,                                                                                    \
    }
#define BCMA_CORETABLE_END                                                                                                                           \
    {                                                                                                                                                \
        0,                                                                                                                                           \
    },

#define BCMA_ANY_MANUF 0xFFFF
#define BCMA_ANY_ID    0xFFFF
#define BCMA_ANY_REV   0xFF
#define BCMA_ANY_CLASS 0xFF

struct virtio_device_id {
    uint32_t device;
    uint32_t vendor;
};

#define VIRTIO_DEVICE_ANY_ID 0xffffffff

/* i2c */

#define I2C_NAME_SIZE        20
#define I2C_MODULE_PREFIX    "i2c:"

struct i2c_device_id {
    char           name[I2C_NAME_SIZE];
    kernel_ulong_t driver_data /* Data private to the driver */
        __attribute__((aligned(sizeof(kernel_ulong_t))));
};

/* spi */

#define SPI_NAME_SIZE     32
#define SPI_MODULE_PREFIX "spi:"

struct spi_device_id {
    char           name[SPI_NAME_SIZE];
    kernel_ulong_t driver_data /* Data private to the driver */
        __attribute__((aligned(sizeof(kernel_ulong_t))));
};

/* dmi */
enum dmi_field {
    DMI_NONE,
    DMI_BIOS_VENDOR,
    DMI_BIOS_VERSION,
    DMI_BIOS_DATE,
    DMI_SYS_VENDOR,
    DMI_PRODUCT_NAME,
    DMI_PRODUCT_VERSION,
    DMI_PRODUCT_SERIAL,
    DMI_PRODUCT_UUID,
    DMI_BOARD_VENDOR,
    DMI_BOARD_NAME,
    DMI_BOARD_VERSION,
    DMI_BOARD_SERIAL,
    DMI_BOARD_ASSET_TAG,
    DMI_CHASSIS_VENDOR,
    DMI_CHASSIS_TYPE,
    DMI_CHASSIS_VERSION,
    DMI_CHASSIS_SERIAL,
    DMI_CHASSIS_ASSET_TAG,
    DMI_STRING_MAX,
};

struct dmi_strmatch {
    unsigned char slot;
    char          substr[79];
};

struct dmi_system_id {
    int (*callback)(const struct dmi_system_id *);
    const char         *ident;
    struct dmi_strmatch matches[4];
    void               *driver_data;
};

/*
 * struct dmi_device_id appears during expansion of
 * "MODULE_DEVICE_TABLE(dmi, x)". Compiler doesn't look inside it
 * but this is enough for gcc 3.4.6 to error out:
 *  error: storage size of '__mod_dmi_device_table' isn't known
 */
#define dmi_device_id dmi_system_id

#define DMI_MATCH(a, b)                                                                                                                              \
    {                                                                                                                                                \
        a, b                                                                                                                                         \
    }

#define PLATFORM_NAME_SIZE     20
#define PLATFORM_MODULE_PREFIX "platform:"

struct platform_device_id {
    char           name[PLATFORM_NAME_SIZE];
    kernel_ulong_t driver_data __attribute__((aligned(sizeof(kernel_ulong_t))));
};

#define MDIO_MODULE_PREFIX "mdio:"

#define MDIO_ID_FMT        "%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d"
#define MDIO_ID_ARGS(_id)                                                                                                                            \
    (_id) >> 31, ((_id) >> 30) & 1, ((_id) >> 29) & 1, ((_id) >> 28) & 1, ((_id) >> 27) & 1, ((_id) >> 26) & 1, ((_id) >> 25) & 1,                   \
        ((_id) >> 24) & 1, ((_id) >> 23) & 1, ((_id) >> 22) & 1, ((_id) >> 21) & 1, ((_id) >> 20) & 1, ((_id) >> 19) & 1, ((_id) >> 18) & 1,         \
        ((_id) >> 17) & 1, ((_id) >> 16) & 1, ((_id) >> 15) & 1, ((_id) >> 14) & 1, ((_id) >> 13) & 1, ((_id) >> 12) & 1, ((_id) >> 11) & 1,         \
        ((_id) >> 10) & 1, ((_id) >> 9) & 1, ((_id) >> 8) & 1, ((_id) >> 7) & 1, ((_id) >> 6) & 1, ((_id) >> 5) & 1, ((_id) >> 4) & 1,               \
        ((_id) >> 3) & 1, ((_id) >> 2) & 1, ((_id) >> 1) & 1, (_id) & 1

/**
 * struct mdio_device_id - identifies PHY devices on an MDIO/MII bus
 * @phy_id: The result of
 *     (mdio_read(&MII_PHYSID1) << 16 | mdio_read(&PHYSID2)) & @phy_id_mask
 *     for this PHY type
 * @phy_id_mask: Defines the significant bits of @phy_id.  A value of 0
 *     is used to terminate an array of struct mdio_device_id.
 */
struct mdio_device_id {
    uint32_t phy_id;
    uint32_t phy_id_mask;
};

struct zorro_device_id {
    uint32_t       id;                         /* Device ID or ZORRO_WILDCARD */
    kernel_ulong_t driver_data;                /* Data private to the driver */
};

#define ZORRO_WILDCARD            (0xffffffff) /* not official */

#define ZORRO_DEVICE_MODALIAS_FMT "zorro:i%08X"

#define ISAPNP_ANY_ID             0xffff

struct isapnp_device_id {
    unsigned short card_vendor, card_device;
    unsigned short vendor, function;
    kernel_ulong_t driver_data; /* data private to the driver */
};

#endif                          /* LINUX_MOD_DEVICETABLE_H */
