/*
 * (C) Copyright 2011
 * H. Nikolaus Schaller, Golden Delicious Computers, hns@goldelico.com
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <command.h>
#include <spi.h>
#include <asm/arch/gpio.h>
#include <malloc.h>

/* board specific configuration */

struct trf7960 {
	/*
	 * OMAP3 McSPI (MultiChannel SPI) has 4 busses (modules)
	 * with different number of chip selects (CS, channels):
	 * McSPI1 has 4 CS (bus 0, cs 0 - 3)
	 * McSPI2 has 2 CS (bus 1, cs 0 - 1)
	 * McSPI3 has 2 CS (bus 2, cs 0 - 1)
	 * McSPI4 has 1 CS (bus 3, cs 0)
	 */
	int bus;
	int cs;
	int clock;
	int irq;	/* GPIO that receives IRQ; use -1 if we have no IRQ */
	int en;		/* GPIO that controls EN; use -1 if hardwired */
	int en2;	/* GPIO that controls EN2; use -1 if hardwired (use same if both are parallel) */
	int vio;	/* specify 10*VIO i.e. 18 for 1.8V, 33 for 3.3V */
	struct spi_slave *slave;
	/* internal */
	int done;	/* done interrupt flag */
	uchar *datapointer;	/* RX/TX data pointer */
	int bytes;		/* number of remaining bytes to transmit (may be negative) */
};

/* low level functions */

#define TRF7960_REG_CSC		0x00	/* Chip Status control (R/W) */
#define TRF7960_REG_ISOC	0x01	/* ISO control (R/W) */
#define TRF7960_REG_ISO14443BTXOPT	0x02	/* TX Options (R/W) */
#define TRF7960_REG_ISO14443AHBROPT	0x03	/* High Bitrate Options (R/W) */
#define TRF7960_REG_TXTIMERMSB	0x04	/* TX Timer (R/W) */
#define TRF7960_REG_TXTIMERLSB	0x05	/* TX Timer (R/W) */
#define TRF7960_REG_TXPULSEC	0x06	/* TX Pulse Lenght control (R/W) */
#define TRF7960_REG_RXNRWAIT	0x07	/* RX no response wait (R/W) */
#define TRF7960_REG_RXWAIT	0x08	/* RX wait time (after TX) (R/W) */
#define TRF7960_REG_MODCLK	0x09	/* Modulator and SYS_CLK (R/W) */
#define TRF7960_REG_RXSPECIAL	0x0a	/* RX special setting (R/W) */
#define TRF7960_REG_REGIO	0x0b	/* Regulator and IO control (R/W) */
#define TRF7960_REG_IRQ	0x0c	/* IRQ status (R) */
#define TRF7960_REG_IRQMASK	0x0d	/* Collision position (MSB) and Interrupt Mask (R/W) */
#define TRF7960_REG_COLLISION	0x0e	/* Collision position (LSB) (R) */
#define TRF7960_REG_RSSI	0x0f	/* RSSI levels and oscillator status (R) */
#define TRF7960_REG_FIFO_STATUS	0x1c	/* FIFO status (R) */
#define TRF7960_REG_FIFO_TXLEN1	0x1d	/* TX length byte 1 (R/W) */
#define TRF7960_REG_FIFO_TXLEN2	0x1e	/* TX length byte 2 (R/W) */
#define TRF7960_REG_FIFO_DATA	0x1f	/* FIFO I/O register (R/W) */

/* FIXME: add bit masks for these registers */

#define TRF7960_CMD_IDLE	0x00
#define TRF7960_CMD_INIT	0x03
#define TRF7960_CMD_RESET	0x0f		/* reset FIFO */
#define TRF7960_CMD_TX_NOCRC	0x10
#define TRF7960_CMD_TX_CRC	0x11	/* TX with CRC */
#define TRF7960_CMD_TX_DELAYED_NOCRC	0x12
#define TRF7960_CMD_TX_DELAYED_CRC	0x13
#define TRF7960_CMD_TX_NEXT_SLOT	0x14	/* send EOF / next slot */
#define TRF7960_CMD_RX_BLOCK	0x16	/* block receiver */
#define TRF7960_CMD_RX_ENABLE	0x17	/* enable receiver */
#define TRF7960_CMD_TEST_RF_INT	0x18
#define TRF7960_CMD_TEST_RF_EXT	0x19
#define TRF7960_CMD_RX_GAIN_ADJUST	0x1a

/* first byte sent through SPI */
#define TRF7960_COMMAND		0x80
#define TRF7960_ADDRESS		0x00	/* combine with READ/WRITE and optionally CONTINUE */
#define TRF7960_READ		0x40
#define TRF7960_WRITE		0x00
#define TRF7960_CONTINUE	0x20

/* power modes (increasing power demand) */
#define TRF7960_POWER_DOWN	0
#define TRF7960_POWER_60kHz	1		/* VDD_X available, 60 kHz */
#define TRF7960_POWER_STANDBY	2	/* 13.56 MHz osc. on, SYS_CLK available; regulators in low power */
#define TRF7960_POWER_ACTIVE	3	/* 13.56 MHz osc. on, SYS_CLK available; regulators active */
#define TRF7960_POWER_RX	4	/* RX active */
#define TRF7960_POWER_RXTX_HALF	5	/* RX+TX active; half power mode */
#define TRF7960_POWER_RXTX_FULL	6	/* RX+TX active; full power mode */

