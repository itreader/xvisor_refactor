/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file frame_buffer.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Frame buffer framework header
 *
 * The source has been largely adapted from Linux 3.x or higher:
 * include/linux/frame_buffer.h
 *
 * The original code is licensed under the GPL.
 */

#ifndef __DRV_FB_H_
#define __DRV_FB_H_

#include <libs/list.h>
#include <libs/mathlib.h>
#include <libs/stringlib.h>
#include <vmm_device_driver.h>
#include <vmm_heap.h>
#include <vmm_host_io.h>
#include <vmm_mutex.h>
#include <vmm_notifier.h>
#include <vmm_types.h>

#define FB_CLASS_NAME                "fb"
#define FB_CLASS_IPRIORITY           1

/* Definitions of frame buffers                     */

#define FB_MAX                       32   /* sufficient for now */

#define FB_TYPE_PACKED_PIXELS        0    /* Packed Pixels  */
#define FB_TYPE_PLANES               1    /* Non interleaved planes */
#define FB_TYPE_INTERLEAVED_PLANES   2    /* Interleaved planes */
#define FB_TYPE_TEXT                 3    /* Text/attributes    */
#define FB_TYPE_VGA_PLANES           4    /* EGA/VGA planes */

#define FB_AUX_TEXT_MDA              0    /* Monochrome text */
#define FB_AUX_TEXT_CGA              1    /* CGA/EGA/VGA Color text */
#define FB_AUX_TEXT_S3_MMIO          2    /* S3 MMIO fasttext */
#define FB_AUX_TEXT_MGA_STEP16       3    /* MGA Millenium I: text, attr, 14 reserved bytes */
#define FB_AUX_TEXT_MGA_STEP8        4    /* other MGAs:      text, attr,  6 reserved bytes */
#define FB_AUX_TEXT_SVGA_GROUP       8    /* 8-15: SVGA tileblit compatible modes */
#define FB_AUX_TEXT_SVGA_MASK        7    /* lower three bits says step */
#define FB_AUX_TEXT_SVGA_STEP2       8    /* SVGA text mode:  text, attr */
#define FB_AUX_TEXT_SVGA_STEP4       9    /* SVGA text mode:  text, attr,  2 reserved bytes */
#define FB_AUX_TEXT_SVGA_STEP8       10   /* SVGA text mode:  text, attr,  6 reserved bytes */
#define FB_AUX_TEXT_SVGA_STEP16      11   /* SVGA text mode:  text, attr, 14 reserved bytes */
#define FB_AUX_TEXT_SVGA_LAST        15   /* reserved up to 15 */

#define FB_AUX_VGA_PLANES_VGA4       0    /* 16 color planes (EGA/VGA) */
#define FB_AUX_VGA_PLANES_CFB4       1    /* CFB4 in planes (VGA) */
#define FB_AUX_VGA_PLANES_CFB8       2    /* CFB8 in planes (VGA) */

#define FB_VISUAL_MONO01             0    /* Monochr. 1=Black 0=White */
#define FB_VISUAL_MONO10             1    /* Monochr. 1=White 0=Black */
#define FB_VISUAL_TRUECOLOR          2    /* True color */
#define FB_VISUAL_PSEUDOCOLOR        3    /* Pseudo color (like atari) */
#define FB_VISUAL_DIRECTCOLOR        4    /* Direct color */
#define FB_VISUAL_STATIC_PSEUDOCOLOR 5    /* Pseudo color readonly */

