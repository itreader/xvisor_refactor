/**
 * Copyright (c) 2013 Anup Patel.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @file mmc_core.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief MMC/SD/SDIO core framework interface.
 *
 * The source has been largely adapted from u-boot:
 * include/mmc.h
 *
 * Copyright 2008,2010 Freescale Semiconductor, Inc
 * Andy Fleming
 *
 * Based (loosely) on the Linux code
 *
 * The original code is licensed under the GPL.
 */

#ifndef __DRV_MMC_CORE_H__
#define __DRV_MMC_CORE_H__

#include <block/vmm_block_device.h>
#include <block/vmm_block_request_queue.h>
#include <libs/list.h>
#include <vmm_compiler.h>
#include <vmm_completion.h>
#include <vmm_mutex.h>
#include <vmm_timer.h>
#include <vmm_types.h>

#define MMC_CORE_IPRIORITY              (VMM_BLOCK_DEVICE_CLASS_IPRIORITY + 1)

/* This belongs to host.h */
#define MMC_BUS_WIDTH_1                 0
#define MMC_BUS_WIDTH_4                 2
#define MMC_BUS_WIDTH_8                 3

#define MMC_TIMING_LEGACY               0
#define MMC_TIMING_MMC_HS               1
#define MMC_TIMING_SD_HS                2
#define MMC_TIMING_UHS_SDR12            3
#define MMC_TIMING_UHS_SDR25            4
#define MMC_TIMING_UHS_SDR50            5
#define MMC_TIMING_UHS_SDR104           6
#define MMC_TIMING_UHS_DDR50            7
#define MMC_TIMING_MMC_DDR52            8
#define MMC_TIMING_MMC_HS200            9
#define MMC_TIMING_MMC_HS400            10

#define MMC_MAX_BLOCK_LEN               512
#define MMC_DATA_READ                   1
#define MMC_DATA_WRITE                  2

#define MMC_CMD_GO_IDLE_STATE           0
#define MMC_CMD_SEND_OP_COND            1
#define MMC_CMD_ALL_SEND_CID            2
#define MMC_CMD_SET_RELATIVE_ADDR       3
#define MMC_CMD_SET_DSR                 4
#define MMC_CMD_SWITCH                  6
#define MMC_CMD_SELECT_CARD             7
#define MMC_CMD_SEND_EXT_CSD            8
#define MMC_CMD_SEND_CSD                9
#define MMC_CMD_SEND_CID                10
#define MMC_CMD_STOP_TRANSMISSION       12
#define MMC_CMD_SEND_STATUS             13
#define MMC_CMD_SET_BLOCKLEN            16
#define MMC_CMD_READ_SINGLE_BLOCK       17
#define MMC_CMD_READ_MULTIPLE_BLOCK     18
#define MMC_CMD_SEND_TUNING_BLOCK       19
#define MMC_CMD_SEND_TUNING_BLOCK_HS200 21
#define MMC_CMD_SET_BLOCK_COUNT         23 /* adtc [31:0] data addr   R1  */
#define MMC_CMD_WRITE_SINGLE_BLOCK      24
#define MMC_CMD_WRITE_MULTIPLE_BLOCK    25
#define MMC_CMD_ERASE_GROUP_START       35
#define MMC_CMD_ERASE_GROUP_END         36
#define MMC_CMD_ERASE                   38
#define MMC_CMD_APP_CMD                 55
#define MMC_CMD_SPI_READ_OCR            58
#define MMC_CMD_SPI_CRC_ON_OFF          59

#define SD_CMD_SEND_RELATIVE_ADDR       3
#define SD_CMD_SWITCH_FUNC              6
#define SD_CMD_SEND_IF_COND             8

#define SD_CMD_APP_SET_BUS_WIDTH        6
#define SD_CMD_APP_SD_STATUS            13
#define SD_CMD_ERASE_WR_BLK_START       32
#define SD_CMD_ERASE_WR_BLK_END         33
#define SD_CMD_APP_SEND_OP_COND         41
#define SD_CMD_APP_SEND_SCR             51

/* SCR definitions in different words */
#define SD_HIGHSPEED_BUSY               0x00020000
#define SD_HIGHSPEED_SUPPORTED          0x00020000