/* protocols */
#define TRF7960_PROTOCOL_ISO15693_LBR_1SC_4		0x00
#define TRF7960_PROTOCOL_ISO15693_LBR_1SC_256	0x01
#define TRF7960_PROTOCOL_ISO15693_HBR_1SC_4		0x02
#define TRF7960_PROTOCOL_ISO15693_HBR_1SC_256	0x03
#define TRF7960_PROTOCOL_ISO15693_LBR_2SC_4		0x04
#define TRF7960_PROTOCOL_ISO15693_LBR_2SC_256	0x05
#define TRF7960_PROTOCOL_ISO15693_HBR_2SC_4		0x06
#define TRF7960_PROTOCOL_ISO15693_HBR_2SC_256	0x07
#define TRF7960_PROTOCOL_ISO14443A_BR_106	0x08
#define TRF7960_PROTOCOL_ISO14443A_BR_212	0x09
#define TRF7960_PROTOCOL_ISO14443A_BR_424	0x0a
#define TRF7960_PROTOCOL_ISO14443A_BR_848	0x0b
#define TRF7960_PROTOCOL_ISO14443B_BR_106	0x0c
#define TRF7960_PROTOCOL_ISO14443B_BR_212	0x0d
#define TRF7960_PROTOCOL_ISO14443B_BR_424	0x0e
#define TRF7960_PROTOCOL_ISO14443B_BR_848	0x0f
#define TRF7960_PROTOCOL_TAGIT	0x13

/* */

#if 1	/* fix HW problem with TRF7960 being in "SPI without SS" mode */

#define spi_xfer bitbang_spi_xfer
#define spi_claim_bus(X)
#define spi_release_bus(X)

#define McSPI3_CLK	130
#define McSPI3_SIMO	131
#define McSPI3_SOMI	132

#define HALFBIT 1	/* 1us gives approx. 500kHz clock */

static int bitbang_spi_xfer(struct spi_slave *slave, int bitlen, uchar writeBuffer[], uchar readBuffer[], int flags)
{ /* generates bitlen+2 clock pulses */
	static int first=1;
	int bit;
	uchar wb=0;
	uchar rb=0;
	if(first) { /* if not correctly done by pinmux */
		omap_set_gpio_direction(McSPI3_CLK, 0);
		omap_set_gpio_direction(McSPI3_SIMO, 0);
		omap_set_gpio_direction(McSPI3_SOMI, 1);
		omap_set_gpio_dataout(McSPI3_CLK, 0);
		omap_set_gpio_dataout(McSPI3_SIMO, 0);	/* send out constant 0 bits (idle command) */
		first=0;
		udelay(100);
	}
#if 0
	printf("bitbang_spi_xfer %d bits\n", bitlen);
#endif
	if(flags & SPI_XFER_BEGIN) {
		omap_set_gpio_dataout(McSPI3_CLK, 1);
		udelay(HALFBIT);	/* may be optional (>50ns) */
		omap_set_gpio_dataout(McSPI3_SIMO, 1);	/* start condition (data transision while clock=1) */
		udelay(HALFBIT);		
	}
	omap_set_gpio_dataout(McSPI3_CLK, 0);
	for(bit=0; bit < bitlen; bit++)
		{ /* write data */
			if(bit%8 == 0)
				wb=writeBuffer[bit/8];
			omap_set_gpio_dataout(McSPI3_SIMO, (wb&0x80)?1:0);	/* send MSB first */
			wb <<= 1;
			udelay(HALFBIT);
			omap_set_gpio_dataout(McSPI3_CLK, 1);
			udelay(HALFBIT);
			omap_set_gpio_dataout(McSPI3_CLK, 0);
			rb = (rb<<1) | omap_get_gpio_datain(McSPI3_SOMI);	/* sample on falling edge and receive MSB first */
			if(bit%8 == 7)
				readBuffer[bit/8]=rb;
		}
	if(flags & SPI_XFER_END) {
		omap_set_gpio_dataout(McSPI3_SIMO, 1);	/* set data to 1 */
		udelay(HALFBIT);
		omap_set_gpio_dataout(McSPI3_CLK, 1);
		udelay(HALFBIT);
		omap_set_gpio_dataout(McSPI3_SIMO, 0);	/* stop condition (data transision while clock=1) */
		udelay(HALFBIT);	/* may be optional (>50ns) */
		omap_set_gpio_dataout(McSPI3_CLK, 0);
		udelay(HALFBIT);
	}
	return 0;
}

#endif
					 
static inline int readRegister(struct trf7960 *device, uchar addr)
{
	uchar writeBuffer[2];
	uchar readBuffer[2];
	writeBuffer[0] = TRF7960_ADDRESS | TRF7960_READ | addr;
	spi_claim_bus(device->slave);
	if(spi_xfer(device->slave, 16, writeBuffer, readBuffer, SPI_XFER_BEGIN | SPI_XFER_END) != 0)
		return -1;
	spi_release_bus(device->slave);
	return readBuffer[1];
}

