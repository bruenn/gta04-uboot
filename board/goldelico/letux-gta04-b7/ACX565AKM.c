/* u-boot driver for the Sharp ACX565AKM LCM
 *
 * Copyright (C) 2012 by Golden Delicious Computers GmbH&Co. KG
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
#include "ACX565AKM.h"

#define mdelay(n) ({ unsigned long msec = (n); while (msec--) udelay(1000); })

#ifdef CONFIG_TARGET_LETUX_GTA04_B7	/* Neo900 */

#define GPIO_POWER 12			/* McBSP5-CLKX enables 5V DC/DC (backlight) for the display */
#define GPIO_BLSHUTDOWN 19		/* McBSP5-FSX controls Backlight SHUTDOWN (shutdown if high) */
#define GPIO_SHUTDOWN 20		/* McBSP5-DX controls LVDS SHUTDOWN (shutdown if low) */

#elif CONFIG_TARGET_LETUX_BEAGLE_B7	/* Neo900 with BeagleBoard XM */

#define GPIO_POWER 162			/* McBSP1-CLKX enables 5V DC/DC (backlight) for the display */
#define GPIO_BLSHUTDOWN 161		/* McBSP1-FSX controls Backlight SHUTDOWN (shutdown if high) */
#define GPIO_SHUTDOWN 158		/* McBSP1-DX controls LVDS SHUTDOWN (shutdown if low) */

#else
#error only for B7 boards
#endif

// configure beagle board DSS for the ACX565AKM

#define DVI_BACKGROUND_COLOR		0x00fadc29	// rgb(250, 220, 41)

#define DSS1_FCLK	432000000	// see figure 15-65
#define DSS1_FCLK3730	108000000	// compare table 3-34, figure 7-63 - but there are other factors
#define PIXEL_CLOCK	48336000	// approx. 48.336 MHz (will be divided from 432 MHz)

// all values are min ratings

#define VDISP	600				// vertical active area
#define VFP		(621-VDISP)/3	// vertical front porch
#define VS		(621-VDISP)/3	// VSYNC pulse width
#define VBP		(621-VDISP)/3	// vertical back porch
#define VDS		(VS+VBP)		// vertical data start
#define VBL		(VS+VBP+VFP)	// vertical blanking period
#define VP		(VDISP+VBL)		// vertical cycle

#define HDISP	1024			// horizontal active area
#define HFP		(1312-HDISP)/3	// horizontal front porch
#define HS		(1312-HDISP)/3	// HSYNC pulse width
#define HBP		(1312-HDISP)/3	// horizontal back porch
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
	// positive DE pulse incl. HSYNC&VSYNC
	.pol_freq	= (1<<17)|(1<<16)|(0<<15)|(0<<14)|(0<<13)|(0<<12)|0x28,    /* Pol Freq */
	.divisor	= (0x0001<<16)|(DSS1_FCLK/PIXEL_CLOCK), /* Pixel Clock divisor from dss1_fclk */
	.lcd_size	= ((HDISP-1)<<0) | ((VDISP-1)<<16), /* as defined by LCM */
	.panel_type	= 0x01, /* TFT */
	.data_lines	= 0x03, /* 24 Bit RGB */
	.load_mode	= 0x02, /* Frame Mode */
	.panel_color	= DVI_BACKGROUND_COLOR
};

int panel_reg_init(void)
{
	gpio_request(GPIO_SHUTDOWN, "shutdown");
	gpio_direction_output(GPIO_SHUTDOWN, 0);		// output
	gpio_request(GPIO_POWER, "power");
	gpio_direction_output(GPIO_POWER, 0);		// output
	gpio_request(GPIO_BLSHUTDOWN, "backlight");
	gpio_direction_output(GPIO_BLSHUTDOWN, 0);	// output
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
int panel_enter_state(enum panel_state new_state)
{
	return 0;
}

int panel_display_onoff(int on)
{
	if(on)
		{
		gpio_direction_output(GPIO_POWER, 1);
		mdelay(10);
		gpio_direction_output(GPIO_SHUTDOWN, 1);
		mdelay(100);
		gpio_direction_output(GPIO_BLSHUTDOWN, 0);
		}
	else
		{
		gpio_direction_output(GPIO_BLSHUTDOWN, 1);
		mdelay(5);
		gpio_direction_output(GPIO_SHUTDOWN, 0);
		mdelay(10);
		gpio_direction_output(GPIO_POWER, 0);
		mdelay(200);
		}
	return 0;
}

int board_video_init(GraphicDevice *pGD)
{
	extern int isXM(void);
	printf("board_video_init() ACX565AKM\n");
	
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
	
	printf("did board_video_init()\n");
	return 0;
}