#define MMC_HS_TIMING                   0x00000100
#define MMC_HS_52MHZ                    0x2

#define OCR_BUSY                        0x80000000
#define OCR_HCS                         0x40000000
#define OCR_S18R                        0x1000000
#define OCR_VOLTAGE_MASK                0x007FFF80
#define OCR_ACCESS_MODE                 0x60000000

#define SECURE_ERASE                    0x80000000

#define MMC_STATUS_MASK                 (~0x0206BF7F)
#define MMC_STATUS_RDY_FOR_DATA         (1 << 8)
#define MMC_STATUS_CURR_STATE           (0xf << 9)
#define MMC_STATUS_ERROR                (1 << 19)

#define MMC_STATE_PRG                   (7 << 9)

#define MMC_SWITCH_MODE_CMD_SET         0x00     /* Change the command set */
#define MMC_SWITCH_MODE_SET_BITS                                                                                                                     \
    0x01                                         /* Set bits in EXT_CSD byte                                                                         \
                                        addressed by index which are                                                                                 \
                                        1 in value field */
#define MMC_SWITCH_MODE_CLEAR_BITS                                                                                                                   \
    0x02                                         /* Clear bits in EXT_CSD byte                                                                       \
                                addressed by index, which are                                                                                \
                                1 in value field */
#define MMC_SWITCH_MODE_WRITE_BYTE          0x03 /* Set target byte to value */

#define SD_SWITCH_CHECK                     0
#define SD_SWITCH_SWITCH                    1

/*
 * EXT_CSD fields
 */
#define EXT_CSD_ENH_START_ADDR              136 /* R/W */
#define EXT_CSD_ENH_SIZE_MULT               140 /* R/W */
#define EXT_CSD_GP_SIZE_MULT                143 /* R/W */
#define EXT_CSD_PARTITION_SETTING           155 /* R/W */
#define EXT_CSD_PARTITIONS_ATTRIBUTE        156 /* R/W */
#define EXT_CSD_MAX_ENH_SIZE_MULT           157 /* R */
#define EXT_CSD_PARTITIONING_SUPPORT        160 /* RO */
#define EXT_CSD_RST_N_FUNCTION              162 /* R/W */
#define EXT_CSD_BKOPS_EN                    163 /* R/W & R/W/E */
#define EXT_CSD_WR_REL_PARAM                166 /* R */
#define EXT_CSD_WR_REL_SET                  167 /* R/W */
#define EXT_CSD_RPMB_MULT                   168 /* RO */
#define EXT_CSD_ERASE_GROUP_DEF             175 /* R/W */
#define EXT_CSD_BOOT_BUS_WIDTH              177
#define EXT_CSD_PART_CONF                   179 /* R/W */
#define EXT_CSD_BUS_WIDTH                   183 /* R/W */
#define EXT_CSD_HS_TIMING                   185 /* R/W */
#define EXT_CSD_REV                         192 /* RO */
#define EXT_CSD_CARD_TYPE                   196 /* RO */
#define EXT_CSD_SEC_CNT                     212 /* RO, 4 bytes */
#define EXT_CSD_HC_WP_GRP_SIZE              221 /* RO */
#define EXT_CSD_HC_ERASE_GRP_SIZE           224 /* RO */
#define EXT_CSD_BOOT_MULT                   226 /* RO */
#define EXT_CSD_BKOPS_SUPPORT               502 /* RO */

/*
 * EXT_CSD field definitions
 */

#define EXT_CSD_CMD_SET_NORMAL              (1 << 0)
#define EXT_CSD_CMD_SET_SECURE              (1 << 1)
#define EXT_CSD_CMD_SET_CPSECURE            (1 << 2)

#define EXT_CSD_CARD_TYPE_26                (1 << 0) /* Card can run at 26MHz */
#define EXT_CSD_CARD_TYPE_52                (1 << 1) /* Card can run at 52MHz */
#define EXT_CSD_CARD_TYPE_DDR_1_8V          (1 << 2)
#define EXT_CSD_CARD_TYPE_DDR_1_2V          (1 << 3)
#define EXT_CSD_CARD_TYPE_DDR_52            (EXT_CSD_CARD_TYPE_DDR_1_8V | EXT_CSD_CARD_TYPE_DDR_1_2V)

