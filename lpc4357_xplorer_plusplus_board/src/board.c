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

/*
 * Most of the code is similar to lpc_board_keil_mcb_4357 (LPCOpen) and NGX Xplorer++ example codes
 * which in turn was a mix of many other sources without any licensing information.
 * Here all board initialization code is rewritten with naming consistency.
 * New project needs to be linked with lpc_chip_43xx and this board library.
 */

#include "spifi_flash.h"
#include "board.h"
#include "lan8720_phy.h"

static gpio_pin_info_t board_leds[] = {
		{5, 18}, //LED4
		{4, 11}, //LED5
};

static gpio_pin_info_t board_switches[] = {
		{4, 0}, //SW2
};

static void init_clock(void);
static void init_gpio(void);
static void init_sdram(void);
static void init_eth_phy(void);
static uint16_t eth_phy_read(uint32_t reg);
static void eth_phy_write(uint32_t reg, uint16_t val);

static uint32_t delay_minimum_ns;
static void init_delay(void);
static void delay_ns(uint64_t d);

#define DELAY_NS(x) delay_ns((x))
#define DELAY_US(x) DELAY_NS((x) * 1000ULL)
#define DELAY_MS(x) DELAY_US((x) * 1000ULL)
#define DELAY_MAX_SECONDS 20ULL

void board_init_all(void) {
	init_clock();
	init_gpio();
	init_delay();
	init_sdram();
	init_eth_phy();
	spifi_flash_init();
	spifi_flash_memmap();
	board_init_usb1();
}


static const PINMUX_GRP_T usb1_pinmuxing[] = {
	{2, 5,  (SCU_MODE_INBUFF_EN | SCU_MODE_INACT | SCU_MODE_FUNC2)},
};

void board_init_usb1(void) {
	Chip_SCU_SetPinMuxing(usb1_pinmuxing, sizeof(usb1_pinmuxing) / sizeof(PINMUX_GRP_T));
}

void board_init_clock_and_spifi(void) {
	init_clock();
	spifi_flash_init();
	spifi_flash_memmap();
}

void board_init_for_exec_from_spifi(void) {
	//It is assumed a flash program exists
	//which as has already called BoardClkAndSPIFI_Init
	//before jumping into SPIFI program.
	init_gpio();
	init_delay();
	init_sdram();
	init_eth_phy();
}

void board_init_gpio(void) {
	init_gpio();
}

void board_led_set(uint8_t led_number, bool on) {
	if (led_number < (sizeof(board_leds) / sizeof(board_leds[0]))) {
		Chip_GPIO_SetPinState(LPC_GPIO_PORT, board_leds[led_number].port, board_leds[led_number].pin, !on);
	}
}

static void init_delay(void) {
	double clk_period = (1.0 / MAX_CLOCK_FREQ) + 0.0000000005; //add 0.5ns to round to next higher integer ns.
	delay_minimum_ns = (uint32_t) (clk_period * 1000000000);

	NVIC_DisableIRQ(RITIMER_IRQn); //disable RIT irq in case application has installed it.

	Chip_RIT_Init(LPC_RITIMER);
}

static void delay_ns(uint64_t d) {
	uint32_t cmp_v = d / delay_minimum_ns;
	if (cmp_v == 0) {
		cmp_v = 1;
	}

	Chip_RIT_Disable(LPC_RITIMER);
	Chip_RIT_ClearInt(LPC_RITIMER);
	Chip_RIT_SetCOMPVAL(LPC_RITIMER, cmp_v);
	LPC_RITIMER->COUNTER = 0;
	Chip_RIT_Enable(LPC_RITIMER);

	while (Chip_RIT_GetIntStatus(LPC_RITIMER) != SET);
}

void board_delay_in_seconds(uint32_t seconds) {
	uint32_t i = seconds / DELAY_MAX_SECONDS;

	while(i > 0) {
		DELAY_MS(DELAY_MAX_SECONDS * 1000);
		i--;
	}

	DELAY_MS((seconds % DELAY_MAX_SECONDS) * 1000);
}

