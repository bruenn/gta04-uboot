#include <common.h>
#include <twl4030.h>
#include <asm/io.h>
#include <asm/arch/mmc_host_def.h>
#include <asm/arch/mux.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/gpio.h>
#include <asm/mach-types.h>
#include "../../ti/beagle/beagle.h"

// make us initialize using both pinmux sets
char *muxname="BeagleBoard";
char *peripheral="";

extern int get_board_revision(void);

int misc_init_r(void)
{
	int orig_misc_init_r(void);
	char devtree[50]="unknown";
	orig_misc_init_r();		// initializes board revision dependent mux (e.g. MUX_BEAGLE_C())
#ifdef BEAGLE_EXTRA_MUX
	BEAGLE_EXTRA_MUX();	// set MUX for expander board
#endif
	switch (get_board_revision()) {
		case REVISION_AXBX:
		case REVISION_CX:
		case REVISION_C4:
			strcpy(devtree, "omap3-beagle");
			break;
		case REVISION_XM_A:
		case REVISION_XM_B:
			strcpy(devtree, "omap3-beagle-xm-ab");
			break;
		case REVISION_XM_C:
			strcpy(devtree, "omap3-beagle-xm");
			break;
	}
	
	setenv("mux", muxname);
	strcat(devtree, peripheral);
	setenv("devicetree", devtree);
	printf("Device Tree: %s\n", devtree);

	return 0;
}

int isXM(void)
{
	int rev=get_board_revision();
	return (rev == REVISION_XM_A ||rev == REVISION_XM_B || rev == REVISION_XM_C);
}

#undef misc_init_r
#define misc_init_r orig_misc_init_r

// take the original beagle.c code - but rename misc_init_r
#include "../../ti/beagle/beagle.c"