#define EXT_CSD_CARD_TYPE_HS200_1_8V        BIT(4) /* Card can run at 200MHz */
/* SDR mode @1.8V I/O */
#define EXT_CSD_CARD_TYPE_HS200_1_2V        BIT(5) /* Card can run at 200MHz */
/* SDR mode @1.2V I/O */
#define EXT_CSD_CARD_TYPE_HS200             (EXT_CSD_CARD_TYPE_HS200_1_8V | EXT_CSD_CARD_TYPE_HS200_1_2V)
#define EXT_CSD_CARD_TYPE_HS400_1_8V        BIT(6)
#define EXT_CSD_CARD_TYPE_HS400_1_2V        BIT(7)
#define EXT_CSD_CARD_TYPE_HS400             (EXT_CSD_CARD_TYPE_HS400_1_8V | EXT_CSD_CARD_TYPE_HS400_1_2V)

#define EXT_CSD_BUS_WIDTH_1                 0      /* Card is in 1 bit mode */
#define EXT_CSD_BUS_WIDTH_4                 1      /* Card is in 4 bit mode */
#define EXT_CSD_BUS_WIDTH_8                 2      /* Card is in 8 bit mode */
#define EXT_CSD_DDR_BUS_WIDTH_4             5      /* Card is in 4 bit DDR mode */
#define EXT_CSD_DDR_BUS_WIDTH_8             6      /* Card is in 8 bit DDR mode */
#define EXT_CSD_DDR_FLAG                    BIT(2) /* Flag for DDR mode */

#define EXT_CSD_TIMING_LEGACY               0      /* no high speed */
#define EXT_CSD_TIMING_HS                   1      /* HS */
#define EXT_CSD_TIMING_HS200                2      /* HS200 */
#define EXT_CSD_TIMING_HS400                3      /* HS400 */

#define EXT_CSD_BOOT_ACK_ENABLE             (1 << 6)
#define EXT_CSD_BOOT_PARTITION_ENABLE       (1 << 3)
#define EXT_CSD_PARTITION_ACCESS_ENABLE     (1 << 0)
#define EXT_CSD_PARTITION_ACCESS_DISABLE    (0 << 0)

#define EXT_CSD_BOOT_ACK(x)                 (x << 6)
#define EXT_CSD_BOOT_PART_NUM(x)            (x << 3)
#define EXT_CSD_PARTITION_ACCESS(x)         (x << 0)

#define EXT_CSD_PARTITION_SETTING_COMPLETED (1 << 0)

#define R1_ILLEGAL_COMMAND                  (1 << 22)
#define R1_APP_CMD                          (1 << 5)

#define MMC_RSP_PRESENT                     (1 << 0)
#define MMC_RSP_136                         (1 << 1) /* 136 bit response */
#define MMC_RSP_CRC                         (1 << 2) /* expect valid crc */
#define MMC_RSP_BUSY                        (1 << 3) /* card may send busy */
#define MMC_RSP_OPCODE                      (1 << 4) /* response contains opcode */

#define MMC_CMD_MASK                        (3 << 5) /* non-SPI command type */
#define MMC_CMD_AC                          (0 << 5)
#define MMC_CMD_ADTC                        (1 << 5)
#define MMC_CMD_BC                          (2 << 5)
#define MMC_CMD_BCR                         (3 << 5)

#define MMC_RSP_NONE                        (0)
#define MMC_RSP_R1                          (MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE)
#define MMC_RSP_R1b                         (MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE | MMC_RSP_BUSY)
#define MMC_RSP_R2                          (MMC_RSP_PRESENT | MMC_RSP_136 | MMC_RSP_CRC)
#define MMC_RSP_R3                          (MMC_RSP_PRESENT)
#define MMC_RSP_R4                          (MMC_RSP_PRESENT)
#define MMC_RSP_R5                          (MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE)
#define MMC_RSP_R6                          (MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE)
#define MMC_RSP_R7                          (MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE)