#define FB_ACCEL_NONE                0    /* no hardware accelerator    */
#define FB_ACCEL_ATARIBLITT          1    /* Atari Blitter      */
#define FB_ACCEL_AMIGABLITT          2    /* Amiga Blitter                */
#define FB_ACCEL_S3_TRIO64           3    /* Cybervision64 (S3 Trio64)    */
#define FB_ACCEL_NCR_77C32BLT        4    /* RetinaZ3 (NCR 77C32BLT)      */
#define FB_ACCEL_S3_VIRGE            5    /* Cybervision64/3D (S3 ViRGE)    */
#define FB_ACCEL_ATI_MACH64GX        6    /* ATI Mach 64GX family       */
#define FB_ACCEL_DEC_TGA             7    /* DEC 21030 TGA      */
#define FB_ACCEL_ATI_MACH64CT        8    /* ATI Mach 64CT family       */
#define FB_ACCEL_ATI_MACH64VT        9    /* ATI Mach 64CT family VT class */
#define FB_ACCEL_ATI_MACH64GT        10   /* ATI Mach 64CT family GT class */
#define FB_ACCEL_SUN_CREATOR         11   /* Sun Creator/Creator3D  */
#define FB_ACCEL_SUN_CGSIX           12   /* Sun cg6            */
#define FB_ACCEL_SUN_LEO             13   /* Sun leo/zx         */
#define FB_ACCEL_IMS_TWINTURBO       14   /* IMS Twin Turbo     */
#define FB_ACCEL_3DLABS_PERMEDIA2    15   /* 3Dlabs Permedia 2      */
#define FB_ACCEL_MATROX_MGA2064W     16   /* Matrox MGA2064W (Millenium)    */
#define FB_ACCEL_MATROX_MGA1064SG    17   /* Matrox MGA1064SG (Mystique)    */
#define FB_ACCEL_MATROX_MGA2164W     18   /* Matrox MGA2164W (Millenium II) */
#define FB_ACCEL_MATROX_MGA2164W_AGP 19   /* Matrox MGA2164W (Millenium II) */
#define FB_ACCEL_MATROX_MGAG100      20   /* Matrox G100 (Productiva G100) */
#define FB_ACCEL_MATROX_MGAG200      21   /* Matrox G200 (Myst, Mill, ...) */
#define FB_ACCEL_SUN_CG14            22   /* Sun cgfourteen      */
#define FB_ACCEL_SUN_BWTWO           23   /* Sun bwtwo          */
#define FB_ACCEL_SUN_CGTHREE         24   /* Sun cgthree            */
#define FB_ACCEL_SUN_TCX             25   /* Sun tcx            */
#define FB_ACCEL_MATROX_MGAG400      26   /* Matrox G400            */
#define FB_ACCEL_NV3                 27   /* nVidia RIVA 128              */
#define FB_ACCEL_NV4                 28   /* nVidia RIVA TNT        */
#define FB_ACCEL_NV5                 29   /* nVidia RIVA TNT2       */
#define FB_ACCEL_CT_6555x            30   /* C&T 6555x          */
#define FB_ACCEL_3DFX_BANSHEE        31   /* 3Dfx Banshee           */
#define FB_ACCEL_ATI_RAGE128         32   /* ATI Rage128 family     */
#define FB_ACCEL_IGS_CYBER2000       33   /* CyberPro 2000      */
#define FB_ACCEL_IGS_CYBER2010       34   /* CyberPro 2010      */
#define FB_ACCEL_IGS_CYBER5000       35   /* CyberPro 5000      */
#define FB_ACCEL_SIS_GLAMOUR         36   /* SiS 300/630/540              */
#define FB_ACCEL_3DLABS_PERMEDIA3    37   /* 3Dlabs Permedia 3      */
#define FB_ACCEL_ATI_RADEON          38   /* ATI Radeon family      */
#define FB_ACCEL_I810                39   /* Intel 810/815                */
#define FB_ACCEL_SIS_GLAMOUR_2       40   /* SiS 315, 650, 740      */
#define FB_ACCEL_SIS_XABRE           41   /* SiS 330 ("Xabre")      */
#define FB_ACCEL_I830                42   /* Intel 830M/845G/85x/865G     */
#define FB_ACCEL_NV_10               43   /* nVidia Arch 10               */
#define FB_ACCEL_NV_20               44   /* nVidia Arch 20               */
#define FB_ACCEL_NV_30               45   /* nVidia Arch 30               */
#define FB_ACCEL_NV_40               46   /* nVidia Arch 40               */
#define FB_ACCEL_XGI_VOLARI_V        47   /* XGI Volari V3XT, V5, V8      */
#define FB_ACCEL_XGI_VOLARI_Z        48   /* XGI Volari Z7                */
#define FB_ACCEL_OMAP1610            49   /* TI OMAP16xx                  */
#define FB_ACCEL_TRIDENT_TGUI        50   /* Trident TGUI           */
#define FB_ACCEL_TRIDENT_3DIMAGE     51   /* Trident 3DImage        */
#define FB_ACCEL_TRIDENT_BLADE3D     52   /* Trident Blade3D        */
#define FB_ACCEL_TRIDENT_BLADEXP     53   /* Trident BladeXP        */
#define FB_ACCEL_CIRRUS_ALPINE       53   /* Cirrus Logic 543x/544x/5480    */
#define FB_ACCEL_NEOMAGIC_NM2070     90   /* NeoMagic NM2070              */
#define FB_ACCEL_NEOMAGIC_NM2090     91   /* NeoMagic NM2090              */
#define FB_ACCEL_NEOMAGIC_NM2093     92   /* NeoMagic NM2093              */
#define FB_ACCEL_NEOMAGIC_NM2097     93   /* NeoMagic NM2097              */
#define FB_ACCEL_NEOMAGIC_NM2160     94   /* NeoMagic NM2160              */
#define FB_ACCEL_NEOMAGIC_NM2200     95   /* NeoMagic NM2200              */
#define FB_ACCEL_NEOMAGIC_NM2230     96   /* NeoMagic NM2230              */
#define FB_ACCEL_NEOMAGIC_NM2360     97   /* NeoMagic NM2360              */
#define FB_ACCEL_NEOMAGIC_NM2380     98   /* NeoMagic NM2380              */
#define FB_ACCEL_PXA3XX              99   /* PXA3xx         */

#define FB_ACCEL_SAVAGE4             0x80 /* S3 Savage4                   */
#define FB_ACCEL_SAVAGE3D            0x81 /* S3 Savage3D                  */
#define FB_ACCEL_SAVAGE3D_MV         0x82 /* S3 Savage3D-MV               */
#define FB_ACCEL_SAVAGE2000          0x83 /* S3 Savage2000                */
#define FB_ACCEL_SAVAGE_MX_MV        0x84 /* S3 Savage/MX-MV              */
#define FB_ACCEL_SAVAGE_MX           0x85 /* S3 Savage/MX                 */
#define FB_ACCEL_SAVAGE_IX_MV        0x86 /* S3 Savage/IX-MV              */
#define FB_ACCEL_SAVAGE_IX           0x87 /* S3 Savage/IX                 */
#define FB_ACCEL_PROSAVAGE_PM        0x88 /* S3 ProSavage PM133           */
#define FB_ACCEL_PROSAVAGE_KM        0x89 /* S3 ProSavage KM133           */
#define FB_ACCEL_S3TWISTER_P         0x8a /* S3 Twister                   */
#define FB_ACCEL_S3TWISTER_K         0x8b /* S3 TwisterK                  */
#define FB_ACCEL_SUPERSAVAGE         0x8c /* S3 Supersavage               */
#define FB_ACCEL_PROSAVAGE_DDR       0x8d /* S3 ProSavage DDR             */
#define FB_ACCEL_PROSAVAGE_DDRK      0x8e /* S3 ProSavage DDR-K           */

#define FB_ACCEL_PUV3_UNIGFX         0xa0 /* PKUnity-v3 Unigfx      */

struct frame_buffer_fix_screeninfo {
    char     id[16];                      /* identification string eg "TT Builtin" */
    uint64_t smem_start;                  /* Start of frame buffer mem */
    /* (physical address) */
    uint32_t smem_len;    /* Length of frame buffer mem */
    uint32_t type;        /* see FB_TYPE_*      */
    uint32_t type_aux;    /* Interleave for interleaved Planes */
    uint32_t visual;      /* see FB_VISUAL_*        */
    uint16_t xpanstep;    /* zero if no hardware panning  */
    uint16_t ypanstep;    /* zero if no hardware panning  */
    uint16_t ywrapstep;   /* zero if no hardware ywrap    */
    uint32_t line_length; /* length of a line in bytes    */
    uint64_t mmio_start;  /* Start of Memory Mapped I/O   */
    /* (physical address) */
    uint32_t mmio_len; /* Length of Memory Mapped I/O  */
    uint32_t accel;    /* Indicate to driver which  */
    /*  specific chip/card we have  */
    uint16_t reserved[3]; /* Reserved for future compatibility */
};