static inline int writeRegister(struct trf7960 *device, uchar addr, uchar byte)
{
	uchar writeBuffer[2];
	uchar readBuffer[2];
	writeBuffer[0] = TRF7960_ADDRESS | TRF7960_WRITE | addr;
	writeBuffer[1] = byte;
	spi_claim_bus(device->slave);
	if(spi_xfer(device->slave, 2*8, writeBuffer, readBuffer, SPI_XFER_BEGIN | SPI_XFER_END) != 0)
		return -1;
	spi_release_bus(device->slave);
	return 0;
}

static inline int sendCommand(struct trf7960 *device, uchar cmd)
{
	uchar writeBuffer[1];
	uchar readBuffer[1];
	writeBuffer[0] = TRF7960_COMMAND | cmd;
	spi_claim_bus(device->slave);
	if(spi_xfer(device->slave, 8, writeBuffer, readBuffer, SPI_XFER_BEGIN | SPI_XFER_END) != 0)
		return -1;
	spi_release_bus(device->slave);
	return 0;
}

/* mid level  (should be partially mapped to sysfs) */

int resetIRQ(struct trf7960 *device)
{
	uchar writeBuffer[3];
	uchar readBuffer[3];
	writeBuffer[0] = TRF7960_ADDRESS | TRF7960_READ | TRF7960_CONTINUE | TRF7960_REG_IRQ;
	writeBuffer[1] = 0;	// dummy read
	writeBuffer[2] = 0;	// dummy read
	spi_claim_bus(device->slave);
	if(spi_xfer(device->slave, 3*8, writeBuffer, readBuffer, SPI_XFER_BEGIN | SPI_XFER_END) != 0)
		return -1;
	spi_release_bus(device->slave);
#if 0
	{
	int i;
	for(i=0; i<3; i++)
		printf("rb[i]=%02x\n", readBuffer[i]);
	}
#endif
	return readBuffer[1];
}

int getCollisionPosition(struct trf7960 *device)
{
	uchar writeBuffer[3];
	uchar readBuffer[3];
	writeBuffer[0] = TRF7960_ADDRESS | TRF7960_READ | TRF7960_CONTINUE | TRF7960_REG_IRQMASK;
	writeBuffer[1] = 0;	// dummy read
	writeBuffer[2] = 0;	// dummy read
	spi_claim_bus(device->slave);
	if(spi_xfer(device->slave, 3*8, writeBuffer, readBuffer, SPI_XFER_BEGIN | SPI_XFER_END) != 0)
		return -1;
	spi_release_bus(device->slave);
	return ((readBuffer[1]&0xc0)<<2) | readBuffer[2];	// combine into 10 bits
}

/* how to handle spi_claim_bus during irq handler? */

int prepareIrq(struct trf7960 *device, uchar *data, unsigned int bytes)
{
#if 1
	printf("prepareIrq data=%p bytes=%u\n", data, bytes);
#endif
#if 0
	{
	//	uchar irq=readRegister(device, TRF7960_REG_IRQ);
	uchar irq=resetIRQ(device);
	printf("    irq-status=%02x\n", irq);		
	}
#endif
#if 0
	while(device->irq >= 0 && omap_get_gpio_datain(device->irq)) {
		printf("prepareIrq: IRQ pin already active!\n");
		resetIRQ(device);
	}
#endif
	device->done=0;	/* not yet done */
	device->datapointer=data;
	device->bytes=bytes;
	/* enable IRQ */
	return 0;
}