#define MMCPART_NOAVAILABLE                 (0xff)
#define PART_ACCESS_MASK                    (0x7)
#define PART_SUPPORT                        (0x1)
#define ENHNCD_SUPPORT                      (0x2)
#define PART_ENH_ATTRIB                     (0x1f)

#define SDIO_MAX_FUNCS                      7

struct sdio_func_tuple;
struct sdio_func;

struct sd_switch_caps {
    uint32_t hs_max_dtr;
    uint32_t uhs_max_dtr;
#define HIGH_SPEED_MAX_DTR 50000000
#define UHS_SDR104_MAX_DTR 208000000
#define UHS_SDR50_MAX_DTR  100000000
#define UHS_DDR50_MAX_DTR  50000000
#define UHS_SDR25_MAX_DTR  UHS_DDR50_MAX_DTR
#define UHS_SDR12_MAX_DTR  25000000
    uint32_t sd3_bus_mode;
#define UHS_SDR12_BUS_SPEED  0
#define HIGH_SPEED_BUS_SPEED 1
#define UHS_SDR25_BUS_SPEED  1
#define UHS_SDR50_BUS_SPEED  2
#define UHS_SDR104_BUS_SPEED 3
#define UHS_DDR50_BUS_SPEED  4

#define SD_MODE_HIGH_SPEED   (1 << HIGH_SPEED_BUS_SPEED)
#define SD_MODE_UHS_SDR12    (1 << UHS_SDR12_BUS_SPEED)
#define SD_MODE_UHS_SDR25    (1 << UHS_SDR25_BUS_SPEED)
#define SD_MODE_UHS_SDR50    (1 << UHS_SDR50_BUS_SPEED)
#define SD_MODE_UHS_SDR104   (1 << UHS_SDR104_BUS_SPEED)
#define SD_MODE_UHS_DDR50    (1 << UHS_DDR50_BUS_SPEED)
    uint32_t sd3_drv_type;
#define SD_DRIVER_TYPE_B 0x01
#define SD_DRIVER_TYPE_A 0x02
#define SD_DRIVER_TYPE_C 0x04
#define SD_DRIVER_TYPE_D 0x08
    uint32_t sd3_curr_limit;
#define SD_SET_CURRENT_LIMIT_200 0
#define SD_SET_CURRENT_LIMIT_400 1
#define SD_SET_CURRENT_LIMIT_600 2
#define SD_SET_CURRENT_LIMIT_800 3
#define SD_SET_CURRENT_NO_CHANGE (-1)

#define SD_MAX_CURRENT_200       (1 << SD_SET_CURRENT_LIMIT_200)
#define SD_MAX_CURRENT_400       (1 << SD_SET_CURRENT_LIMIT_400)
#define SD_MAX_CURRENT_600       (1 << SD_SET_CURRENT_LIMIT_600)
#define SD_MAX_CURRENT_800       (1 << SD_SET_CURRENT_LIMIT_800)
};

struct sdio_cccr {
    uint32_t sdio_vsn;
    uint32_t sd_vsn;
    uint32_t multi_block : 1, low_speed : 1, wide_bus : 1, high_power : 1, high_speed : 1, disable_cd : 1;
};

struct sdio_cis {
    unsigned short vendor;
    unsigned short device;
    unsigned short blksize;
    uint32_t       max_dtr;
};

enum mmc_voltage {
    MMC_SIGNAL_VOLTAGE_000 = 0,
    MMC_SIGNAL_VOLTAGE_120 = 1,
    MMC_SIGNAL_VOLTAGE_180 = 2,
    MMC_SIGNAL_VOLTAGE_330 = 4,
};

#define MMC_ALL_SIGNAL_VOLTAGE (MMC_SIGNAL_VOLTAGE_120 | MMC_SIGNAL_VOLTAGE_180 | MMC_SIGNAL_VOLTAGE_330)

struct mmc_cid {
    uint64_t       psn;
    unsigned short oid;
    unsigned char  mid;
    unsigned char  prv;
    unsigned char  mdt;
    char           pnm[7];
};

struct mmc_cmd {
    uint16_t cmdidx;
    uint32_t resp_type;
    uint32_t cmdarg;
    uint32_t response[4];
};