/* Interpretation of offset for color fields: All offsets are from the right,
 * inside a "pixel" value, which is exactly 'bits_per_pixel' wide (means: you
 * can use the offset as right argument to <<). A pixel afterwards is a bit
 * stream and is written to video memory as that unmodified.
 *
 * For pseudocolor: offset and length should be the same for all color
 * components. Offset specifies the position of the least significant bit
 * of the pallette index in a pixel value. Length indicates the number
 * of available palette entries (i.e. # of entries = 1 << length).
 */
struct frame_buffer_bitfield {
    uint32_t offset;    /* beginning of bitfield    */
    uint32_t length;    /* length of bitfield       */
    uint32_t msb_right; /* != 0 : Most significant bit is */
    /* right */
};

#define FB_NONSTD_HAM          1 /* Hold-And-Modify (HAM)        */
#define FB_NONSTD_REV_PIX_IN_B 2 /* order of pixels in each byte is reversed */

#define FB_ACTIVATE_NOW        0 /* set values immediately (or vbl)*/
#define FB_ACTIVATE_NXTOPEN    1 /* activate on next open   */
#define FB_ACTIVATE_TEST       2 /* don't set, round up impossible */
#define FB_ACTIVATE_MASK       15
/* values           */
#define FB_ACTIVATE_VBL        16  /* activate values on next vbl  */
#define FB_CHANGE_CMAP_VBL     32  /* change colormap on vbl    */
#define FB_ACTIVATE_ALL        64  /* change all VCs on this fb */
#define FB_ACTIVATE_FORCE      128 /* force apply even when no change*/
#define FB_ACTIVATE_INV_MODE   256 /* invalidate videomode */

#define FB_ACCELF_TEXT         1   /* (OBSOLETE) see frame_buffer_info.flags and vc_mode */

#define FB_SYNC_HOR_HIGH_ACT   1   /* horizontal sync high active   */
#define FB_SYNC_VERT_HIGH_ACT  2   /* vertical sync high active */
#define FB_SYNC_EXT            4   /* external sync     */
#define FB_SYNC_COMP_HIGH_ACT  8   /* composite sync high active   */
#define FB_SYNC_BROADCAST      16  /* broadcast video timings      */
/* vtotal = 144d/288n/576i => PAL  */
/* vtotal = 121d/242n/484i => NTSC */
#define FB_SYNC_ON_GREEN       32  /* sync on green */

#define FB_VMODE_NONINTERLACED 0   /* non interlaced */
#define FB_VMODE_INTERLACED    1   /* interlaced    */
#define FB_VMODE_DOUBLE        2   /* double scan */
#define FB_VMODE_ODD_FLD_FIRST 4   /* interlaced: top line first */
#define FB_VMODE_MASK          255

#define FB_VMODE_YWRAP         256 /* ywrap instead of panning     */
#define FB_VMODE_SMOOTH_XPAN   512 /* smooth xpan possible (internally used) */
#define FB_VMODE_CONUPDATE     512 /* don't update x/yoffset    */

/*
 * Display rotation support
 */
#define FB_ROTATE_UR           0
#define FB_ROTATE_CW           1
#define FB_ROTATE_UD           2
#define FB_ROTATE_CCW          3

#define PICOS2KHZ(a)           udiv32(1000000000UL, (a))
#define KHZ2PICOS(a)           udiv32(1000000000UL, (a))

struct frame_buffer_var_screeninfo {
    uint32_t xres;                       /* visible resolution      */
    uint32_t yres;
    uint32_t xres_virtual;               /* virtual resolution      */
    uint32_t yres_virtual;
    uint32_t xoffset;                    /* offset from virtual to visible */
    uint32_t yoffset;                    /* resolution          */

    uint32_t bits_per_pixel;             /* guess what          */
    uint32_t grayscale;                  /* != 0 Graylevels instead of colors */

    struct frame_buffer_bitfield red;    /* bitfield in fb mem if true color, */
    struct frame_buffer_bitfield green;  /* else only length is significant */
    struct frame_buffer_bitfield blue;
    struct frame_buffer_bitfield transp; /* transparency            */

    uint32_t nonstd;                     /* != 0 Non standard pixel format */

    uint32_t activate;                   /* see FB_ACTIVATE_*       */

    uint32_t height;                     /* height of picture in mm    */
    uint32_t width;                      /* width of picture in mm     */

    uint32_t accel_flags;                /* (OBSOLETE) see frame_buffer_info.flags */

    /* Timing: All values in pixclocks, except pixclock (of course) */
    uint32_t pixclock;     /* pixel clock in ps (pico seconds) */
    uint32_t left_margin;  /* time from sync to picture */
    uint32_t right_margin; /* time from picture to sync */
    uint32_t upper_margin; /* time from sync to picture */
    uint32_t lower_margin;
    uint32_t hsync_len;    /* length of horizontal sync */
    uint32_t vsync_len;    /* length of vertical sync   */
    uint32_t sync;         /* see FB_SYNC_*     */
    uint32_t vmode;        /* see FB_VMODE_*        */
    uint32_t rotate;       /* angle we rotate counter clockwise */
    uint32_t reserved[5];  /* Reserved for future compatibility */
};

struct frame_buffer_cmap {
    uint32_t  start;  /* First entry */
    uint32_t  len;    /* Number of entries */
    uint16_t *red;    /* Red values  */
    uint16_t *green;
    uint16_t *blue;
    uint16_t *transp; /* transparency, can be NULL */
};

/* VESA Blanking Levels */
#define VESA_NO_BLANKING   0
#define VESA_VSYNC_SUSPEND 1
#define VESA_HSYNC_SUSPEND 2
#define VESA_POWERDOWN     3

enum {
    /* screen: unblanked, hsync: on,  vsync: on */
    FB_BLANK_UNBLANK       = VESA_NO_BLANKING,

    /* screen: blanked,   hsync: on,  vsync: on */
    FB_BLANK_NORMAL        = VESA_NO_BLANKING + 1,

    /* screen: blanked,   hsync: on,  vsync: off */
    FB_BLANK_VSYNC_SUSPEND = VESA_VSYNC_SUSPEND + 1,

    /* screen: blanked,   hsync: off, vsync: on */
    FB_BLANK_HSYNC_SUSPEND = VESA_HSYNC_SUSPEND + 1,

    /* screen: blanked,   hsync: off, vsync: off */
    FB_BLANK_POWERDOWN     = VESA_POWERDOWN + 1
};