void handleInterrupt(struct trf7960 *device)
{ /* process interrupt */
	unsigned char buffer[12];
//	uchar irq=readRegister(device, TRF7960_REG_IRQ);
	uchar irq=resetIRQ(device);
	if(!irq)
		return;	/* false alarm or waitIrq */
	if(device->done)
		return;	/* unprocessed previous interrupt */
#if 0
	printf("handleirq %02x\n", irq);
#endif
	device->done=irq;	/* set done flag with interrupt flags */
#if 0	// read again test (did a read reset the IRQ flags?)
	irq=readRegister(device, TRF7960_REG_IRQ);
	printf("handleirq %02x %02x\n", device->done, irq);
	irq=device->done;	/* restore */
#endif
#if 0
	udelay(5);
	if(device->irq >= 0 && omap_get_gpio_datain(device->irq))
		printf("  IRQ pin still/again active!\n");
#endif
	if(irq & 0x80) { /* end of TX */
#if 1
		printf("handleirq end of TX %02x\n", irq);
#endif
		sendCommand(device, TRF7960_CMD_RESET);	/* reset FIFO */
		return;
		}
	if(irq & 0x02) { /* collision occurred */
		int position;
#if 1
		printf("handleirq collision %02x\n", irq);
#endif
		// FIXME: combine into single message
		sendCommand(device, TRF7960_CMD_RX_BLOCK);	/* block RX */
		position=getCollisionPosition(device);
#if 1
		printf("position=%d\n", position);
#endif
		// number of valid bytes is collpos - 32
		// read bytes
		// handle broken byte
		sendCommand(device, TRF7960_CMD_RESET);	/* reset FIFO */
	}
	else if(irq & 0x40) { /* end of RX */
		uchar fifosr=readRegister(device, TRF7960_REG_FIFO_STATUS);
		uchar unread=(fifosr&0xf) + 1;
#if 1
		printf("handleirq end of RX %02x fifosr=%02x unread=%d\n", irq, fifosr, unread);
#endif
		int n = device->bytes <= unread ? device->bytes : unread;	/* limit to remaining bytes in FIFO or buffer */
		buffer[0] = TRF7960_READ | TRF7960_CONTINUE | TRF7960_REG_FIFO_DATA;	/* continuous read from FIFO */
		memset(&buffer[1], 0, sizeof(buffer)/sizeof(buffer[0])-1);	/* clear buffer so that we don't write garbage */
		if(spi_xfer(device->slave, 8*sizeof(buffer[0])*(2 + n), buffer, buffer, SPI_XFER_BEGIN | SPI_XFER_END) != 0)	/* we need 8 more clocks to receive the last byte */
			return;	/* spi error */
		memcpy(device->datapointer, &buffer[1], n);	/* buffer[0] are the first 8 bits shifted out while we did send the command */
		device->datapointer += n;
		device->bytes -= n;
#if 1
		if(device->bytes > 0)
			printf("  remaining buffer=%d\n", device->bytes);
#endif
		// handle broken byte
		sendCommand(device, TRF7960_CMD_RESET);	/* reset FIFO */		
	}
	else if(irq & 0x20) { /* FIFO interrupt */
		int n = device->bytes <= 9 ? device->bytes : 9;	/* limit to 9 bytes */
#if 1
		printf("handleirq fifo request %02x (%d bytes remaining)\n", irq, device->bytes);
#endif
		if(n > 0) {
			if(irq & 0x80) { /* write next n bytes to FIFO (up to 9 or as defined by bytes) */
#if 1
				printf("write more (%d)\n", n);
#endif
				buffer[0] = TRF7960_WRITE | TRF7960_CONTINUE | TRF7960_REG_FIFO_DATA;	/* continuous write to FIFO */
				memcpy(&buffer[1], device->datapointer, n);
				if(spi_xfer(device->slave, 8*sizeof(buffer[0])*(1 + n), buffer, buffer, SPI_XFER_BEGIN | SPI_XFER_END) != 0)
					return;	/* spi error */
				device->datapointer += n;
				device->bytes -= n;				
			} else { /* read next 9 bytes from FIFO in one sequence */
#if 1
				printf("read more (%d)\n", n);
#endif
				buffer[0] = TRF7960_READ | TRF7960_CONTINUE | TRF7960_REG_FIFO_DATA;	/* continuous read from FIFO */
				memset(&buffer[1], 0, sizeof(buffer)/sizeof(buffer[0])-1);	/* clear buffer so that we don't write garbage */
				if(spi_xfer(device->slave, 8*sizeof(buffer[0])*(2 + n), buffer, buffer, SPI_XFER_BEGIN | SPI_XFER_END) != 0)	/* we need 8 more clocks to receive the last byte */
					return;	/* spi error */
				memcpy(device->datapointer, &buffer[1], n);	/* buffer[0] are the first 8 bits shifted out while we did send the command */
				device->datapointer += n;
				device->bytes -= n;
			}
			device->done=0;	/* don't notify successful data trandfer as 'done' status */
		}
		// else some error (FIFO interrupt but no data available or buffer is full
	}
}

void waitIrq(struct trf7960 *device)
{ /* wait for IRQ */
#if 0
	printf("waitIrq %d\n", device->irq);
#endif
	if(device->irq >= 0)	{ /* check IRQ pin */
		int cnt=2000;	// software timeout
		while(!omap_get_gpio_datain(device->irq) && cnt-- > 0)
			udelay(500); /* wait for IRQ pin */
		handleInterrupt(device);
	}
	else { /* poll interrupt register to check for interrupts */
		while(!device->done)
			handleInterrupt(device);
	}
#if 0
	printf("waitIrq -> %02x\n", device->done);
#endif
}