struct mmc_data {
    union {
        uint8_t       *dest;
        const uint8_t *src; /* src buffers don't get written to */
    };

    uint32_t flags;
    uint32_t blocks;
    uint32_t blocksize;
};

struct mmc_request {
    struct mmc_cmd *sbc; /* SET_BLOCK_COUNT for multiblock */
    struct mmc_cmd *cmd;
    struct mmc_cmd *stop;

    vmm_completion_t completion;
    void (*done)(struct mmc_request *); /* completion function */
    struct mmc_host *host;
};

struct mmc_ios {
    uint32_t         bus_width;
    uint32_t         clock;
    bool             clock_disable;
    enum mmc_voltage signal_voltage;
};

struct sd_ssr {
    uint32_t au;            /* In sectors */
    uint32_t erase_timeout; /* In milliseconds */
    uint32_t erase_offset;  /* In milliseconds */
};

enum mmc_bus_mode {
    MMC_LEGACY,
    SD_LEGACY,
    MMC_HS,
    SD_HS,
    MMC_HS_52,
    MMC_DDR_52,
    UHS_SDR12,
    UHS_SDR25,
    UHS_SDR50,
    UHS_DDR50,
    UHS_SDR104,
    MMC_HS_200,
    MMC_HS_400,
    MMC_MODES_END
};

#define MMC_CAP_MODE(mode) (1 << mode)

struct mmc_card {
    struct mmc_host *host; /* the host this device belongs to */
    vmm_device_t     dev;  /* the device */

    uint32_t version;
    /* SD/MMC version bits; 8 flags, 8 major, 8 minor, 8 change */
#define SD_VERSION_SD                   (1U << 31)
#define MMC_VERSION_MMC                 (1U << 30)

#define MAKE_SDMMC_VERSION(a, b, c)     ((((uint32_t)(a)) << 16) | ((uint32_t)(b) << 8) | (uint32_t)(c))
#define MAKE_SD_VERSION(a, b, c)        (SD_VERSION_SD | MAKE_SDMMC_VERSION(a, b, c))
#define MAKE_MMC_VERSION(a, b, c)       (MMC_VERSION_MMC | MAKE_SDMMC_VERSION(a, b, c))

#define EXTRACT_SDMMC_MAJOR_VERSION(x)  (((uint32_t)(x) >> 16) & 0xff)
#define EXTRACT_SDMMC_MINOR_VERSION(x)  (((uint32_t)(x) >> 8) & 0xff)
#define EXTRACT_SDMMC_CHANGE_VERSION(x) ((uint32_t)(x) & 0xff)

#define SD_VERSION_3                    MAKE_SD_VERSION(3, 0, 0)
#define SD_VERSION_2                    MAKE_SD_VERSION(2, 0, 0)
#define SD_VERSION_1_0                  MAKE_SD_VERSION(1, 0, 0)
#define SD_VERSION_1_10                 MAKE_SD_VERSION(1, 10, 0)

#define MMC_VERSION_UNKNOWN             MAKE_MMC_VERSION(0, 0, 0)
#define MMC_VERSION_1_2                 MAKE_MMC_VERSION(1, 2, 0)
#define MMC_VERSION_1_4                 MAKE_MMC_VERSION(1, 4, 0)
#define MMC_VERSION_2_2                 MAKE_MMC_VERSION(2, 2, 0)
#define MMC_VERSION_3                   MAKE_MMC_VERSION(3, 0, 0)
#define MMC_VERSION_4                   MAKE_MMC_VERSION(4, 0, 0)
#define MMC_VERSION_4_1                 MAKE_MMC_VERSION(4, 1, 0)
#define MMC_VERSION_4_2                 MAKE_MMC_VERSION(4, 2, 0)
#define MMC_VERSION_4_3                 MAKE_MMC_VERSION(4, 3, 0)
#define MMC_VERSION_4_4                 MAKE_MMC_VERSION(4, 4, 0)
#define MMC_VERSION_4_41                MAKE_MMC_VERSION(4, 4, 1)
#define MMC_VERSION_4_5                 MAKE_MMC_VERSION(4, 5, 0)
#define MMC_VERSION_5_0                 MAKE_MMC_VERSION(5, 0, 0)
#define MMC_VERSION_5_1                 MAKE_MMC_VERSION(5, 1, 0)

#define IS_SD(card)                     ((card)->version & SD_VERSION_SD)