static const PINMUX_GRP_T pinclockmuxing[] = {
	{0, 0,  (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC0)},
	{0, 1,  (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC0)},
	{0, 2,  (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC0)},
	{0, 3,  (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC0)},
};


/**
 * Initialize chip clock from external 12MHz crystal oscillator.
 */
static void init_clock(void) {
	/* Setup FLASH acceleration to target clock rate prior to clock switch */
	Chip_CREG_SetFlashAcceleration(MAX_CLOCK_FREQ);

	Chip_SetupCoreClock(CLKIN_CRYSTAL, MAX_CLOCK_FREQ, true);

	/* Reset and enable 32Khz oscillator */
	LPC_CREG->CREG0 &= ~((1 << 3) | (1 << 2));
	LPC_CREG->CREG0 |= (1 << 1) | (1 << 0);

	SystemCoreClockUpdate();
}

static const PINMUX_GRP_T gpio_pinmux[] = {
	{LED4_PORT, LED4_PIN, (SCU_MODE_8MA_DRIVESTR | SCU_MODE_FUNC4)},
	{LED5_PORT, LED5_PIN, (SCU_MODE_8MA_DRIVESTR | SCU_MODE_FUNC0)},

	{SW2_PORT, SW2_PIN, (SCU_MODE_INBUFF_EN | SCU_MODE_INACT | SCU_MODE_FUNC3)},
};

bool board_sw2_is_pressed(void) {
	return !Chip_GPIO_ReadPortBit(LPC_GPIO_PORT, board_switches[SW2].port, board_switches[SW2].pin);
}

static void init_gpio(void) {
	int i;

	Chip_SCU_SetPinMuxing(gpio_pinmux, sizeof(gpio_pinmux) / sizeof(PINMUX_GRP_T));

	for (i = 0; i < (sizeof(board_leds) / sizeof(board_leds[0])); i++) {
			Chip_GPIO_SetPinDIROutput(LPC_GPIO_PORT, board_leds[i].port, board_leds[i].pin);
			board_led_set(i, false);
	}

	/* Configure input switch SW2 as falling edge interrupt on Pin Interrupt channel 0. */
	Chip_GPIO_SetPinDIRInput(LPC_GPIO_PORT, board_switches[SW2].port, board_switches[SW2].pin);
	Chip_PININT_SetPinModeEdge(LPC_GPIO_PIN_INT, PININTCH0);
	Chip_PININT_EnableIntLow(LPC_GPIO_PIN_INT, PININTCH0);
	Chip_SCU_GPIOIntPinSel(0, board_switches[SW2].port, board_switches[SW2].pin);
	//NVIC_EnableIRQ(PIN_INT0_IRQn);
}


static const PINMUX_GRP_T sdram_emc_pinmux[] = {
	/* External data lines D0 .. D31 */
	{0x1, 7,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC3)},
	{0x1, 8,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC3)},
	{0x1, 9,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC3)},
	{0x1, 10,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC3)},
	{0x1, 11,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC3)},
	{0x1, 12,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC3)},
	{0x1, 13,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC3)},
	{0x1, 14,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC3)},
	{0x5, 4,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC2)},
	{0x5, 5,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC2)},
	{0x5, 6,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC2)},
	{0x5, 7,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC2)},
	{0x5, 0,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC2)},
	{0x5, 1,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC2)},
	{0x5, 2,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC2)},
	{0x5, 3,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC2)},
	{0xD, 2,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC2)},
	{0xD, 3,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC2)},
	{0xD, 4,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC2)},
	{0xD, 5,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC2)},
	{0xD, 6,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC2)},
	{0xD, 7,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC2)},
	{0xD, 8,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC2)},
	{0xD, 9,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC2)},
	{0xE, 5,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC3)},
	{0xE, 6,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC3)},
	{0xE, 7,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC3)},
	{0xE, 8,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC3)},
	{0xE, 9,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC3)},
	{0xE, 10,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC3)},
	{0xE, 11,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC3)},
	{0xE, 12,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC3)},

	 /* Address lines A0 .. A11 */
	{0x2, 9,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC3)},
	{0x2, 10,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC3)},
	{0x2, 11,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC3)},
	{0x2, 12,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC3)},
	{0x2, 13,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC3)},
	{0x1, 0,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC2)},
	{0x1, 1,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC2)},
	{0x1, 2,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC2)},
	{0x2, 8,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC3)},
	{0x2, 7,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC3)},
	{0x2, 6,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC2)},
	{0x2, 2,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC2)},
	 /* A12 is not present
	 {0x2, 1,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC2)},
	 */

	/* BA 0 .. 1 */
	{0x2, 0,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC2)},
	{0x6, 8,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC1)},

	/* CS */
	{0x6, 9,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC3)},
	/* WE */
	{0x1, 6,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC3)},
	/* CAS */
	{0x6, 4,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC3)},
	/* RAS */
	{0x6, 5,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC3)},

	/* CKEOUT0 */
	{0x6, 11,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC3)},
	/* DQMOUT 0 .. 3 */
	{0x6, 12,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC3)},
	{0x6, 10,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC3)},
	{0xD, 0,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC2)},
	{0xE, 13,
	 (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC3)},
};

