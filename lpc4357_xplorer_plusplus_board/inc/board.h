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


#ifndef BOARD_H_
#define BOARD_H_

#include "chip.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SDRAM_BASE_ADDR 0x28000000
#define SPIFI_VTABLE 0x14000000
#define SPIFI_FLASH_SIZE (4*1024*1024)

#define LED4 0
#define LED5 1

/**
 * CHIP Pins for multiplexing, not GPIO port
 */
#define LED4_PORT 	9
#define LED4_PIN 	5
#define LED5_PORT 	9
#define LED5_PIN 	6
#define SW2_PORT	8
#define SW2_PIN		0

#define SW2 0
#define BOARD_SW2_GPIO_IRQn PIN_INT0_IRQn

/**
 * GPIO Port pins, not CHIP pins.
 */
typedef struct {
	uint8_t port;
	uint8_t pin;
} gpio_pin_info_t;

/**
 * Only required call from application's main function.
 * This will initialize Clocks, Pin Muxing and Peripherals available on this board.
 */
void board_init_all(void);
void board_init_clock_and_spifi(void);
void board_init_for_exec_from_spifi(void);
void board_init_gpio(void);
bool board_sw2_is_pressed(void);

/**
 * Initializes only the board specific USB1_VBUS pin, reset is done by USB ROM APIs
 */
void board_init_usb1(void);

/**
 * This function uses RIT timer (without interrupts, so it can used when global interrupt is disabled) for delay.
 */
void board_delay_in_seconds(uint32_t seconds);

void board_led_set(uint8_t led_number, bool on);
bool eth_auto_negotiate_speed(void);
bool eth_link_is_up(void);
uint32_t eth_phy_status(void);
void spifi_flash_init(void);
void spifi_flash_memmap(void);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_H_ */
