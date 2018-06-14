// all pins on BB expansion connector

// GPIO -> BB-Pin -> Expander function

#define MUX_BEAGLE_EXPANDER() \
/*MUX_VAL(CP(MMC2_CLK),		(IEN  | PTD | EN  | M1)) /*GPIO_130 / MCSPI3-CLK -> TRF*/\
MUX_VAL(CP(MMC2_CLK),		(IDIS | PTD | EN  | M4)) /*GPIO_130 / MCSPI3-CLK -> TRF*/\
/*MUX_VAL(CP(MMC2_CMD),		(IDIS | PTD | EN  | M1)) /*GPIO_131 / MCSPI3-SIMO -> TRF*/\
MUX_VAL(CP(MMC2_CMD),		(IDIS | PTD | EN  | M4)) /*GPIO_131 / MCSPI3-SIMO -> TRF*/\
/*MUX_VAL(CP(MMC2_DAT0),		(IEN  | PTU | EN  | M1)) /*GPIO_132 / MCSPI3-SOMI -> TRF*/\
MUX_VAL(CP(MMC2_DAT0),		(IEN  | PTU | EN  | M4)) /*GPIO_132 / MCSPI3-SOMI -> TRF*/\
MUX_VAL(CP(MMC2_DAT1),		(IEN  | PTU | EN  | M4)) /*GPIO_133 / UART3-RX (software)*/\
MUX_VAL(CP(MMC2_DAT2),		(IDIS | PTU | EN  | M4)) /*GPIO_134 / UART3-TX (software)*/\
MUX_VAL(CP(MMC2_DAT3),		(IDIS | PTU | EN  | M1)) /*GPIO_135 / MCSPI3-CS0*/\
MUX_VAL(CP(MMC2_DAT4),		(IEN  | PTD | EN  | M4)) /*GPIO_136 / AUX */\
MUX_VAL(CP(MMC2_DAT5),		(IEN  | PTU | EN  | M4)) /*GPIO_137 / POWER */\
MUX_VAL(CP(MMC2_DAT6),		(IDIS | PTU | EN  | M4)) /*GPIO_138 / UART3-RTS (software) */\
MUX_VAL(CP(MMC2_DAT7),		(IEN  | PTU | EN  | M4)) /*GPIO_139 / UART3-CTS (software) */\
MUX_VAL(CP(UART2_RX),		(IEN  | PTU | EN  | M4)) /*GPIO_143 / UART2-RX */\
MUX_VAL(CP(UART2_CTS),		(IDIS | PTU | EN  | M4)) /*GPIO_144 / UART2-CTS */\
MUX_VAL(CP(UART2_RTS),		(IDIS | PTU | EN  | M4)) /*GPIO_145 / UART2-RTS */\
MUX_VAL(CP(UART2_TX),		(IEN  | PTU | EN  | M4)) /*GPIO_146 / UART2-TX */\
MUX_VAL(CP(MCBSP1_CLKR),	(IEN  | PTD | EN  | M4)) /*GPIO_156 / ... - KEYIRQ -> TRF IRQ */\
MUX_VAL(CP(MCBSP1_FSR),		(IEN  | PTU | EN  | M4)) /*GPIO_157 / ... - PENIRQ */\
MUX_VAL(CP(MCBSP1_DX),		(IDIS | PTD | EN  | M4)) /*GPIO_158 / ... - Display STBY */\
MUX_VAL(CP(MCBSP1_DR),		(IDIS | PTD | EN  | M4)) /*GPIO_159 / McBSP1-DR -> TRF EN2 */\
MUX_VAL(CP(MCBSP_CLKS),		(IEN  | PTU | DIS | M0)) /*GPIO_??? / McBSP_CLKS */\
MUX_VAL(CP(MCBSP1_FSX),		(IDIS | PTD | EN  | M4)) /*GPIO_161 / McBSP1-FSX -> TRF EN */\
MUX_VAL(CP(MCBSP1_CLKX),	(IDIS | PTD | EN  | M4)) /*GPIO_162 / McBSP1-CLKX -> UART3 Powerdown */\
muxname="BeagleBoardB2", peripheral="+gta04b2"