/* Internal HW accel */
#define ROP_COPY 0
#define ROP_XOR  1

struct frame_buffer_copyarea {
    uint32_t dx;
    uint32_t dy;
    uint32_t width;
    uint32_t height;
    uint32_t sx;
    uint32_t sy;
};

struct frame_buffer_fillrect {
    uint32_t dx; /* screen-relative */
    uint32_t dy;
    uint32_t width;
    uint32_t height;
    uint32_t color;
    uint32_t rop;
};

struct frame_buffer_image {
    uint32_t                 dx;       /* Where to place image */
    uint32_t                 dy;
    uint32_t                 width;    /* Size of image */
    uint32_t                 height;
    uint32_t                 fg_color; /* Only used when a mono bitmap */
    uint32_t                 bg_color;
    uint8_t                  depth;    /* Depth of the image */
    const char              *data;     /* Pointer to image data */
    struct frame_buffer_cmap cmap;     /* color map info */
};

/*
 * hardware cursor control
 */

#define FB_CUR_SETIMAGE 0x01
#define FB_CUR_SETPOS   0x02
#define FB_CUR_SETHOT   0x04
#define FB_CUR_SETCMAP  0x08
#define FB_CUR_SETSHAPE 0x10
#define FB_CUR_SETSIZE  0x20
#define FB_CUR_SETALL   0xFF

struct fbcurpos {
    uint16_t x, y;
};

struct fb_cursor {
    uint16_t                  set;    /* what to set */
    uint16_t                  enable; /* cursor on/off */
    uint16_t                  rop;    /* bitop operation */
    const char               *mask;   /* cursor mask bits */
    struct fbcurpos           hot;    /* cursor hot spot */
    struct frame_buffer_image image;  /* Cursor image */
};

#ifdef CONFIG_FB_BACKLIGHT
/* Settings for the generic backlight code */
#define FB_BACKLIGHT_LEVELS 128
#define FB_BACKLIGHT_MAX    0xFF
#endif

/* Definitions below are used in the parsed monitor specs */
#define FB_DPMS_ACTIVE_OFF      1
#define FB_DPMS_SUSPEND         2
#define FB_DPMS_STANDBY         4

#define FB_DISP_DDI             1
#define FB_DISP_ANA_700_300     2
#define FB_DISP_ANA_714_286     4
#define FB_DISP_ANA_1000_400    8
#define FB_DISP_ANA_700_000     16

#define FB_DISP_MONO            32
#define FB_DISP_RGB             64
#define FB_DISP_MULTI           128
#define FB_DISP_UNKNOWN         256

#define FB_SIGNAL_NONE          0
#define FB_SIGNAL_BLANK_BLANK   1
#define FB_SIGNAL_SEPARATE      2
#define FB_SIGNAL_COMPOSITE     4
#define FB_SIGNAL_SYNC_ON_GREEN 8
#define FB_SIGNAL_SERRATION_ON  16

#define FB_MISC_PRIM_COLOR      1
#define FB_MISC_1ST_DETAIL      2 /* First Detailed Timing is preferred */

struct fb_chroma {
    uint32_t redx;                /* in fraction of 1024 */
    uint32_t greenx;
    uint32_t bluex;
    uint32_t whitex;
    uint32_t redy;
    uint32_t greeny;
    uint32_t bluey;
    uint32_t whitey;
};

struct fb_monspecs {
    struct fb_chroma     chroma;
    struct fb_videomode *modedb;          /* mode database */
    uint8_t              manufacturer[4]; /* Manufacturer */
    uint8_t              monitor[14];     /* Monitor String */
    uint8_t              serial_no[14];   /* Serial Number */
    uint8_t              ascii[14];       /* ? */
    uint32_t             modedb_len;      /* mode database length */
    uint32_t             model;           /* Monitor Model */
    uint32_t             serial;          /* Serial Number - Integer */
    uint32_t             year;            /* Year manufactured */
    uint32_t             week;            /* Week Manufactured */
    uint32_t             hfmin;           /* hfreq lower limit (Hz) */
    uint32_t             hfmax;           /* hfreq upper limit (Hz) */
    uint32_t             dclkmin;         /* pixelclock lower limit (Hz) */
    uint32_t             dclkmax;         /* pixelclock upper limit (Hz) */
    uint16_t             input;           /* display type - see FB_DISP_* */
    uint16_t             dpms;            /* DPMS support - see FB_DPMS_ */
    uint16_t             signal;          /* Signal Type - see FB_SIGNAL_* */
    uint16_t             vfmin;           /* vfreq lower limit (Hz) */
    uint16_t             vfmax;           /* vfreq upper limit (Hz) */
    uint16_t             gamma;           /* Gamma - in fractions of 100 */
    uint16_t             gtf : 1;         /* supports GTF */
    uint16_t             misc;            /* Misc flags - see FB_MISC_* */
    uint8_t              version;         /* EDID version... */
    uint8_t              revision;        /* ...and revision */
    uint8_t              max_x;           /* Maximum horizontal size (cm) */
    uint8_t              max_y;           /* Maximum vertical size (cm) */
};

struct fb_blit_caps {
    uint32_t x;
    uint32_t y;
    uint32_t len;
    uint32_t flags;
};

struct frame_buffer_info;

/*
 * Register/unregister for framebuffer events
 */

/*  The resolution of the passed in frame_buffer_info about to change */
#define FB_EVENT_MODE_CHANGE       0x01
/*  The display on this frame_buffer_info is beeing suspended, no access to the
 *  framebuffer is allowed any more after that call returns
 */
#define FB_EVENT_SUSPEND           0x02
/*  The display on this frame_buffer_info was resumed, you can restore the display
 *  if you own it
 */
#define FB_EVENT_RESUME            0x03
/*      An entry from the modelist was removed */
#define FB_EVENT_MODE_DELETE       0x04
/*      A driver registered itself */
#define FB_EVENT_FB_REGISTERED     0x05
/*      A driver unregistered itself */
#define FB_EVENT_FB_UNREGISTERED   0x06
/*      CONSOLE-SPECIFIC: get console to framebuffer mapping */
#define FB_EVENT_GET_CONSOLE_MAP   0x07
/*      CONSOLE-SPECIFIC: set console to framebuffer mapping */
#define FB_EVENT_SET_CONSOLE_MAP   0x08
/*      A hardware display blank change occurred */
#define FB_EVENT_BLANK             0x09
/*      Private modelist is to be replaced */
#define FB_EVENT_NEW_MODELIST      0x0A
/*  The resolution of the passed in frame_buffer_info about to change and
        all vc's should be changed         */
