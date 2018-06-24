/* u-boot driver for the OrtusTech COM37H3M05DTC LCM
 *
 * Copyright (C) 2011-12 by Golden Delicious Computers GmbH&Co. KG
 * Author: H. Nikolaus Schaller <hns@goldelico.com>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/mux.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/gpio.h>
#include <asm/gpio.h>
#include <asm/mach-types.h>
#include <twl4030.h>
#include "../letux-gta04/dssfb.h"
#include "../letux-gta04/panel.h"
#include "../letux-gta04/backlight.h"
#include "COM37H3M05DTC.h"

// FIXME: should go to board config

#ifdef CONFIG_TARGET_LETUX_GTA04_B2

#define GPIO_STBY 20

#elif CONFIG_TARGET_LETUX_BEAGLE_B2

#define GPIO_STBY 158

#else

#error only for B2 Expander boards

#endif

// configure beagle board DSS for the COM37H3M05DTC

#define DVI_BACKGROUND_COLOR		0x00fadc29	// rgb(250, 220, 41)

#define DSS1_FCLK	432000000	// see figure 15-65
#define DSS1_FCLK3730	108000000	// compare table 3-34, figure 7-63 - but there are other factors
#define PIXEL_CLOCK	22400000	// approx. 22.4 MHz (will be divided from 432 MHz)

// all values are min ratings for COM37H3M05DTC and COM37H3M99DTC (more critical)

#define VDISP	640				// vertical active area
#define VFP		4				// vertical front porch
#define VS		3				// VSYNC pulse width (negative going)
#define VBP		3				// vertical back porch
#define VDS		(VS+VBP)		// vertical data start
#define VBL		(VS+VBP+VFP)	// vertical blanking period
#define VP		(VDISP+VBL)		// vertical cycle

#define HDISP	480				// horizontal active area
#define HFP		8				// horizontal front porch
#define HS		10				// HSYNC pulse width (negative going)
#define HBP		10				// horizontal back porch
#define HDS		(HS+HBP)		// horizontal data start
#define HBL		(HS+HBP+HFP)	// horizontal blanking period
#define HP		(HDISP+HBL)		// horizontal cycle

int displayColumns=HDISP;
int displayLines=VDISP;

static /*const*/ struct panel_config lcm_cfg = 
{
	.timing_h	= ((HBP-1)<<20) | ((HFP-1)<<8) | ((HS-1)<<0), /* Horizantal timing */
	.timing_v	= ((VBP+0)<<20) | ((VFP+0)<<8) | ((VS-1)<<0), /* Vertical timing */
	// negative clock edge
	// negative sync pulse
	// positive DE pulse
	.pol_freq	= (1<<17)|(1<<16)|(0<<15)|(0<<14)|(1<<13)|(1<<12)|0x28,    /* Pol Freq */
	.divisor	= (0x0001<<16)|(DSS1_FCLK/PIXEL_CLOCK), /* Pixel Clock divisor from dss1_fclk */
	.lcd_size	= ((HDISP-1)<<0) | ((VDISP-1)<<16), /* as defined by LCM */
	.panel_type	= 0x01, /* TFT */
	.data_lines	= 0x03, /* 24 Bit RGB */
	.load_mode	= 0x02, /* Frame Mode */
	.panel_color	= DVI_BACKGROUND_COLOR
};

int panel_reg_init(void)
{
	gpio_request(GPIO_STBY, "panel-stby");
	gpio_direction_output(GPIO_STBY, 0);		// output
	return 0;
}

int panel_check(void)
{
	return 0;
}

const char *panel_state(void)
{
	return "?";
}

/* frontend function */

static char initialized = false;

int panel_enter_state(enum panel_state new_state)
{
	return 0;
}

int panel_display_onoff(int on)
{
	if (!initialized)
		return -EINVAL;

	gpio_direction_output(GPIO_STBY, on?1:0);	// on = no STBY
	return 0;
}

int board_video_init(GraphicDevice *pGD)
{
	extern int isXM(void);
	printf("board_video_init() COM37H3M05DTC\n");

	// FIXME: here we should pass down the GPIO(s)

	backlight_init();	// initialize backlight
	if(isXM()) {
		/* Set VAUX1 to 3.3V for GTA04E display board */
		twl4030_pmrecv_vsel_cfg(TWL4030_PM_RECEIVER_VAUX1_DEDICATED,
								/*TWL4030_PM_RECEIVER_VAUX1_VSEL_33*/ 0x07,
								TWL4030_PM_RECEIVER_VAUX1_DEV_GRP,
								TWL4030_PM_RECEIVER_DEV_GRP_P1);
		udelay(5000);
	}

	// FIXME: here we should init the TSC and pass down the GPIO numbers and resistance values

	if(panel_reg_init())	// initialize SPI
		{
		printf("No LCM connected\n");
		return 1;
		}

	if (get_cpu_family() == CPU_OMAP36XX)
		lcm_cfg.divisor	= (0x0001<<16)|(DSS1_FCLK3730/PIXEL_CLOCK); /* get Pixel Clock divisor from dss1_fclk */
	dssfb_init(&lcm_cfg);
	
	initialized = true;

	printf("did board_video_init()\n");
	return 0;
}