int setPowerMode(struct trf7960 *device, int mode)
{ /* control power modes */
	int status = 0;
#if 1
	printf("setPowerMode %d\n", mode);
#endif
	if(mode < TRF7960_POWER_DOWN || mode > TRF7960_POWER_RXTX_FULL)
		return -1;
	if(device->en >= 0)
		omap_set_gpio_direction(device->en, 0);		/* make output */
	if(device->en2 >= 0)
		omap_set_gpio_direction(device->en2, 0);	/* make output */
	if(device->irq >= 0)
		omap_set_gpio_direction(device->irq, 1);	/* make input */
	if(mode == TRF7960_POWER_DOWN) {
		if(device->slave) {
			spi_free_slave(device->slave);
			device->slave = NULL;
		}
		if(device->en >= 0)
			omap_set_gpio_dataout(device->en, 0);
		if(device->en2 >= 0 && device->en != device->en2)
			omap_set_gpio_dataout(device->en2, 0);	/* not tied togehter */
	}
	else if(mode == TRF7960_POWER_60kHz) {
		if(device->en >= 0 && device->en2 >= 0 && device->en == device->en2)
			return -1;	/* can't control them separately */
		if(device->en >= 0)
			omap_set_gpio_dataout(device->en, 0);
		if(device->en2 >= 0)
			omap_set_gpio_dataout(device->en2, 1);
	}
	else {
		if(device->en >= 0)
			omap_set_gpio_dataout(device->en, 1);
		if(!device->slave) {
			device->slave = spi_setup_slave(device->bus, device->cs, device->clock, SPI_MODE_0);
			if(!device->slave)
				return -1;	// failed			
		}
		udelay(1000);	/* wait until we can read/write */
		status = readRegister(device, TRF7960_REG_CSC);
#if 1
		printf("CSC = %02x\n", status);
#endif
		if(status < 0)
			return status;	/* some error */
		switch(mode) {
			case TRF7960_POWER_STANDBY:
				status |= 0x80;
				break;
			case TRF7960_POWER_ACTIVE:
				status &= 0x5d;
				break;
			case TRF7960_POWER_RX:
				status &= 0x5d;
				status |= 0x02;
				break;
			case TRF7960_POWER_RXTX_HALF:
				status &= 0x4f;
				status |= 0x20;
				break;
			case TRF7960_POWER_RXTX_FULL:
				status &= 0x4f;
				status |= 0x30;
				break;
			default:
				return -1;
		}
#if 1
		printf("   => %02x\n", status);
#endif
		status = writeRegister(device, TRF7960_REG_CSC, status);
		// init other registers (only if previous mode was 0 or 1)
		if(device->vio < 27)
			; /* set bit 5 in Reg #0x0b to decrease output resistance for low voltage I/O */
		udelay(5000);	/* wait until reader has recovered (should depend on previous and current mode) */
	}
	return status;
}

int chooseProtocol(struct trf7960 *device, int protocol)
{
#if 1
	printf("chooseProtocol %d\n", protocol);
#endif
	if((protocol < 0 || protocol >15) && protocol != 0x13)
		return -1;
	protocol |= readRegister(device, TRF7960_REG_CSC) & 0x80;	/* keep no RX CRC mode */
	return writeRegister(device, TRF7960_REG_ISOC, protocol);
}

/* high level functions (protocol handlers) */
/* the meaning of the flags and command codes in ISO15693 mode are described in TI document sloa141.pdf */