#define FB_EVENT_MODE_CHANGE_ALL   0x0B
/*  A software display blank change occurred */
#define FB_EVENT_CONBLANK          0x0C
/*      Get drawing requirements        */
#define FB_EVENT_GET_REQ           0x0D
/*      Unbind from the console if possible */
#define FB_EVENT_FB_UNBIND         0x0E
/*      CONSOLE-SPECIFIC: remap all consoles to new fb - for vga switcheroo */
#define FB_EVENT_REMAP_ALL_CONSOLE 0x0F
/*  Someone opened the frame buffer */
#define FB_EVENT_OPENED            0x10
/*  Someone released the frame buffer */
#define FB_EVENT_RELEASED          0x11

struct fb_event {
    struct frame_buffer_info *info;
    void                     *data;
};

/*
 * Pixmap structure definition
 *
 * The purpose of this structure is to translate data
 * from the hardware independent format of fbdev to what
 * format the hardware needs.
 */

#define FB_PIXMAP_DEFAULT 1   /* used internally by fbcon */
#define FB_PIXMAP_SYSTEM  2   /* memory is in system RAM  */
#define FB_PIXMAP_IO      4   /* memory is iomapped       */
#define FB_PIXMAP_SYNC    256 /* set if GPU can DMA       */

struct fb_pixmap {
    uint8_t *addr;            /* pointer to memory         */
    uint32_t size;            /* size of buffer in bytes       */
    uint32_t offset;          /* current offset to buffer      */
    uint32_t buf_align;       /* byte alignment of each bitmap */
    uint32_t scan_align;      /* alignment per scanline        */
    uint32_t access_align;    /* alignment per read/write (bits)   */
    uint32_t flags;           /* see FB_PIXMAP_*           */
    uint32_t blit_x;          /* supported bit block dimensions (1-32)*/
    uint32_t blit_y;          /* Format: blit_x = 1 << (width - 1)    */
    /*         blit_y = 1 << (height - 1)   */
    /* if 0, will be set to 0xffffffff (all)*/
    /* access methods */
    void (*writeio)(struct frame_buffer_info *info, void *dst, void *src, uint32_t size);
    void (*readio)(struct frame_buffer_info *info, void *dst, void *src, uint32_t size);
};

/*
 * Frame buffer operations
 *
 * LOCKING NOTE: those functions must _ALL_ be called with the console
 * semaphore held, this is the only suitable locking mechanism we have
 * in 2.6. Some may be called at interrupt time at this point though.
 *
 * The exception to this is the debug related hooks.  Putting the fb
 * into a debug state (e.g. flipping to the kernel console) and restoring
 * it must be done in a lock-free manner, so low level drivers should
 * keep track of the initial console (if applicable) and may need to
 * perform direct, unlocked hardware writes in these hooks.
 */

struct fb_ops {
    /* open/release and usage marking */
    int (*fb_open)(struct frame_buffer_info *info, int user);
    int (*fb_release)(struct frame_buffer_info *info, int user);

    /* checks var and eventually tweaks it to something supported,
     * DO NOT MODIFY PAR */
    int (*fb_check_var)(struct frame_buffer_var_screeninfo *var, struct frame_buffer_info *info);

    /* set the video mode according to info->var */
    int (*fb_set_par)(struct frame_buffer_info *info);

    /* set shared memory base address and length */
    int (*fb_set_smem)(struct frame_buffer_info *info, uint64_t start, uint32_t len);

    /* set color register */
    int (*fb_setcolreg)(unsigned regno, unsigned red, unsigned green, unsigned blue, unsigned transp, struct frame_buffer_info *info);

    /* set color registers in batch */
    int (*fb_setcmap)(struct frame_buffer_cmap *cmap, struct frame_buffer_info *info);

    /* blank display */
    int (*fb_blank)(int blank, struct frame_buffer_info *info);

    /* pan display */
    int (*fb_pan_display)(struct frame_buffer_var_screeninfo *var, struct frame_buffer_info *info);

    /* Draws a rectangle */
    void (*frame_buffer_fillrect)(struct frame_buffer_info *info, const struct frame_buffer_fillrect *rect);
    /* Copy data from area to another */
    void (*frame_buffer_copyarea)(struct frame_buffer_info *info, const struct frame_buffer_copyarea *region);
    /* Draws a image to the display */
    void (*frame_buffer_imageblit)(struct frame_buffer_info *info, const struct frame_buffer_image *image);

    /* Draws cursor */
    int (*fb_cursor)(struct frame_buffer_info *info, struct fb_cursor *cursor);

    /* Rotates the display */
    void (*fb_rotate)(struct frame_buffer_info *info, int angle);

    /* wait for blit idle, optional */
    int (*fb_sync)(struct frame_buffer_info *info);

    /* perform fb specific ioctl (optional) */
    int (*fb_ioctl)(struct frame_buffer_info *info, uint32_t cmd, uint64_t arg);

    /* Handle 32bit compat ioctl (optional) */
    int (*fb_compat_ioctl)(struct frame_buffer_info *info, unsigned cmd, uint64_t arg);

    /* get capability given var */
    void (*fb_get_caps)(struct frame_buffer_info *info, struct fb_blit_caps *caps, struct frame_buffer_var_screeninfo *var);

    /* teardown any resources to do with this framebuffer */
    void (*fb_destroy)(struct frame_buffer_info *info);
};

#ifdef CONFIG_FB_TILEBLITTING
#define FB_TILE_CURSOR_NONE        0
#define FB_TILE_CURSOR_UNDERLINE   1
#define FB_TILE_CURSOR_LOWER_THIRD 2
#define FB_TILE_CURSOR_LOWER_HALF  3
#define FB_TILE_CURSOR_TWO_THIRDS  4
#define FB_TILE_CURSOR_BLOCK       5

struct fb_tilemap {
    uint32_t       width;  /* width of each tile in pixels */
    uint32_t       height; /* height of each tile in scanlines */
    uint32_t       depth;  /* color depth of each tile */
    uint32_t       length; /* number of tiles in the map */
    const uint8_t *data;   /* actual tile map: a bitmap array, packed
               to the nearest byte */
};

