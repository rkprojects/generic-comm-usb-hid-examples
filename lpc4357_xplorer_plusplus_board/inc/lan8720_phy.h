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


#ifndef LAN8720_PHY_H_
#define LAN8720_PHY_H_

/* Registers */
#define PHY_REG_BMCR        0x00        /* Basic Mode Control Register       */
#define PHY_REG_BMSR        0x01        /* Basic Mode Status Register        */
#define PHY_REG_IDR1        0x02        /* PHY Identifier 1                  */
#define PHY_REG_IDR2        0x03        /* PHY Identifier 2                  */
#define PHY_REG_ANAR        0x04        /* Auto-Negotiation Advertisement    */
#define PHY_REG_ANLPAR      0x05        /* Auto-Neg. Link Partner Abitily    */
#define PHY_REG_ANER        0x06        /* Auto-Neg. Expansion Register      */
#define PHY_REG_ANNPTR      0x07        /* Auto-Neg. Next Page TX            */

/* Speed identify */
#define PHY_REG_SPCON    				0x1f   /* Speed indication Register     */
#define PHY_REG_HCDSPEED_MASK    		0x1c   /* Speed indication Register mask*/
#define PHY_REG_HCDSPEED_10MB_HALFD    	0x04   /* Speed is 10Mbps HALF-duplex   */
#define PHY_REG_HCDSPEED_10MB_FULLD    	0x14   /* Speed is 10Mbps FULL-duplex   */
#define PHY_REG_HCDSPEED_100MB_HALFD    0x08   /* Speed is 100Mbps HALF-duplex  */
#define PHY_REG_HCDSPEED_100MB_FULLD    0x18   /* Speed is 100Mbps FULL-duplex  */


/* Extended Registers */
#define PHY_REG_STS         0x10        /* Status Register                   */
#define PHY_REG_MICR        0x11        /* MII Interrupt Control Register    */
#define PHY_REG_MISR        0x12        /* MII Interrupt Status Register     */
#define PHY_REG_FCSCR       0x14        /* False Carrier Sense Counter       */
#define PHY_REG_RECR        0x15        /* Receive Error Counter             */
#define PHY_REG_PCSR        0x16        /* PCS Sublayer Config. and Status   */
#define PHY_REG_RBR         0x17        /* RMII and Bypass Register          */
#define PHY_REG_LEDCR       0x18        /* LED Direct Control Register       */
#define PHY_REG_PHYCR       0x19        /* PHY Control Register              */
#define PHY_REG_10BTSCR     0x1A        /* 10Base-T Status/Control Register  */
#define PHY_REG_CDCTRL1     0x1B        /* CD Test Control and BIST Extens.  */
#define PHY_REG_EDCR        0x1D        /* Energy Detect Control Register    */

/* Control and Status bits  */
#define PHY_FULLD_100M      0x2100      /* Full Duplex 100Mbit               */
#define PHY_HALFD_100M      0x2000      /* Half Duplex 100Mbit               */
#define PHY_FULLD_10M       0x0100      /* Full Duplex 10Mbit                */
#define PHY_HALFD_10M       0x0000      /* Half Duplex 10MBit                */
#define PHY_AUTO_NEG        0x1000      /* Select Auto Negotiation           */
#define PHY_AUTO_NEG_DONE   0x0020		/* AutoNegotiation Complete in BMSR PHY reg  */
#define PHY_BMCR_RESET		0x8000		/* Reset bit at BMCR PHY reg         */
#define LINK_VALID_STS		0x0004		/* Link Valid Status at PHY_REG_BMSR PHY reg	 */
#define FULL_DUP_STS		0x0004		/* Full Duplex Status at REG_STS PHY reg */
#define SPEED_10M_STS		0x0002		/* 10Mbps Status at REG_STS PHY reg */

#define PHY_LINK_CONNECTED LINK_VALID_STS //Used in FreeRTOS+TCP NetworkInterface Port.

#define LAN8720_ID			0x0007C0F1  /* PHY Identifier for SMSC PHY       */

#endif /* LAN8720_PHY_H_ */
