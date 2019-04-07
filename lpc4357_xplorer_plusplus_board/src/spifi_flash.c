/*
 * Copyright (C) 2019 Ravikiran Bukkasagara, <contact@ravikiranb.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH
 * THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "spifi_flash.h"

typedef struct
{
	unsigned long CTRL;
	unsigned long COMMAND;
	unsigned long ADDR;
	unsigned long IDATA;
	unsigned long CLIMIT;
	union {
		unsigned char byte;
		unsigned short hword;
		unsigned long word;
	} DATA;
	unsigned long MCOMMAND;
	unsigned long STAT;
} LPC_SPIFI_Type;

#define LPC_SPIFI ((volatile LPC_SPIFI_Type*) 0x40003000)

/* Status Register bits */
#define RESET 			(1<<4)
#define CMD 			(1<<1)
#define MCINIT			(1<<0)

/* Command Register bits */
#define DATALEN_MASK 	0x3fff
#define DATALEN(x)	 	(((x) & DATALEN_MASK))
#define POLL 			(1<<14)
#define DOUT 			(1<<15)
#define INTLEN_MASK		0x7
#define INTLEN(x)		(((x) & INTLEN_MASK)<<16)
#define FIELDFORM_MASK	0x3
#define FIELDFORM(x)	(((x) & FIELDFORM_MASK)<<19)
#define FRAMEFORM_MASK	(0x7)
#define FRAMEFORM(x)	(((x) & FRAMEFORM_MASK)<<21)
#define OPCODE_MASK		0xff
#define OPCODE(x)		(((x) & OPCODE_MASK)<<24)

#define NULL 0

enum {
	CMD_READ_STATUS_FOR_READY = 1,
	CMD_WRITE_ENABLE,	
	CMD_WRITE_CONFIG_REG,
	CMD_READ_ID,
	CMD_ERASE_CHIP,
	CMD_ERASE_SECTOR,
};

static void wait_cmd_over(void)
{
	while(LPC_SPIFI->STAT & CMD);
}

static void send_cmd(int command, unsigned char* data, unsigned int datalen, int qmode)
{
	unsigned int i;
	
	switch(command)
	{
		case CMD_READ_STATUS_FOR_READY:
			LPC_SPIFI->COMMAND = POLL | FIELDFORM(qmode) | FRAMEFORM(0x1) | OPCODE(0x5);
			LPC_SPIFI->DATA.byte;
			break;
		case CMD_WRITE_ENABLE:
			LPC_SPIFI->COMMAND = FIELDFORM(qmode) | FRAMEFORM(0x1) | OPCODE(0x6);
			break;
		case CMD_WRITE_CONFIG_REG:
			LPC_SPIFI->COMMAND = DATALEN(datalen) | DOUT | FIELDFORM(qmode) | FRAMEFORM(0x1) | OPCODE(0x1);
			for (i = 0; i < datalen; i++)
				LPC_SPIFI->DATA.byte = data[i];
			break;
		case CMD_READ_ID:
			LPC_SPIFI->COMMAND = DATALEN(datalen) | FIELDFORM(qmode) | FRAMEFORM(0x1) | OPCODE(0x9f);
			for (i = 0; i < datalen; i++)
				data[i] = LPC_SPIFI->DATA.byte;
			break;
		case CMD_ERASE_CHIP:
			LPC_SPIFI->COMMAND = FIELDFORM(qmode) | FRAMEFORM(0x1) | OPCODE(0x60);
			break;
		default:
			return;
	}
	wait_cmd_over();
}

int spifi_program_page(unsigned long address, const unsigned char* data, int datalen)
{
	int i;
	
	if (address & (265-1))
		return 1; /* must be 256 byte aligned*/
	
	if (datalen > 256)
		return 1; 
	
	send_cmd(CMD_WRITE_ENABLE, NULL, 0, 0);
	
	LPC_SPIFI->ADDR = address;
	LPC_SPIFI->COMMAND = DATALEN(datalen) | DOUT | FIELDFORM(1) | FRAMEFORM(0x4) | OPCODE(0x32);
	
	for (i = 0; i < datalen; i++)
		LPC_SPIFI->DATA.byte = data[i];
	
	wait_cmd_over();
	
	send_cmd(CMD_READ_STATUS_FOR_READY, NULL, 0, 0);
	
	return 0;
}

int spifi_erase_sector(unsigned long address)
{
	send_cmd(CMD_WRITE_ENABLE, NULL, 0, 0);
	
	LPC_SPIFI->ADDR = address;
	LPC_SPIFI->COMMAND = FIELDFORM(0) | FRAMEFORM(0x4) | OPCODE(0xd8);
	
	wait_cmd_over();
	
	send_cmd(CMD_READ_STATUS_FOR_READY, NULL, 0, 0);
	
	return 0;
}

int spifi_erase_chip(void)
{
	send_cmd(CMD_WRITE_ENABLE, NULL, 0, 0);
	send_cmd(CMD_ERASE_CHIP, NULL, 0, 0);
	send_cmd(CMD_READ_STATUS_FOR_READY, NULL, 0, 0);
	
	return 0;
}

int spifi_init(void)
{
	static unsigned char data[10];
	
	LPC_SPIFI->CTRL = 0xfffff | (1<<30) |  (1<<29); //FBCLK + RFCLK
	
	LPC_SPIFI->STAT = RESET; /*Reset SPIFI controller only */
	while(LPC_SPIFI->STAT & RESET);
	
	send_cmd(CMD_READ_STATUS_FOR_READY, NULL, 0, 0);
	
	/* Turn on QUAD mode */
	send_cmd(CMD_WRITE_ENABLE, NULL, 0, 0);
	
	data[0] = 0; /* Status Register */
	data[1] = 2; /* Configuration Register */
	send_cmd(CMD_WRITE_CONFIG_REG, data, 2, 0);
	
	send_cmd(CMD_READ_STATUS_FOR_READY, NULL, 0, 0);
	
	send_cmd(CMD_READ_ID, data, 4, 0);
	
	if (data[0] == 0x01)
		if (data[1] == 0x02)
			if (data[2] == 0x15)
				return 0;

	
	return 1;
}

int spifi_set_mem_mode(void)
{
#if 1 /*High Performance Quad IO */
	LPC_SPIFI->IDATA = 0xa0a0a0;
	LPC_SPIFI->ADDR = 0;
	LPC_SPIFI->COMMAND = DATALEN(1) | INTLEN(3) | FIELDFORM(0x2) | FRAMEFORM(0x4) | OPCODE(0xeb);
	LPC_SPIFI->DATA.byte;
	wait_cmd_over();
	
	LPC_SPIFI->STAT = RESET; /*Reset SPIFI controller only. External SPI Flash is in High Performance QIO mode */
	while(LPC_SPIFI->STAT & RESET);
	
	LPC_SPIFI->IDATA = 0xa0a0a0;
	LPC_SPIFI->MCOMMAND = INTLEN(3) | FIELDFORM(0x3) | FRAMEFORM(0x6) | OPCODE(0xeb);
#else
	LPC_SPIFI->STAT = RESET; /*Reset SPIFI controller only. QIO mode */
	while(LPC_SPIFI->STAT & RESET);
	
	LPC_SPIFI->IDATA = 0;
	LPC_SPIFI->MCOMMAND = INTLEN(1) | FIELDFORM(1) | FRAMEFORM(4) | OPCODE(0x6b);
#endif 
	
	while(!(LPC_SPIFI->STAT & MCINIT));
	
	return 0;
}