    uint32_t caps;

    uint32_t ocr;
    uint32_t dsr;
    uint32_t dsr_imp;

    uint32_t scr[2];
#define SD_DATA_4BIT 0x00040000

    uint32_t csd[4];
    uint32_t cid[4];
    uint16_t rca;

    struct sd_ssr ssr;                          /* SD status register */

    uint8_t  ext_csd[MMC_MAX_BLOCK_LEN];
    uint32_t ext_csd_cardtype;                  /* cardtype read from the MMC */

    uint32_t type;                              /* card type */
#define MMC_TYPE_MMC      0                     /* MMC card */
#define MMC_TYPE_SD       1                     /* SD card */
#define MMC_TYPE_SDIO     2                     /* SDIO card */
#define MMC_TYPE_SD_COMBO 3                     /* SD combo (IO+mem) card */

    uint32_t state;                             /* (our) card state */
#define MMC_STATE_PRESENT     (1 << 0)          /* present in sysfs */
#define MMC_STATE_READONLY    (1 << 1)          /* card is read-only */
#define MMC_STATE_BLOCKADDR   (1 << 2)          /* card uses block-addressing */
#define MMC_CARD_SDXC         (1 << 3)          /* card is SDXC */
#define MMC_CARD_REMOVED      (1 << 4)          /* card has been removed */
#define MMC_STATE_DOING_BKOPS (1 << 5)          /* card is doing BKOPS */
#define MMC_STATE_SUSPENDED   (1 << 6)          /* card is suspended */

    uint32_t quirks;                            /* card quirks */
#define MMC_QUIRK_LENIENT_FN0          (1 << 0) /* allow SDIO FN0 writes outside of the VS CCCR range */
#define MMC_QUIRK_BLKSZ_FOR_BYTE_MODE  (1 << 1) /* use func->cur_blocksize */
    /* for byte mode */
#define MMC_QUIRK_NONSTD_SDIO          (1 << 2) /* non-standard SDIO card attached */
    /* (missing CIA registers) */
#define MMC_QUIRK_NONSTD_FUNC_IF       (1 << 3) /* SDIO card has nonstd function interfaces */
#define MMC_QUIRK_DISABLE_CD           (1 << 4) /* disconnect CD/DAT[3] resistor */
#define MMC_QUIRK_INAND_CMD38          (1 << 5) /* iNAND devices have broken CMD38 */
#define MMC_QUIRK_BLK_NO_CMD23         (1 << 6) /* Avoid CMD23 for regular multiblock */
#define MMC_QUIRK_BROKEN_BYTE_MODE_512 (1 << 7) /* Avoid sending 512 bytes in */
    /* byte mode */
#define MMC_QUIRK_LONG_READ_TIME       (1 << 8) /* Data read time > CSD says */

    uint32_t          legacy_speed;
    enum mmc_bus_mode selected_mode;            /* mode currently used */
    enum mmc_bus_mode best_mode;                /* best mode is the supported mode with the
                                                 * highest bandwidth. It may not always be the
                                                 * operating mode due to limitations when
                                                 * accessing the boot partitions
                                                 */
    uint32_t          tran_speed;
    int               ddr_mode;
    const char       *mode_name;

    int high_capacity;

    char     part_num;
    uint8_t  part_config;
    uint8_t  part_support;
    uint8_t  part_attr;
    uint8_t  wr_rel_set;
    uint32_t read_bl_len;
    uint32_t write_bl_len;
    uint32_t erase_grp_size;
    uint32_t hc_wp_grp_size;
    uint64_t capacity;
    uint64_t capacity_user;
    uint64_t capacity_boot;
    uint64_t capacity_rpmb;
    uint64_t capacity_gp[4];
    uint64_t enh_user_start;
    uint64_t enh_user_size;

    struct sd_switch_caps sw_caps;               /* switch (CMD6) caps */