#if 0
static const IP_EMC_DYN_CONFIG_T MT48LC8M32_config = {
	EMC_NANOSECOND(64000000 / 4096),	/* Row refresh time */
	0x01,	/* Command Delayed */
	EMC_NANOSECOND(18),
	EMC_NANOSECOND(42),
	EMC_NANOSECOND(70),
	EMC_CLOCK(0x05),
	EMC_CLOCK(0x05),
	EMC_NANOSECOND(12),
	EMC_NANOSECOND(60),
	EMC_NANOSECOND(60),
	EMC_NANOSECOND(70),
	EMC_NANOSECOND(12),
	EMC_CLOCK(0x02),
	{
		{
			EMC_ADDRESS_DYCS0,
			3,	/* RAS */

			EMC_DYN_MODE_WBMODE_PROGRAMMED |
			EMC_DYN_MODE_OPMODE_STANDARD |
			EMC_DYN_MODE_CAS_3 |
			EMC_DYN_MODE_BURST_TYPE_SEQUENTIAL |
			EMC_DYN_MODE_BURST_LEN_4,

			EMC_DYN_CONFIG_DATA_BUS_32 |
			EMC_DYN_CONFIG_8Mx32_4BANKS_13ROWS_8COLS |
			EMC_DYN_CONFIG_MD_SDRAM
		},
		{0, 0, 0, 0},
		{0, 0, 0, 0},
		{0, 0, 0, 0}
	}
};
#endif

static uint32_t NS2CLK(uint32_t freq, uint32_t time) {
	return (((uint64_t)time*freq/1000000000));
}