int scanInventory(struct trf7960 *device, uchar flags, uchar length, void (*found)(struct trf7960 *device, uint64_t uid, int rssi))
{ /* poll for tag uids and resolve collisions */
	static uchar buffer[32];	/* shared rx/tx buffer */
	uchar collisionslots[16];	/* up to 16 collision slots */
	int collisions = 0;
	uchar mask[8];	/* up to 8 mask bytes */
	int slot;
	int slots = (flags & (1<<5)) ? 1 : 16;	/* multislot flag */
	
	int masksize = (length + 7) / 8;	/* add one mask byte for each started 8 bits */
	int pdusize = 3 + masksize;			/* flags byte + command byte + mask length byte + mask bytes */
	
	int protocol = readRegister(device, TRF7960_REG_ISOC) & 0x1f;

	if((protocol & 0x18) == TRF7960_PROTOCOL_ISO15693_LBR_1SC_4)
		;	/* ISO15693 */
	// FIXME: implement different algorithms for other protocols
	if((protocol & 0x1c) == TRF7960_PROTOCOL_ISO14443A_BR_106)
		return -1;	/* ISO14443A */
	if((protocol & 0x1c) == TRF7960_PROTOCOL_ISO14443B_BR_106)
		return -1;	/* ISO14443B */
	if(protocol == TRF7960_PROTOCOL_TAGIT)
		return -1;	/* Tag-It */
	if(protocol >= 0x10)
		return -1;	/* undefined */
#if 1
	printf("inventoryRequest\n");
#endif
	spi_claim_bus(device->slave);
	
	if(writeRegister(device, TRF7960_REG_IRQMASK, 0x3f))	/* enable no-response interrupt */
		return -1;

	// write modulator control
	/* writeRegister(device, TRF... 0x09, something); */
	// set rxnoresponse timeout to 0x2f if bit1 is 0 (low data rate), 0x13 else (high data rate)
	
	buffer[0] = TRF7960_COMMAND | TRF7960_CMD_RESET;	/* reset FIFO */
	buffer[1] = TRF7960_COMMAND | TRF7960_CMD_TX_CRC;	/* start TX with CRC */
	buffer[2] = 0x3d;	/* continuous write to register 0x1d */
	
	buffer[5] = flags;	/* ISO15693 flags */
	buffer[6] = 0x01;	/* ISO15693 inventory command */
	if(flags & (1<<4)) { /* AFI */
		buffer[7] = 0;	/* insert AFI value */
		buffer[8] = length;	/* mask length in bits */
		memcpy(&buffer[9], mask, masksize);	/* append mask */
		pdusize++;
	}
	else {
		buffer[7] = length;	/* mask length in bits */
		memcpy(&buffer[8], mask, masksize);	/* append mask */		
	}
	buffer[3] = pdusize >> 8;
	buffer[4] = pdusize << 4;
	prepareIrq(device, &buffer[5+12], pdusize-12);
	if(pdusize > 12)
		pdusize=12; /* limit initial transmission - remainder is sent by interrupt handler */
#if 0
	printf("length = %d\n", length);
	printf("masksize = %d\n", masksize);
	printf("pdusize = %d\n", pdusize);
	printf("bitsize = %d\n", 8*sizeof(buffer[0])*(5 + pdusize));
#endif
	if(spi_xfer(device->slave, 8*sizeof(buffer[0])*(5 + pdusize), buffer, buffer, SPI_XFER_BEGIN | SPI_XFER_END) != 0)
		return -1;
#if 0
	printf("cmd sent\n");
#endif
	waitIrq(device);	/* wait for TX interrupt */
#if 0
	printf("tx done %02x\n", device->done);
#endif
	if(!(device->done & 0x80)) {
#if 1
		printf(" unknown TX interrupt %02x\n", device->done);
#endif
		return -1;
	}
	collisions=0;
	for(slot=0; slot < slots; slot++) { /* repeat for all time slots */
		int rssi;
#if 0
		printf("slot %d\n", slot);
#endif
		prepareIrq(device, buffer, 10);	/* prepare for RX of tag id (2 bytes status + 8 bytes UID) */
		waitIrq(device);	/* wait for RX interrupt */
#if 1
		printf("rx[%d] done %02x", slot, device->done);
#endif
		rssi = readRegister(device, TRF7960_REG_RSSI) & 0x3f;
#if 0
		printf(" rssi=%02o", rssi);	/* print 2 octal digits for AUX and Active channel */
#endif
		if(device->done & 0x02) { /* collision */
#if 1
			printf(" collision\n");
#endif
			collisionslots[collisions++]=slot;	/* remember slot number */
		}
		else if(device->done & 0x01) {
			/* ignore no response timeout */
#if 1
			printf(" no response\n");
#endif
		}
		/* check for other errors */
		else if(device->done & 0x40) { /* RX done - FIFO already reset */
			union {
				uint64_t uid;
				char b[sizeof(uint64_t)];
			} uid;
#if 0
			printf(" valid id received\n");
#endif
			// buffer[0] and buffer[1] appear to be status flags and 00 if ok
			memcpy(uid.b, &buffer[2], sizeof(uid.uid));	/* extract UID */
			(*found)(device, le64_to_cpu(uid.uid), rssi);	/* notify caller */
		}
		else {
#if 1
			printf(" unknown condition %02x\n", device->done);
#endif
			break;	/* unknown interrupt reason */
		}
		if(!(device->done & 0x40))
			sendCommand(device, TRF7960_CMD_RESET);	/* reset FIFO */
		if(slots == 16) { /* send EOF only in ISO15693 multislot Inventory command */
			sendCommand(device, TRF7960_CMD_RX_BLOCK);
			sendCommand(device, TRF7960_CMD_RX_ENABLE);
			sendCommand(device, TRF7960_CMD_TX_NEXT_SLOT);
		}
	}
	if(slots == 16 ) {
		int i;
#if 1
		printf("did have %d collisions\n", collisions);
#endif
		for(i=0; i<collisions; i++) { /* loop over all slots with collision */
			// generate new mask (increased length by 4) from collision slot numbers
			// inventoryRequest(new mask, length+4)
		}
	}
	// FIXME: how to handle collision in single slot mode?
	// disable irq
	spi_release_bus(device->slave);
	return 0;
}

