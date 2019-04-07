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

#include "board.h"

#include <cr_section_macros.h>
#include <stdio.h>
#include <string.h>

#include "app_usbd_cfg.h"
#include "hid_generic.h"



/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/
static USBD_HANDLE_T g_hUsb;

/* Endpoint 0 patch that prevents nested NAK event processing */
static uint32_t g_ep0RxBusy = 0;/* flag indicating whether EP0 OUT/RX buffer is busy. */
static USB_EP_HANDLER_T g_Ep0BaseHdlr;	/* variable to store the pointer to base EP0 handler */

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/
const USBD_API_T *g_pUsbApi;
bool is_device_active = false;

/*****************************************************************************
 * Private functions
 ****************************************************************************/
static ErrorCode_t device_configured (USBD_HANDLE_T hUsb);
static ErrorCode_t device_suspended (USBD_HANDLE_T hUsb);
static ErrorCode_t device_reset (USBD_HANDLE_T hUsb);

#define USB_IRQ_PRIORITY 0
#define MCPWM_IRQ_PRIORITY 2
#define GPIO_IRQ_PRIORITY 1

/* EP0_patch part of WORKAROUND for artf45032. */
ErrorCode_t EP0_patch(USBD_HANDLE_T hUsb, void *data, uint32_t event)
{
	switch (event) {
	case USB_EVT_OUT_NAK:
		if (g_ep0RxBusy) {
			/* we already queued the buffer so ignore this NAK event. */
			return LPC_OK;
		}
		else {
			/* Mark EP0_RX buffer as busy and allow base handler to queue the buffer. */
			g_ep0RxBusy = 1;
		}
		break;

	case USB_EVT_SETUP:	/* reset the flag when new setup sequence starts */
	case USB_EVT_OUT:
		/* we received the packet so clear the flag. */
		g_ep0RxBusy = 0;
		break;
	}
	return g_Ep0BaseHdlr(hUsb, data, event);
}

/*****************************************************************************
 * Public functions
 ****************************************************************************/

/**
 * @brief	Handle interrupt from USB1
 * @return	Nothing
 */
void USB_IRQHandler(void)
{
	USBD_API->hw->ISR(g_hUsb);
}

/**
 * @brief	Find the address of interface descriptor for given class type.
 * @return	If found returns the address of requested interface else returns NULL.
 */
USB_INTERFACE_DESCRIPTOR *find_IntfDesc(const uint8_t *pDesc, uint32_t intfClass)
{
	USB_COMMON_DESCRIPTOR *pD;
	USB_INTERFACE_DESCRIPTOR *pIntfDesc = 0;
	uint32_t next_desc_adr;

	pD = (USB_COMMON_DESCRIPTOR *) pDesc;
	next_desc_adr = (uint32_t) pDesc;

	while (pD->bLength) {
		/* is it interface descriptor */
		if (pD->bDescriptorType == USB_INTERFACE_DESCRIPTOR_TYPE) {

			pIntfDesc = (USB_INTERFACE_DESCRIPTOR *) pD;
			/* did we find the right interface descriptor */
			if (pIntfDesc->bInterfaceClass == intfClass) {
				break;
			}
		}
		pIntfDesc = 0;
		next_desc_adr = (uint32_t) pD + pD->bLength;
		pD = (USB_COMMON_DESCRIPTOR *) next_desc_adr;
	}

	return pIntfDesc;
}

uint32_t ticks_in_one_msec;

#define DEFAULT_BLINKS_PER_SECOND 10
#define BLINK_PERIOD_MS(x) (1000 / (x))
#define BLINK_ONTIME_MS(x) (BLINK_PERIOD_MS((x)) / 2)

uint32_t MCPWM_CH1_Init(uint32_t period_ms, uint32_t ontime_ms) {
	uint32_t mcon_clk_hz = Chip_Clock_GetRate(CLK_APB1_MOTOCON);
	uint32_t ticks_in_one_time_unit = mcon_clk_hz / 1000;
	LPC_MCPWM->LIM[1] =  period_ms * ticks_in_one_time_unit;
	LPC_MCPWM->MAT[1] =  ontime_ms * ticks_in_one_time_unit;

	// LPC_MCPWM->INTEN_SET = (1<<4); //Enable ILIM1

	// Start timer of channel 1 in edge-aligned mode.
	// By default TC runs in timer mode.
	LPC_MCPWM->CON_SET = (1<<8);

	return ticks_in_one_time_unit;
}

void MCPWM_CH1_Update(uint8_t rate) {
	// Limit rate
	if (rate < 1)
		rate = 1;
	else if (rate > 20)
		rate = 20;

	// Stop timer
	LPC_MCPWM->CON_CLR = (1<<8);

	// Update PWM cycle
	LPC_MCPWM->LIM[1] =  BLINK_PERIOD_MS(rate) * ticks_in_one_msec;
	LPC_MCPWM->MAT[1] =  BLINK_ONTIME_MS(rate) * ticks_in_one_msec;
	LPC_MCPWM->TC[1] = 0;

	// restart timer.
	LPC_MCPWM->CON_SET = (1<<8);

}