struct fb_tilerect {
    uint32_t sx;     /* origin in the x-axis */
    uint32_t sy;     /* origin in the y-axis */
    uint32_t width;  /* number of tiles in the x-axis */
    uint32_t height; /* number of tiles in the y-axis */
    uint32_t index;  /* what tile to use: index to tile map */
    uint32_t fg;     /* foreground color */
    uint32_t bg;     /* background color */
    uint32_t rop;    /* raster operation */
};

struct fb_tilearea {
    uint32_t sx;     /* source origin in the x-axis */
    uint32_t sy;     /* source origin in the y-axis */
    uint32_t dx;     /* destination origin in the x-axis */
    uint32_t dy;     /* destination origin in the y-axis */
    uint32_t width;  /* number of tiles in the x-axis */
    uint32_t height; /* number of tiles in the y-axis */
};

struct fb_tileblit {
    uint32_t  sx;      /* origin in the x-axis */
    uint32_t  sy;      /* origin in the y-axis */
    uint32_t  width;   /* number of tiles in the x-axis */
    uint32_t  height;  /* number of tiles in the y-axis */
    uint32_t  fg;      /* foreground color */
    uint32_t  bg;      /* background color */
    uint32_t  length;  /* number of tiles to draw */
    uint32_t *indices; /* array of indices to tile map */
};

struct fb_tilecursor {
    uint32_t sx;    /* cursor position in the x-axis */
    uint32_t sy;    /* cursor position in the y-axis */
    uint32_t mode;  /* 0 = erase, 1 = draw */
    uint32_t shape; /* see FB_TILE_CURSOR_* */
    uint32_t fg;    /* foreground color */
    uint32_t bg;    /* background color */
};

struct fb_tile_ops {
    /* set tile characteristics */
    void (*fb_settile)(struct frame_buffer_info *info, struct fb_tilemap *map);

    /* all dimensions from hereon are in terms of tiles */

    /* move a rectangular region of tiles from one area to another*/
    void (*fb_tilecopy)(struct frame_buffer_info *info, struct fb_tilearea *area);
    /* fill a rectangular region with a tile */
    void (*fb_tilefill)(struct frame_buffer_info *info, struct fb_tilerect *rect);
    /* copy an array of tiles */
    void (*fb_tileblit)(struct frame_buffer_info *info, struct fb_tileblit *blit);
    /* cursor */
    void (*fb_tilecursor)(struct frame_buffer_info *info, struct fb_tilecursor *cursor);
    /* get maximum length of the tile map */
    int (*fb_get_tilemax)(struct frame_buffer_info *info);
};
#endif /* CONFIG_FB_TILEBLITTING */

/* FBINFO_* = frame_buffer_info.flags bit flags */
#define FBINFO_MODULE            0x0001 /* Low-level driver is a module */
#define FBINFO_HWACCEL_DISABLED  0x0002
/* When FBINFO_HWACCEL_DISABLED is set:
 *  Hardware acceleration is turned off.  Software implementations
 *  of required functions (copyarea(), fillrect(), and imageblit())
 *  takes over; acceleration engine should be in a quiescent state */

/* hints */
#define FBINFO_VIRTFB            0x0004 /* FB is System RAM, not device. */
#define FBINFO_PARTIAL_PAN_OK    0x0040 /* otw use pan only for double-buffering */
#define FBINFO_READS_FAST        0x0080 /* soft-copy faster than rendering */

/* hardware supported ops */
/*  semantics: when a bit is set, it indicates that the operation is
 *   accelerated by hardware.
 *  required functions will still work even if the bit is not set.
 *  optional functions may not even exist if the flag bit is not set.
 */
#define FBINFO_HWACCEL_NONE      0x0000
#define FBINFO_HWACCEL_COPYAREA  0x0100   /* required */
#define FBINFO_HWACCEL_FILLRECT  0x0200   /* required */
#define FBINFO_HWACCEL_IMAGEBLIT 0x0400   /* required */
#define FBINFO_HWACCEL_ROTATE    0x0800   /* optional */
#define FBINFO_HWACCEL_XPAN      0x1000   /* optional */
#define FBINFO_HWACCEL_YPAN      0x2000   /* optional */
#define FBINFO_HWACCEL_YWRAP     0x4000   /* optional */

#define FBINFO_MISC_USEREVENT                              \
    0x10000                               /* event request \
                     from userspace */
#define FBINFO_MISC_TILEBLITTING  0x20000 /* use tile blitting */

/* A driver may set this flag to indicate that it does want a set_par to be
 * called every time when fbcon_switch is executed. The advantage is that with
 * this flag set you can really be sure that set_par is always called before
 * any of the functions dependent on the correct hardware state or altering
 * that state, even if you are using some broken X releases. The disadvantage
 * is that it introduces unwanted delays to every console switch if set_par
 * is slow. It is a good idea to try this flag in the drivers initialization
 * code whenever there is a bug report related to switching between X and the
 * framebuffer console.
 */
#define FBINFO_MISC_ALWAYS_SETPAR 0x40000

/* where the fb is a firmware driver, and can be replaced with a proper one */
#define FBINFO_MISC_FIRMWARE      0x80000
/*
 * Host and GPU endianness differ.
 */
#define FBINFO_FOREIGN_ENDIAN     0x100000
/*
 * Big endian math. This is the same flags as above, but with different
 * meaning, it is set by the fb subsystem depending FOREIGN_ENDIAN flag
 * and host endianness. Drivers should not use this flag.
 */
#define FBINFO_BE_MATH            0x100000

/* report to the VT layer that this fb driver can accept forced console
   output like oopses */
#define FBINFO_CAN_FORCE_OUTPUT   0x200000

struct fb_user {
    double_list_t head;
    void (*save)(struct frame_buffer_info *info, void *private);
    void (*restore)(struct frame_buffer_info *info, void *private);
    void *private;
};

struct frame_buffer_info {
    char          name[VMM_FIELD_NAME_SIZE];
    int           node;
    atomic_t      count;                         /* Reference counting for open operation */
    vmm_device_t  dev;                           /* This is this fb device */
    double_list_t user_list;                     /* List of fb users who have opened fb */

