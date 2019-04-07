/*
 * @brief This file contains USB HID Generic example using USB ROM Drivers.
 *
 * @note
 * Copyright(C) NXP Semiconductors, 2013
 * All rights reserved.
 *
 * @par
 * Software that is described herein is for illustrative purposes only
 * which provides customers with programming information regarding the
 * LPC products.  This software is supplied "AS IS" without any warranties of
 * any kind, and NXP Semiconductors and its licensor disclaim any and
 * all warranties, express or implied, including all implied warranties of
 * merchantability, fitness for a particular purpose and non-infringement of
 * intellectual property rights.  NXP Semiconductors assumes no responsibility
 * or liability for the use of the software, conveys no license or rights under any
 * patent, copyright, mask work right, or any other intellectual property rights in
 * or to any products. NXP Semiconductors reserves the right to make changes
 * in the software without notification. NXP Semiconductors also makes no
 * representation or warranty that such application will be suitable for the
 * specified use without further testing or modification.
 *
 * @par
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, under NXP Semiconductors' and its
 * licensor's relevant copyrights in the software, without fee, provided that it
 * is used in conjunction with NXP Semiconductors microcontrollers.  This
 * copyright, permission, and disclaimer notice must appear in all copies of
 * this code.
 */

#include "board.h"
#include <stdint.h>
#include <string.h>
#include "usbd_rom_api.h"

/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/

/* Buffer to hold report data */
typedef struct {
	uint8_t out_report;
	uint8_t in_report;
} report_data_t;

static report_data_t *report_data;

static int sw2_intr_report_pending;
static USBD_HANDLE_T g_hUsb;
/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/
extern bool is_device_active;
extern const uint8_t HID_ReportDescriptor[];
extern const uint16_t HID_ReportDescSize;

extern void MCPWM_CH1_Update(uint8_t rate);

/*****************************************************************************
 * Private functions
 ****************************************************************************/

/*  HID get report callback function. */
static ErrorCode_t HID_GetReport(USBD_HANDLE_T hHid, USB_SETUP_PACKET *pSetup, uint8_t * *pBuffer, uint16_t *plength)
{
	/* ReportID = SetupPacket.wValue.WB.L; */
	switch (pSetup->wValue.WB.H) {
	case HID_REPORT_INPUT:
		*pBuffer[0] = report_data->in_report;
		*plength = 1;
		break;

	case HID_REPORT_OUTPUT:
		return ERR_USBD_STALL;			/* Not Supported */

	case HID_REPORT_FEATURE:
		return ERR_USBD_STALL;			/* Not Supported */
	}
	return LPC_OK;
}

/* HID set report callback function. */
static ErrorCode_t HID_SetReport(USBD_HANDLE_T hHid, USB_SETUP_PACKET *pSetup, uint8_t * *pBuffer, uint16_t length)
{
	uint8_t blink_rate;

	/* we will reuse standard EP0Buf */
	if (length == 0) {
		return LPC_OK;
	}

	/* ReportID = SetupPacket.wValue.WB.L; */
	switch (pSetup->wValue.WB.H) {
	case HID_REPORT_INPUT:
		return ERR_USBD_STALL;			/* Not Supported */

	case HID_REPORT_OUTPUT:
		report_data->out_report = **pBuffer;
		board_led_set(LED5, report_data->out_report & 0x1);
		break;

	case HID_REPORT_FEATURE:
		blink_rate = **pBuffer;
		MCPWM_CH1_Update(blink_rate);
		break;
	}
	return LPC_OK;
}

/* HID Interrupt endpoint event handler. */
static ErrorCode_t HID_Ep_Hdlr(USBD_HANDLE_T hUsb, void *data, uint32_t event)
{
	USB_HID_CTRL_T *pHidCtrl = (USB_HID_CTRL_T *) data;

	switch (event) {
	case USB_EVT_IN:
		if (sw2_intr_report_pending > 0) {
			// Send 1 -> 0 transition.
			report_data->in_report = 0;
			USBD_API->hw->WriteEP(g_hUsb, HID_EP_IN, &report_data->in_report, 1);
			sw2_intr_report_pending = 0;
		}
		break;

	case USB_EVT_OUT_NAK:
		USBD_API->hw->ReadReqEP(hUsb, pHidCtrl->epout_adr, &report_data->out_report, 1);
		break;

	case USB_EVT_OUT:
		USBD_API->hw->ReadEP(hUsb, pHidCtrl->epout_adr, &report_data->out_report);
		board_led_set(LED5, report_data->out_report & 0x1);
		break;
	}
	return LPC_OK;
}

void GPIO0_IRQHandler(void) {
	Chip_PININT_ClearFallStates(LPC_GPIO_PIN_INT, PININTCH0);

	// Report only when device is configured and not suspended.
	if (is_device_active) {
		sw2_intr_report_pending++;

		// Send 0 -> 1 transition.
		report_data->in_report = (uint8_t) sw2_intr_report_pending;
		USBD_API->hw->WriteEP(g_hUsb, HID_EP_IN, &report_data->in_report, 1);
	}
}

/*****************************************************************************
 * Public functions
 ****************************************************************************/

static USB_HID_REPORT_T hid_reports_data[1];

/* HID init routine */
ErrorCode_t usb_hid_init(USBD_HANDLE_T hUsb,
						 USB_INTERFACE_DESCRIPTOR *pIntfDesc,
						 uint32_t *mem_base,
						 uint32_t *mem_size)
{
	USBD_HID_INIT_PARAM_T hid_param;

	ErrorCode_t ret = LPC_OK;

	memset((void *) &hid_param, 0, sizeof(USBD_HID_INIT_PARAM_T));
	/* HID paramas */
	hid_param.max_reports = 1;
	/* Init reports_data */
	hid_reports_data[0].len = HID_ReportDescSize;
	hid_reports_data[0].idle_time = 0;
	hid_reports_data[0].desc = (uint8_t *) &HID_ReportDescriptor[0];

	if ((pIntfDesc == 0) || (pIntfDesc->bInterfaceClass != USB_DEVICE_CLASS_HUMAN_INTERFACE)) {
		return ERR_FAILED;
	}

	hid_param.mem_base = *mem_base;
	hid_param.mem_size = *mem_size;
	hid_param.intf_desc = (uint8_t *) pIntfDesc;
	/* user defined functions */
	hid_param.HID_GetReport = HID_GetReport;
	hid_param.HID_SetReport = HID_SetReport;
	hid_param.HID_EpIn_Hdlr  = HID_Ep_Hdlr;
	hid_param.HID_EpOut_Hdlr = HID_Ep_Hdlr;
	hid_param.report_data  = hid_reports_data;

	ret = USBD_API->hid->init(hUsb, &hid_param);
	/* allocate USB accessable memory space for report data */
	report_data =  (report_data_t *) hid_param.mem_base;
	hid_param.mem_base += sizeof(report_data_t);

	/* update memory variables */
	*mem_base = hid_param.mem_base;
	*mem_size = hid_param.mem_size;

	g_hUsb = hUsb;
	return ret;
}