static void init_sdram(void) {
	#define CLK0_DELAY 7
	#define EMC_B_ENABLE 					(1 << 19)
	#define EMC_ENABLE 						(1 << 0)
	#define EMC_CE_ENABLE 					(1 << 0)
	#define EMC_CS_ENABLE 					(1 << 1)
	#define EMC_CLOCK_DELAYED_STRATEGY 		(0 << 0)
	#define EMC_COMMAND_DELAYED_STRATEGY 	(1 << 0)
	#define EMC_COMMAND_DELAYED_STRATEGY2 	(2 << 0)
	#define EMC_COMMAND_DELAYED_STRATEGY3 	(3 << 0)
	#define EMC_INIT(i) 					((i) << 7)
	#define EMC_NORMAL 						(0)
	#define EMC_MODE 						(1)
	#define EMC_PRECHARGE_ALL 				(2)
	#define EMC_NOP 						(3)


	#define EMC_SDRAM_WIDTH_8_BITS		0
	#define EMC_SDRAM_WIDTH_16_BITS		1
	#define EMC_SDRAM_WIDTH_32_BITS		2

	#define EMC_SDRAM_SIZE_16_MBITS		0
	#define EMC_SDRAM_SIZE_64_MBITS		1
	#define EMC_SDRAM_SIZE_128_MBITS	2
	#define EMC_SDRAM_SIZE_256_MBITS	3
	#define EMC_SDRAM_SIZE_512_MBITS	4

	#define EMC_SDRAM_DATA_BUS_16_BITS	0
	#define EMC_SDRAM_DATA_BUS_32_BITS	1
	#define EMC_SDRAM_REFRESH(freq,time)  (((uint64_t)((uint64_t)time * freq)/16000000000ull)+1)

	int i;

	/* Reset EMC Controller in case already running, example debugging from SRAM. */
	Chip_RGU_TriggerReset(RGU_EMC_RST);

	Chip_SCU_SetPinMuxing(sdram_emc_pinmux, sizeof(sdram_emc_pinmux) / sizeof(PINMUX_GRP_T));

	for (i = 0; i < (sizeof(pinclockmuxing) / sizeof(pinclockmuxing[0])); i++) {
		Chip_SCU_ClockPinMuxSet(pinclockmuxing[i].pinnum, pinclockmuxing[i].modefunc);
	}

	/* Setup EMC Delays */
	/* Move all clock delays together */
	LPC_SCU->EMCDELAYCLK = ((CLK0_DELAY) | (CLK0_DELAY << 4) | (CLK0_DELAY << 8) | (CLK0_DELAY << 12));

	/* Setup EMC Clock Divider for divide by 2 - this is done in both the CCU (clocking)
		   and CREG. For frequencies over 120MHz, a divider of 2 must be used. For frequencies
		   less than 120MHz, a divider of 1 or 2 is ok. */
	Chip_Clock_EnableOpts(CLK_MX_EMC_DIV, true, true, 2);
	LPC_CREG->CREG6 |= (1 << 16);

	/* Enable EMC clock */
	Chip_Clock_Enable(CLK_MX_EMC);

	/* Init EMC Controller -Enable-LE mode */
	//Chip_EMC_Init(1, 0, 0);
	/* Init EMC Dynamic Controller */
	//Chip_EMC_Dynamic_Init((IP_EMC_DYN_CONFIG_T *) &MT48LC8M32_config);

	// adjust the CCU delaye for EMI (default to zero)
	//LPC_SCU->EMCCLKDELAY = (CLK0_DELAY | (CLKE0_DELAY << 16));
	// Move all clock delays together

	/* Initialize EMC to interface with SDRAM */
	LPC_EMC->CONTROL 			= 0x00000001;   /* Enable the external memory controller */
	LPC_EMC->CONFIG 			= 0;

	//LPC_EMC->DYNAMICCONFIG0 	= 1<<14 | 1<<12 | 2<<9 | 1<<7;
	LPC_EMC->DYNAMICCONFIG0 	= 1<<14 | 0<<12 | 2<<9 | 1<<7; //256 Mb (8Mx32), 4 banks, row length = 12, column length = 9


	LPC_EMC->DYNAMICRASCAS0 	= (3 << 0) | (3 << 8);      // aem
	/* LPC_EMC->DYNAMICRASCAS2 	= (3 << 0) | (3 << 8);  // aem */

	LPC_EMC->DYNAMICREADCONFIG	= EMC_COMMAND_DELAYED_STRATEGY;

	LPC_EMC->DYNAMICRP 			= NS2CLK(MAX_CLOCK_FREQ, 20);
	LPC_EMC->DYNAMICRAS 		= NS2CLK(MAX_CLOCK_FREQ, 42);
	LPC_EMC->DYNAMICSREX 		= NS2CLK(MAX_CLOCK_FREQ, 63);
	LPC_EMC->DYNAMICAPR 		= 5;
	LPC_EMC->DYNAMICDAL 		= 5;
	LPC_EMC->DYNAMICWR 			= 2;
	LPC_EMC->DYNAMICRC 			= NS2CLK(MAX_CLOCK_FREQ, 63);
	LPC_EMC->DYNAMICRFC 		= NS2CLK(MAX_CLOCK_FREQ, 63);
	LPC_EMC->DYNAMICXSR 		= NS2CLK(MAX_CLOCK_FREQ, 63);
	LPC_EMC->DYNAMICRRD 		= NS2CLK(MAX_CLOCK_FREQ, 14);
	LPC_EMC->DYNAMICMRD 		= 2;

	LPC_EMC->DYNAMICCONTROL 	= EMC_CE_ENABLE | EMC_CS_ENABLE | EMC_INIT(EMC_NOP);

	DELAY_US(100);

	LPC_EMC->DYNAMICCONTROL 	= EMC_CE_ENABLE | EMC_CS_ENABLE | EMC_INIT(EMC_PRECHARGE_ALL);

	LPC_EMC->DYNAMICREFRESH 	= EMC_SDRAM_REFRESH(MAX_CLOCK_FREQ, 70);

	DELAY_US(100);

	LPC_EMC->DYNAMICREFRESH 	= 90; //200;//20;//50;

	LPC_EMC->DYNAMICCONTROL 	= EMC_CE_ENABLE | EMC_CS_ENABLE | EMC_INIT(EMC_MODE);

	/* burst size 4 */
	*((volatile uint32_t *) (SDRAM_BASE_ADDR | ((0x32) << (9 + 2 + 2)))); //RBC mode


	LPC_EMC->DYNAMICCONTROL 	= 0; // EMC_CE_ENABLE | EMC_CS_ENABLE;
	LPC_EMC->DYNAMICCONFIG0 	|= EMC_B_ENABLE;
}