    uint32_t          sdio_funcs;                /* number of SDIO functions */
    struct sdio_cccr  cccr;                      /* common card info */
    struct sdio_cis   cis;                       /* common tuple info */
    struct sdio_func *sdio_func[SDIO_MAX_FUNCS]; /* SDIO functions (devices) */
    struct sdio_func *sdio_single_irq;           /* SDIO function when only one IRQ active */
    unsigned char     sda_spec3;

    uint32_t sd_bus_speed;                       /* Bus Speed Mode set for the card */
    uint32_t mmc_avail_type;                     /* supported device type by both host and card */
    uint32_t drive_strength;                     /* for UHS-I, HS200 or HS400 */

    vmm_block_device_t *block_device;
};

struct mmc_host;

struct mmc_host_ops {
    int (*init)(struct mmc_host *mmc, int soft);
    int (*send_cmd)(struct mmc_host *mmc, struct mmc_cmd *cmd, struct mmc_data *data);
    int (*set_ios)(struct mmc_host *mmc, struct mmc_ios *ios);
    int (*init_card)(struct mmc_host *mmc, struct mmc_card *card);
    int (*get_cd)(struct mmc_host *mmc); /* Returns
                        0: No Card
                        1: Card Present */
    int (*get_wp)(struct mmc_host *mmc);
    int (*execute_tuning)(struct mmc_host *mmc, uint32_t opcode);
};

/**
 * struct mmc_slot - MMC slot functions
 *
 * @cd_irq:     MMC/SD-card slot hotplug detection IRQ or -EINVAL
 * @lock:       protect the @handler_priv pointer
 * @handler_priv:   MMC/SD-card slot context
 *
 * Some MMC/SD host controllers implement slot-functions like card and
 * write-protect detection natively. However, a large number of controllers
 * leave these functions to the CPU. This struct provides a hook to attach
 * such slot-function drivers.
 */
struct mmc_slot {
    int         cd_irq;
    vmm_mutex_t lock;
    void       *handler_private;
};

struct mmc_host {
    double_list_t link;
    vmm_device_t *dev;
    uint32_t      host_num;

    uint32_t voltages;

#define MMC_VDD_165_195 0x00000080 /* VDD voltage 1.65 - 1.95 */
#define MMC_VDD_20_21   0x00000100 /* VDD voltage 2.0 ~ 2.1 */
#define MMC_VDD_21_22   0x00000200 /* VDD voltage 2.1 ~ 2.2 */
#define MMC_VDD_22_23   0x00000400 /* VDD voltage 2.2 ~ 2.3 */
#define MMC_VDD_23_24   0x00000800 /* VDD voltage 2.3 ~ 2.4 */
#define MMC_VDD_24_25   0x00001000 /* VDD voltage 2.4 ~ 2.5 */
#define MMC_VDD_25_26   0x00002000 /* VDD voltage 2.5 ~ 2.6 */
#define MMC_VDD_26_27   0x00004000 /* VDD voltage 2.6 ~ 2.7 */
#define MMC_VDD_27_28   0x00008000 /* VDD voltage 2.7 ~ 2.8 */
#define MMC_VDD_28_29   0x00010000 /* VDD voltage 2.8 ~ 2.9 */
#define MMC_VDD_29_30   0x00020000 /* VDD voltage 2.9 ~ 3.0 */
#define MMC_VDD_30_31   0x00040000 /* VDD voltage 3.0 ~ 3.1 */
#define MMC_VDD_31_32   0x00080000 /* VDD voltage 3.1 ~ 3.2 */
#define MMC_VDD_32_33   0x00100000 /* VDD voltage 3.2 ~ 3.3 */
#define MMC_VDD_33_34   0x00200000 /* VDD voltage 3.3 ~ 3.4 */
#define MMC_VDD_34_35   0x00400000 /* VDD voltage 3.4 ~ 3.5 */
#define MMC_VDD_35_36   0x00800000 /* VDD voltage 3.5 ~ 3.6 */