int readBlocks(struct trf7960 *device, uchar flags, uint64_t uid, uchar firstBlock, uchar blocks, uchar *data)
{ /* read single/multiple blocks */
	static uchar buffer[32];	/* shared rx/tx buffer */
	uchar *rxbuf;
	int pdusize = 4 + (uid?sizeof(uid):0);			/* flags byte + command byte + optional uid + firstblock + #blocks */
#if 1
	printf("readBlocks\n");
#endif
	if(blocks == 0)
		return 0;	/* no blocks */
	spi_claim_bus(device->slave);
	
	if(writeRegister(device, TRF7960_REG_IRQMASK, 0x3f))	/* enable no-response interrupt */
		return -1;
	
	rxbuf=(uchar *) malloc(2+32*blocks);	// allocate enough memory for storing 32*blocks bytes
	if(!rxbuf)
		return -1;	// can't allocate

	buffer[0] = TRF7960_COMMAND | TRF7960_CMD_RESET;	/* reset FIFO */
	buffer[1] = TRF7960_COMMAND | TRF7960_CMD_TX_CRC;	/* start TX with CRC */
	buffer[2] = 0x3d;	/* continuous write to register 0x1d */
	
	buffer[5] = flags;	/* ISO15693 flags */
	buffer[6] = blocks > 1 ? 0x23 : 0x20;	/* ISO15693 read single or multiple blocks command */
	if(uid) { /* include uid if not 0LL */
		union {
			uint64_t uid;
			char b[sizeof(uint64_t)];
		} uid2;
		uid2.uid=cpu_to_le64(uid);
		memcpy(&buffer[7], uid2.b, sizeof(uid2.b));
		pdusize=2+8+1;
		buffer[15]=firstBlock;
		if(blocks > 1) {
			buffer[16]=blocks-1;
			pdusize++;
		}
	} else { /* no UID */
		pdusize=2+1;
		buffer[7]=firstBlock;
		if(blocks > 1) {
			buffer[8]=blocks-1;
			pdusize++;
		}
	}
	buffer[3] = pdusize >> 8;
	buffer[4] = pdusize << 4;
	prepareIrq(device, &buffer[5+12], pdusize-12);
	if(pdusize > 12)
		pdusize=12; /* limit initial transmission - remainder is sent by interrupt handler */
#if 1
	printf("firstBlock = %d\n", firstBlock);
	printf("blocks = %d\n", blocks);
	printf("pdusize = %d\n", pdusize);
	printf("bitsize = %d\n", 8*sizeof(buffer[0])*(5 + pdusize));
#endif
	if(spi_xfer(device->slave, 8*sizeof(buffer[0])*(5 + pdusize), buffer, buffer, SPI_XFER_BEGIN | SPI_XFER_END) != 0) {
		free(rxbuf);
		return -1;
	}
#if 1
	printf("cmd sent\n");
#endif
	waitIrq(device);	/* wait for TX interrupt */
#if 1
	printf("tx done %02x\n", device->done);
#endif
	if(!(device->done & 0x80)) {
#if 1
		printf(" unknown TX interrupt %02x\n", device->done);
#endif
		free(rxbuf);
		return -1;
	}
	// FIXME: the first bytes received are not data bytes but the received PDU!
	// should we introduce some chained mbuf scheme???
	prepareIrq(device, rxbuf, 2+32*blocks);	/* prepare for receiving n*32 bytes RX */
	waitIrq(device);	/* wait for RX interrupt */
	// check for standard RX done or Collision or timeout or other errors
#if 1
	printf("rx done %02x\n", device->done);
	if(device->done & 0x01)
		printf("  timeout\n");
	if(device->done & 0x02)
		printf("  collision\n");
#endif
	if(rxbuf[0] != 0)
		printf("  rx flags %02x\n", rxbuf[0]);
	// if ok, read the flags byte from the received PDU to determine potential errors
	memcpy(data, rxbuf+2, 32*blocks);	// copy payload
	free(rxbuf);
	return 0;
}

int writeBlocks(struct trf7960 *device, uchar flags, uint64_t uid, uchar firstBlock, uchar blocks, uchar *data)
{ /* write single/multiple blocks */
	return -1;
}
 
/* this all below belongs to GUI/board driver */

/* define BeagleBoard + Expander hardware IF */

struct trf7960 rfid_board = {
	.bus = 2,	/* McSPI3 */
	.cs = 0,	/* CS0 */
	.clock = 1000000,	/* clock speed */
	.irq = 156,	/* KEYIRQ/GPIO156 */
	.en = 161,
	.en2 = 159,
	.vio = 18	/* we use 1.8V IO */
};

static int numfound;

static void found(struct trf7960 *device, uint64_t uid, int rssi)
{
	extern void status_set_status(int value);
//	int i;
	numfound++;
	printf("UID = %llX", uid);
//	for(i=0; i < 8; i++)
//		printf("%02x", (uid>>(8*(7-i))));
	printf(" rssi = %d/%d\n", rssi/8, rssi%8);
}