static const PINMUX_GRP_T eth_phy_pinmux[] = {

	{0x7, 7,  (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC6)},
	{0x1, 17, (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC3)},
	{0x1, 18, (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC3)},
	{0x1, 20, (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC3)},
	{0x1, 19, (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC0)},
	{0x0, 1,  (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC6)},
	{0x1, 15, (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC3)},
	{0x0, 0,  (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC2)},
	{0x1, 16, (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC7)},
	{0xc, 9,  (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC3)},
	{0xd, 14, (SCU_MODE_PULLUP | SCU_MODE_FUNC4)}, //GPIO6[28]
};

#define ETH_PHY_RESET_GPIO_PORT 6
#define ETH_PHY_RESET_GPIO_PIN 28
#define DEF_PHY_ADDRESS 1
#define MAX_PHY_STATUS_ATTEMPTS 250

static void init_eth_phy(void) {

	uint32_t id1, id2;

	Chip_SCU_SetPinMuxing(eth_phy_pinmux, sizeof(eth_phy_pinmux) / sizeof(PINMUX_GRP_T));

	Chip_Clock_SetBaseClock(CLK_BASE_PHY_RX, CLKIN_ENET_TX, true, false);
	Chip_Clock_SetBaseClock(CLK_BASE_PHY_TX, CLKIN_ENET_TX, true, false);

	Chip_GPIO_SetPinDIROutput(LPC_GPIO_PORT, ETH_PHY_RESET_GPIO_PORT, ETH_PHY_RESET_GPIO_PIN); //phy reset

	Chip_GPIO_SetPinState(LPC_GPIO_PORT, ETH_PHY_RESET_GPIO_PORT, ETH_PHY_RESET_GPIO_PIN, 0);
	DELAY_MS(2);
	Chip_GPIO_SetPinState(LPC_GPIO_PORT, ETH_PHY_RESET_GPIO_PORT, ETH_PHY_RESET_GPIO_PIN, 1);

	Chip_ENET_RMIIEnable(LPC_ETHERNET);
	Chip_ENET_Init(LPC_ETHERNET, DEF_PHY_ADDRESS);

	//reset phy
	eth_phy_write(PHY_REG_BMCR, PHY_BMCR_RESET);

	//wait for reset complete
	while ((eth_phy_read (PHY_REG_BMCR) & PHY_BMCR_RESET) != 0) {
		DELAY_US(100);
	}

	id1 = eth_phy_read (PHY_REG_IDR1);
	id2 = eth_phy_read (PHY_REG_IDR2);

	if (((id1 << 16) | id2) != LAN8720_ID ) {
		while(1); //phy read error.
	}

	eth_auto_negotiate_speed();
}

static uint16_t eth_phy_read(uint32_t reg) {
	while(Chip_ENET_IsMIIBusy(LPC_ETHERNET) == true);
	Chip_ENET_StartMIIRead(LPC_ETHERNET, reg);
	while(Chip_ENET_IsMIIBusy(LPC_ETHERNET) == true);
	return Chip_ENET_ReadMIIData(LPC_ETHERNET);
}