    uint32_t caps;

#define MMC_CAP_MODE_LEGACY     (MMC_CAP_MODE(MMC_LEGACY) | MMC_CAP_MODE(SD_LEGACY))
#define MMC_CAP_MODE_HS         (MMC_CAP_MODE(MMC_HS) | MMC_CAP_MODE(SD_HS))
#define MMC_CAP_MODE_HS_52MHz   MMC_CAP_MODE(MMC_HS_52)
#define MMC_CAP_MODE_HS200      MMC_CAP_MODE(MMC_HS_200)
#define MMC_CAP_MODE_HS400      MMC_CAP_MODE(MMC_HS_400)
#define MMC_CAP_MODE_DDR_52MHz  MMC_CAP_MODE(MMC_DDR_52)
#define MMC_CAP_MODE_UHS_SDR12  MMC_CAP_MODE(UHS_SDR12)
#define MMC_CAP_MODE_UHS_SDR25  MMC_CAP_MODE(UHS_SDR25)
#define MMC_CAP_MODE_UHS_SDR50  MMC_CAP_MODE(UHS_SDR50)
#define MMC_CAP_MODE_UHS_SDR104 MMC_CAP_MODE(UHS_SDR104)
#define MMC_CAP_MODE_UHS_DDR50  MMC_CAP_MODE(UHS_DDR50)
#define MMC_CAP_MODE_1BIT       0x00010000
#define MMC_CAP_MODE_4BIT       0x00020000
#define MMC_CAP_MODE_8BIT       0x00040000
#define MMC_CAP_MODE_SPI        0x00080000
#define MMC_CAP_MODE_HC         0x00100000
#define MMC_CAP_NEEDS_POLL      0x00200000
#define MMC_CAP_NONREMOVABLE    0x00400000 /* Nonremovable e.g. eMMC */
#define MMC_CAP_CMD23           0x00800000 /* CMD23 supported */

#define MMC_CAP_MODE_UHS                                                                                                                             \
    (MMC_CAP_MODE(UHS_SDR12) | MMC_CAP_MODE(UHS_SDR25) | MMC_CAP_MODE(UHS_SDR50) | MMC_CAP_MODE(UHS_SDR104) | MMC_CAP_MODE(UHS_DDR50))

    uint32_t caps2;

#define MMC_CAP2_CD_ACTIVE_HIGH (1 << 10) /* Card-detect signal active high */
#define MMC_CAP2_RO_ACTIVE_HIGH (1 << 11) /* Write-protect signal active high */
#define MMC_CAP2_AUTO_CMD12     (1 << 18)

    uint32_t f_min;
    uint32_t f_max;
    uint32_t b_max;
    uint32_t ocr_avail;

    vmm_block_request_queue_t *brq;
    vmm_timer_event_t          poll_ev;

    vmm_mutex_t lock; /* Lock to proctect ops, ios, card, and private */

    struct mmc_host_ops ops;

    uint32_t max_req_size;    /* maximum bytes in one req */
    uint32_t max_block_size;  /* maximum size of one mmc block */
    uint32_t max_block_count; /* maximum number of blocks in one req */

    struct mmc_ios ios;

    struct mmc_card *card;

    struct mmc_slot slot;

    uint64_t private[0];
};

#define mmc_host_is_spi(mmc) ((mmc)->caps & MMC_CAP_MODE_SPI)

#define mmc_hostname(mmc)    ((mmc)->dev->name)

/** Detect card status change
 *  Note: This function can be called from any context.
 */
int mmc_detect_card_change(struct mmc_host *host, uint64_t msecs);

/** Allocate new mmc host instance
 *  Note: This function can be called from any context.
 */
struct mmc_host *mmc_alloc_host(int extra, vmm_device_t *dev);

/** Add mmc host instance and start mmc host thread
 *  Note: This function must be called from Orphan (or Thread) context.
 */
int mmc_add_host(struct mmc_host *host);

/** Remove mmc host instance and stop mmc host thread
 *  Note: This function must be called from Orphan (or Thread) context.
 */
void mmc_remove_host(struct mmc_host *host);

/** Free mmc host instance
 *  Note: This function can be called from any context.
 */
void mmc_free_host(struct mmc_host *host);

/** Retrive mmc host controller specific private data
 *  Note: This function can be called from any context.
 */
static inline void *mmc_private(struct mmc_host *host)
{
    return (void *)&host->private;
}

#endif /* __MMC_CORE_H__ */