/**
 * @brief	main routine for USB device example
 * @return	Function should not exit.
 */
int main(void)
{
	USBD_API_INIT_PARAM_T usb_param;
	USB_CORE_DESCS_T desc;
	ErrorCode_t ret = LPC_OK;
	USB_CORE_CTRL_T *pCtrl;

	// Change LED4 driver from GPIO to Motor Control PWM channel 1 - MCOA1/B1
	Chip_SCU_PinMuxSet(LED4_PORT, LED4_PIN, (SCU_MODE_8MA_DRIVESTR | SCU_MODE_FUNC1));

	ticks_in_one_msec = MCPWM_CH1_Init(BLINK_PERIOD_MS(DEFAULT_BLINKS_PER_SECOND), BLINK_ONTIME_MS(DEFAULT_BLINKS_PER_SECOND));

#ifndef USE_USB1
#error "Use USB1 for device role"
#endif
	/* enable clocks and pinmux */
	USB_init_pin_clk();

	/* Init USB API structure */
	g_pUsbApi = (const USBD_API_T *) LPC_ROM_API->usbdApiBase;

	/* initialize call back structures */
	memset((void *) &usb_param, 0, sizeof(USBD_API_INIT_PARAM_T));
	usb_param.usb_reg_base = LPC_USB_BASE;
	usb_param.mem_base = USB_STACK_MEM_BASE;
	usb_param.mem_size = USB_STACK_MEM_SIZE;
	usb_param.max_num_ep = 2;
	usb_param.USB_Configure_Event = device_configured;
	usb_param.USB_Suspend_Event = device_suspended;
	usb_param.USB_Reset_Event = device_reset;


	/* Set the USB descriptors */
	desc.device_desc = (uint8_t *) USB_DeviceDescriptor;
	desc.string_desc = (uint8_t *) USB_StringDescriptor;

#ifdef USE_USB0
	desc.high_speed_desc = USB_HsConfigDescriptor;
	desc.full_speed_desc = USB_FsConfigDescriptor;
	desc.device_qualifier = (uint8_t *) USB_DeviceQualifier;
#else
	/* Note, to pass USBCV test full-speed only devices should have both
	 * descriptor arrays point to same location and device_qualifier set
	 * to 0.
	 */
	desc.high_speed_desc = USB_FsConfigDescriptor;
	desc.full_speed_desc = USB_FsConfigDescriptor;
	desc.device_qualifier = 0;
#endif

	NVIC_SetPriority(LPC_USB_IRQ, USB_IRQ_PRIORITY);
	NVIC_SetPriority(BOARD_SW2_GPIO_IRQn, GPIO_IRQ_PRIORITY);
	NVIC_EnableIRQ(BOARD_SW2_GPIO_IRQn);
	NVIC_SetPriorityGrouping( 0 );

	/* USB Initialization */
	ret = USBD_API->hw->Init(&g_hUsb, &desc, &usb_param);
	if (ret == LPC_OK) {

		/*	WORKAROUND for artf45032 ROM driver BUG:
		    Due to a race condition there is the chance that a second NAK event will
		    occur before the default endpoint0 handler has completed its preparation
		    of the DMA engine for the first NAK event. This can cause certain fields
		    in the DMA descriptors to be in an invalid state when the USB controller
		    reads them, thereby causing a hang.
		 */
		pCtrl = (USB_CORE_CTRL_T *) g_hUsb;	/* convert the handle to control structure */
		g_Ep0BaseHdlr = pCtrl->ep_event_hdlr[0];/* retrieve the default EP0_OUT handler */
		pCtrl->ep_event_hdlr[0] = EP0_patch;/* set our patch routine as EP0_OUT handler */

		ret = usb_hid_init(g_hUsb,
						   (USB_INTERFACE_DESCRIPTOR *) &USB_FsConfigDescriptor[sizeof(USB_CONFIGURATION_DESCRIPTOR)],
						   &usb_param.mem_base,
						   &usb_param.mem_size);
		if (ret == LPC_OK) {

			/*  enable USB interrrupts */
			NVIC_EnableIRQ(LPC_USB_IRQ);
			/* now connect */
			USBD_API->hw->Connect(g_hUsb, 1);
		}
	}

	while (1) {
		// Complete program is interrupt driven.
		// Sleep until next IRQ happens.
		__WFI();
	}
}

static ErrorCode_t device_configured (USBD_HANDLE_T hUsb)
{
	is_device_active = true;
	return LPC_OK;
}

static ErrorCode_t device_suspended (USBD_HANDLE_T hUsb)
{
	is_device_active = false;
	return LPC_OK;
}

static ErrorCode_t device_reset (USBD_HANDLE_T hUsb)
{
	is_device_active = false;
	return LPC_OK;
}