static void eth_phy_write(uint32_t reg, uint16_t val) {
	while(Chip_ENET_IsMIIBusy(LPC_ETHERNET) == true);
	Chip_ENET_StartMIIWrite(LPC_ETHERNET, reg, val);
	while(Chip_ENET_IsMIIBusy(LPC_ETHERNET) == true);
}

bool eth_auto_negotiate_speed(void) {
	uint32_t attempts = MAX_PHY_STATUS_ATTEMPTS;
	uint16_t val;

	eth_phy_write(PHY_REG_BMCR, PHY_AUTO_NEG);

	while (attempts > 0) {
		if ((eth_phy_read(PHY_REG_BMSR) & PHY_AUTO_NEG_DONE) != 0 ) {
			val = eth_phy_read( PHY_REG_SPCON );
			val &= PHY_REG_HCDSPEED_MASK;
			switch(val)
			{
				case PHY_REG_HCDSPEED_10MB_FULLD:
					Chip_ENET_SetDuplex(LPC_ETHERNET, true);
					Chip_ENET_SetSpeed(LPC_ETHERNET, false);
					break;

				case PHY_REG_HCDSPEED_100MB_HALFD:
					Chip_ENET_SetDuplex(LPC_ETHERNET, false);
					Chip_ENET_SetSpeed(LPC_ETHERNET, true);
					break;

				case PHY_REG_HCDSPEED_100MB_FULLD:
					Chip_ENET_SetDuplex(LPC_ETHERNET, true);
					Chip_ENET_SetSpeed(LPC_ETHERNET, true);
					break;

				default:
					Chip_ENET_SetDuplex(LPC_ETHERNET, false);
					Chip_ENET_SetSpeed(LPC_ETHERNET, false);
					break;
			}
			return true;
		}
		DELAY_MS(10);
		attempts--;
	}

	return false;
}

bool eth_link_is_up(void) {
	uint32_t attempts = MAX_PHY_STATUS_ATTEMPTS;

	while (attempts > 0) {
		if ((eth_phy_read(PHY_REG_BMSR) & LINK_VALID_STS) != 0 ) {
			return true;
		}
		DELAY_MS(10);
		attempts--;
	}

	return false;
}

uint32_t eth_phy_status(void) {
	return eth_phy_read(PHY_REG_BMSR);
}

static const PINMUX_GRP_T spifi_pinmux[] = {

	{0x3, 7, (MD_PLN_FAST | SCU_MODE_FUNC3)}, /* P3_7: SPIFI_MOSI/IO0 */
	{0x3, 6, (MD_PLN_FAST | SCU_MODE_FUNC3)}, /* P3_6: SPIFI_MISO/IO1 */
	{0x3, 5, (MD_PLN_FAST | SCU_MODE_FUNC3)}, /* P3_5: SPIFI IO2 */
	{0x3, 4, (MD_PLN_FAST | SCU_MODE_FUNC3)}, /* P3_4: SPIFI IO3 */
	{0x3, 3, (MD_PLN_FAST | SCU_MODE_FUNC3)}, /* P3_3: SPIFI SCK */
	{0x3, 8, (MD_PLN_FAST | SCU_MODE_FUNC3)}, /* P3_8: SPIFI CS */
};

void spifi_flash_init(void) {

	uint32_t div;

	Chip_SCU_SetPinMuxing(spifi_pinmux, sizeof(spifi_pinmux) / sizeof(PINMUX_GRP_T));

	div = MAX_CLOCK_FREQ / SPIFI_MAX_CLOCK_HZ;
	if ((MAX_CLOCK_FREQ % SPIFI_MAX_CLOCK_HZ) == 0)
		Chip_Clock_SetDivider(CLK_IDIV_E, CLKIN_MAINPLL, div);
	else
		Chip_Clock_SetDivider(CLK_IDIV_E, CLKIN_MAINPLL, div + 1);

	Chip_Clock_SetBaseClock(CLK_BASE_SPIFI, CLKIN_IDIVE, true, false);

	if (spifi_init() != 0)
		while(1);
}

void spifi_flash_memmap(void) {
	spifi_set_mem_mode();
}