    int                                flags;
    vmm_mutex_t                        lock;     /* Lock for open/release/ioctl funcs */
    struct frame_buffer_var_screeninfo var;      /* Current var */
    struct frame_buffer_fix_screeninfo fix;      /* Current fix */
    struct fb_monspecs                 monspecs; /* Current Monitor specs */
    struct fb_pixmap                   pixmap;   /* Image hardware mapper */
    struct fb_pixmap                   sprite;   /* Cursor hardware mapper */
    struct frame_buffer_cmap           cmap;     /* Current cmap */
    double_list_t                      modelist; /* mode list */
    struct fb_videomode               *mode;     /* current mode */

    struct fb_ops *fbops;
#ifdef CONFIG_FB_TILEBLITTING
    struct fb_tile_ops *tileops; /* Tile Blitting */
#endif
    char    *screen_base;        /* Virtual address */
    uint64_t screen_size;        /* Amount of ioremapped VRAM or 0 */
    void    *pseudo_palette;     /* Fake palette of 16 colors */
#define FBINFO_STATE_RUNNING   0
#define FBINFO_STATE_SUSPENDED 1
    uint32_t state;              /* Hardware state i.e suspend */
    void    *fbcon_par;          /* fbcon use-only private area */
    /* From here on everything is device dependent */
    void    *par;

    /* we need the PCI or similar aperture base/size not
       smem_start/size as smem_start may just be an object
       allocated inside the aperture so may not actually overlap */
    struct apertures_struct {
        uint32_t count;

        struct aperture {
            physical_addr_t base;
            physical_size_t size;
        } ranges[0];
    } *apertures;
};

static inline struct apertures_struct *alloc_apertures(uint32_t max_num)
{
    struct apertures_struct *a = vmm_zalloc(sizeof(struct apertures_struct) + max_num * sizeof(struct aperture));

    if (!a) {
        return NULL;
    }

    a->count = max_num;
    return a;
}

/*
 *  `Generic' versions of the frame buffer device operations
 */
int  fb_check_var(struct frame_buffer_info *info, struct frame_buffer_var_screeninfo *var);
int  fb_set_var(struct frame_buffer_info *info, struct frame_buffer_var_screeninfo *var);
int  fb_get_smem(struct frame_buffer_info *info, uint64_t *start, uint32_t *len);
int  fb_set_smem(struct frame_buffer_info *info, uint64_t start, uint32_t len);
int  fb_pan_display(struct frame_buffer_info *info, struct frame_buffer_var_screeninfo *var);
int  fb_blank(struct frame_buffer_info *info, int blank);
void cframe_buffer_fillrect(struct frame_buffer_info *info, const struct frame_buffer_fillrect *rect);
void cframe_buffer_copyarea(struct frame_buffer_info *info, const struct frame_buffer_copyarea *area);
void cframe_buffer_imageblit(struct frame_buffer_info *info, const struct frame_buffer_image *image);

/*
 * Drawing operations where framebuffer is in system RAM
 */
void sys_fillrect(struct frame_buffer_info *info, const struct frame_buffer_fillrect *rect);
void sys_copyarea(struct frame_buffer_info *info, const struct frame_buffer_copyarea *area);
void sys_imageblit(struct frame_buffer_info *info, const struct frame_buffer_image *image);

/*
 * fbmem.c
 */
/** Open frame buffer. (Xvisor specific)
 *  Note: Must be called before accessing frame buffer
 */
int fb_open(
    struct frame_buffer_info *info, void (*save)(struct frame_buffer_info *, void *), void (*restore)(struct frame_buffer_info *, void *),
    void *private);

/** Release frame buffer. (Xvisor specific)
 *  Note: Must be called after accessing frame buffer
 */
int fb_release(struct frame_buffer_info *info);

/** Alloc a new frame buffer info structure
 *  NOTE: Generally frame buffer driver will allocate frame
 *  buffer in-their own way.
 */
struct frame_buffer_info *fb_alloc(size_t size, vmm_device_t *parent);

/** Free frame buffer info structure
 *  NOTE: Generally frame buffer driver will free frame buffer
 *  in-their own way.
 */
void fb_free(struct frame_buffer_info *info);

/** Remove frame buffers conflicting with given apertures */
void fb_remove_conflicting_framebuffers(struct apertures_struct *a, const char *name, bool primary);

/** Register frame buffer to device driver framework */
int fb_register(struct frame_buffer_info *info);

/** Unregister frame buffer from device driver framework */
int fb_unregister(struct frame_buffer_info *info);

/** Find a frame buffer in device driver framework */
struct frame_buffer_info *fb_find(const char *name);

/** Iterate over each frame buffer */
int fb_iterate(struct frame_buffer_info *start, void *data, int (*fn)(struct frame_buffer_info *info, void *data));

/** Get frame buffer with given number */
struct frame_buffer_info *fb_get(int num);

/** Count number of frame buffers */
uint32_t fb_count(void);

/** Acquire mutex lock on frame buffer info */
int lock_frame_buffer_info(struct frame_buffer_info *info);

/** Release mutex lock on frame buffer info */
void unlock_frame_buffer_info(struct frame_buffer_info *info);

/** Get color depth based on screen info */
int fb_get_color_depth(struct frame_buffer_var_screeninfo *var, struct frame_buffer_fix_screeninfo *fix);

/**
 *  Low level driver signals suspend
 *  @info: framebuffer affected
 *  @state: 0 = resuming, !=0 = suspending
 *
 *  This is meant to be used by low level drivers to
 *  signal suspend/resume to the core & clients.
 *  It must be called with the console semaphore held
 */
void fb_set_suspend(struct frame_buffer_info *info, int state);

static inline bool fb_be_math(struct frame_buffer_info *info)
{
#ifdef CONFIG_FB_FOREIGN_ENDIAN
#if defined(CONFIG_FB_BOTH_ENDIAN)
    return info->flags & FBINFO_BE_MATH;
#elif defined(CONFIG_FB_BIG_ENDIAN)
    return TRUE;
#elif defined(CONFIG_FB_LITTLE_ENDIAN)
    return FALSE;
#endif /* CONFIG_FB_BOTH_ENDIAN */
#else
#ifdef CONFIG_CPU_BE
    return TRUE;
#else
    return FALSE;
#endif /* CONFIG_CPU_BE */
#endif /* CONFIG_FB_FOREIGN_ENDIAN */
}