static int do_rfid(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	int len;
	static int statusinit;
	extern void status_set_status(int value);
	extern int status_init(void);
	
	if (argc < 2) {
		printf ("rfid: missing subcommand.\n");
		return (-1);
	}
	
	if (!statusinit) { // initialize status LEDs for scan/loop subcommands
		status_init();
		statusinit=1;
	}
	
	len = strlen (argv[1]);
	if (strncmp ("po", argv[1], 2) == 0) {
		return setPowerMode(&rfid_board, simple_strtoul(argv[2], NULL, 10)) < 0?1:0;
		
#if 1
	} else if (strncmp ("re", argv[1], 2) == 0) {
		int r;
		for(r=0; r <= 0x1f; r++)
			printf("%02x: %02x\n", r, readRegister(&rfid_board, r));
	} else if (strncmp ("co", argv[1], 2) == 0) {
		return 1;
	} else if (strncmp ("rw", argv[1], 2) == 0) {
#define MAX_SPI_BYTES 32
		static int   		bitlen;
		static uchar 		dout[MAX_SPI_BYTES];
		static uchar 		din[MAX_SPI_BYTES];
		char  *cp = 0;
		uchar tmp;
		int   j;
		int   rcode = 0;
		if (argc >= 3)
			bitlen = simple_strtoul(argv[2], NULL, 10);
		if (argc >= 4) {
			cp = argv[3];
			for(j = 0; *cp; j++, cp++) {
				tmp = *cp - '0';
				if(tmp > 9)
					tmp -= ('A' - '0') - 10;
				if(tmp > 15)
					tmp -= ('a' - 'A');
				if(tmp > 15) {
					printf("Hex conversion error on %c\n", *cp);
					return 1;
				}
				if((j % 2) == 0)
					dout[j / 2] = (tmp << 4);
				else
					dout[j / 2] |= tmp;
			}
		}
		if ((bitlen < 0) || (bitlen >  (MAX_SPI_BYTES * 8))) {
			printf("Invalid bitlen %d\n", bitlen);
			return 1;
		}
		
		if(spi_xfer(rfid_board.slave, bitlen, dout, din,
					SPI_XFER_BEGIN | SPI_XFER_END) != 0) {
			printf("Error during SPI transaction\n");
			rcode = 1;
		} else {
			for(j = 0; j < ((bitlen + 7) / 8); j++) {
				printf("%02X", din[j]);
			}
			printf("\n");
		}		
#endif
	} else if (strncmp ("sc", argv[1], 2) == 0) {
		numfound=0;
		setPowerMode(&rfid_board, TRF7960_POWER_RXTX_FULL);
		chooseProtocol(&rfid_board, 0x02);	/* ISO15693 26kbps one-sub 1-out-of-4 (default) */
		scanInventory(&rfid_board, 0x06 /* 0x26 1slot */, 0, &found);
		setPowerMode(&rfid_board, TRF7960_POWER_STANDBY);
		status_set_status(numfound > 0?0x3f:0x00);	/* LEDs on/off */
	} else if (strncmp ("lo", argv[1], 2) == 0) {
		setPowerMode(&rfid_board, TRF7960_POWER_RXTX_FULL);
		chooseProtocol(&rfid_board, 0x02);	/* ISO15693 26kbps one-sub 1-out-of-4 (default) */
		while(!tstc()) { // scan until key is pressed
			numfound=0;
			scanInventory(&rfid_board, 0x06 /* 0x26 1slot */, 0, &found);
			status_set_status(numfound);	/* LEDs binary count */
			udelay(300*1000);	// wait 0.3 seconds
		}
		setPowerMode(&rfid_board, TRF7960_POWER_STANDBY);
		if(tstc())
			getc();
	} else if (strncmp ("rb", argv[1], 2) == 0) { // read block
		uint64_t uid=0;	/* set by =uid (hex) */
		uchar firstBlock=0;	/* set by next parameter */
		uchar blocks=1;	/* set by +n parameter (decimal) */
		uchar *data;
		int r;
		int i=2;
		if(argv[i] && argv[i][0] == '=') {
			uid=simple_strtoull(argv[i]+1, NULL, 16);
			i++;
		}
		if(argv[i] && argv[i][0] == '+') {
			blocks=simple_strtoul(argv[i]+1, NULL, 10);
			i++;
		}
		if(argv[i]) {
			firstBlock=simple_strtoul(argv[i], NULL, 10);
			i++;
		}
		data=malloc(32*blocks);	// allocate enough memory for storing 32*blocks bytes!
		if(!data) {
			printf ("rfid %s: can't allocate buffer for %d blocks\n", argv[1], blocks);			
			return (1);
		}
		setPowerMode(&rfid_board, TRF7960_POWER_RXTX_FULL);
		chooseProtocol(&rfid_board, 0x02);	/* ISO15693 26kbps one-sub 1-out-of-4 (default) */
		r=readBlocks(&rfid_board, uid > 0?0x16:0x06, uid, firstBlock, blocks, data);
		printf("r=%d\n", r);
		for(i=0; i<blocks; i++) { // print data (hex)
			int b;
			for(b=0; b<32; b++) {
				if(b%16 == 0) {
					if(b == 0)
						printf("%03d: ", firstBlock+i);
					else
						printf("     ");					
				}
				printf("%02x", data[32*i+b]);
				if(b%16 == 15)
					printf("\n");
			}
		}
		setPowerMode(&rfid_board, TRF7960_POWER_STANDBY);
		free(data);
	} else if (strncmp ("wb", argv[1], 2) == 0) {
		// FIXME: write block
	} else {
		printf ("rfid: unknown operation: %s\n", argv[1]);
	}
	
	return (0);
}

U_BOOT_CMD(
	rfid,	5,	1,	do_rfid,
	"RFID utility command",
	"po[wer] n - set power state (0=off, 1=60kHz, 2=standby, 3=active, 4=rx, 5=half, 6=full)\n"
	"re[gisters] - read registers\n"
    "rw [bitlen [hexvalue]] - read/write SPI data block\n"
    "sc[an] - scan inventory\n"
	"lo[op] - permanently scan inventory until key is pressed\n"
    "rb[lock] [=uid] [+n] [first] - read n (default=1) 32 byte block(s) starting at first\n"
    "wb[lock] [=uid] [+n] first hexvalue - write n (default=1) 32 byte block(s) starting at first\n"
);
