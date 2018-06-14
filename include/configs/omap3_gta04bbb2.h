/*
 * (C) Copyright 2010
 * Nikolaus Schaller <hns@goldelico.com>
 *
 * Configuration settings for the TI OMAP3530 Beagle board with
 *               Openmoko Hybrid Display extension.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#define CONFIG_GOLDELICO_EXPANDER_B2	1	/* working with BEAGLE and B2 Expander board */

#include "omap3_beagle.h"	/* share config */

#define CONFIG_CMD_UNZIP	1	/* for reducing size of splash image */
// #undef CONFIG_CMD_JFFS2
// #define CONFIG_CMD_JFFS2	1	/* to access the rootfs in NAND flash */

#if 0	// does not compile
#define CONFIG_MUSB_HCD        1 /* Enable USB driver*/
#define CONFIG_TWL4030_USB      1 /* Enable TWL4030 USB */
#define CONFIG_USB_STORAGE
#define CONFIG_USB_OMAP3530
#define CONFIG_USB_HOST
#define CONFIG_CMD_USB
#endif

#define CONFIG_CMD_SPI	1

// FIXME: add configs for the partitions so that JFFS2 runs in the correct NAND partition

#undef CONFIG_SYS_PROMPT
#define CONFIG_SYS_PROMPT		"OGTA04@Beagle B2 # "


/* __CONFIG_H */