#ifdef __VMM_MODULES__
#define FBINFO_DEFAULT FBINFO_MODULE
#else
#define FBINFO_DEFAULT 0
#endif

// This will go away
#define FBINFO_FLAG_MODULE  FBINFO_MODULE
#define FBINFO_FLAG_DEFAULT FBINFO_DEFAULT

#ifdef CONFIG_X86

#define fb_readb         vmm_readb
#define fb_readw         vmm_readw
#define fb_readl         vmm_readl
#define fb_readq         vmm_readq
#define fb_writeb        vmm_writeb
#define fb_writew        vmm_writew
#define fb_writel        vmm_writel
#define fb_writeq        vmm_writeq
#define fb_memset        memset_io
#define fb_memcpy_fromfb memcpy_fromio
#define fb_memcpy_tofb   memcpy_toio

#else

#define fb_readb(addr)     (*(volatile uint8_t *)(addr))
#define fb_readw(addr)     (*(volatile uint16_t *)(addr))
#define fb_readl(addr)     (*(volatile uint32_t *)(addr))
#define fb_readq(addr)     (*(volatile uint64_t *)(addr))
#define fb_writeb(b, addr) (*(volatile uint8_t *)(addr) = (b))
#define fb_writew(b, addr) (*(volatile uint16_t *)(addr) = (b))
#define fb_writel(b, addr) (*(volatile uint32_t *)(addr) = (b))
#define fb_writeq(b, addr) (*(volatile uint64_t *)(addr) = (b))
#define fb_memset          memset
#define fb_memcpy_fromfb   memcpy
#define fb_memcpy_tofb     memcpy

#endif

#define FB_LEFT_POS(p, bpp)         (fb_be_math(p) ? (32 - (bpp)) : 0)
#define FB_SHIFT_HIGH(p, val, bits) (fb_be_math(p) ? (val) >> (bits) : (val) << (bits))
#define FB_SHIFT_LOW(p, val, bits)  (fb_be_math(p) ? (val) << (bits) : (val) >> (bits))

/*
 * fb_notify.c
 */

int fb_register_client(vmm_notifier_block_t *nb);
int fb_unregister_client(vmm_notifier_block_t *nb);
int fb_notifier_call_chain(uint64_t val, void *v);

/*
 * fbmon.c
 */
#define FB_MAXTIMINGS         0
#define FB_VSYNCTIMINGS       1
#define FB_HSYNCTIMINGS       2
#define FB_DCLKTIMINGS        3
#define FB_IGNOREMON          0x100

#define FB_MODE_IS_UNKNOWN    0
#define FB_MODE_IS_DETAILED   1
#define FB_MODE_IS_STANDARD   2
#define FB_MODE_IS_VESA       4
#define FB_MODE_IS_CALCULATED 8
#define FB_MODE_IS_FIRST      16
#define FB_MODE_IS_FROM_VAR   32

int  fb_get_mode(int flags, uint32_t val, struct frame_buffer_var_screeninfo *var, struct frame_buffer_info *info);
int  fb_validate_mode(const struct frame_buffer_var_screeninfo *var, struct frame_buffer_info *info);
int  fb_parse_edid(unsigned char *edid, struct frame_buffer_var_screeninfo *var);
void fb_edid_to_monspecs(unsigned char *edid, struct fb_monspecs *specs);
void fb_edid_add_monspecs(unsigned char *edid, struct fb_monspecs *specs);
void fb_destroy_modedb(struct fb_videomode *modedb);

/*
 * fbcvt.c
 */
int fb_find_mode_cvt(struct fb_videomode *mode, int margins, int rb);

/*
 * modedb.c
 */
#define VESA_MODEDB_SIZE 34

void                            frame_buffer_var_to_videomode(struct fb_videomode *mode, const struct frame_buffer_var_screeninfo *var);
void                            fb_videomode_to_var(struct frame_buffer_var_screeninfo *var, const struct fb_videomode *mode);
int                             fb_mode_is_equal(const struct fb_videomode *mode1, const struct fb_videomode *mode2);
int                             fb_add_videomode(const struct fb_videomode *mode, double_list_t *head);
void                            fb_delete_videomode(const struct fb_videomode *mode, double_list_t *head);
const struct fb_videomode      *fb_match_mode(const struct frame_buffer_var_screeninfo *var, double_list_t *head);
const struct fb_videomode      *fb_find_best_mode(const struct frame_buffer_var_screeninfo *var, double_list_t *head);
const struct fb_videomode      *fb_find_nearest_mode(const struct fb_videomode *mode, double_list_t *head);
void                            fb_destroy_modelist(double_list_t *head);
void                            fb_videomode_to_modelist(const struct fb_videomode *modedb, int num, double_list_t *head);
const struct fb_videomode      *fb_find_best_display(const struct fb_monspecs *specs, double_list_t *head);
/*
 * fbcmap.c
 */
int                             fb_alloc_cmap(struct frame_buffer_cmap *cmap, int len, int transp);
void                            fb_dealloc_cmap(struct frame_buffer_cmap *cmap);
int                             fb_copy_cmap(const struct frame_buffer_cmap *from, struct frame_buffer_cmap *to);
int                             fb_set_cmap(struct frame_buffer_cmap *cmap, struct frame_buffer_info *info);
const struct frame_buffer_cmap *fb_default_cmap(int len);
void                            fb_invert_cmaps(void);

struct fb_videomode {
    const char *name;    /* optional */
    uint32_t    refresh; /* optional */
    uint32_t    xres;
    uint32_t    yres;
    uint32_t    pixclock;
    uint32_t    left_margin;
    uint32_t    right_margin;
    uint32_t    upper_margin;
    uint32_t    lower_margin;
    uint32_t    hsync_len;
    uint32_t    vsync_len;
    uint32_t    sync;
    uint32_t    vmode;
    uint32_t    flag;
};

extern const char               *fb_mode_option;
extern const struct fb_videomode vesa_modes[];
extern const struct fb_videomode cea_modes[64];

struct fb_modelist {
    double_list_t       list;
    struct fb_videomode mode;
};

int fb_find_mode(
    struct frame_buffer_var_screeninfo *var, struct frame_buffer_info *info, const char *mode_option, const struct fb_videomode *db, uint32_t dbsize,
    const struct fb_videomode *default_mode, uint32_t default_bpp);

#endif /* __DRV_FB_H_ */
