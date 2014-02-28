/*
 * XpressRICH3-AXI PCIe Host Bridge Driver.
 *
 * Copyright (C) 2012 ARM Ltd.
 * Author: Andrew Murray <andrew.murray@arm.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#ifndef __XPRESS_RICH3_H__
#define __XPRESS_RICH3_H__

/* Host Bridge Identification */
#define DEVICE_NAME "XpressRICH3-AXI PCIe Host Bridge"
#define DEVICE_VENDOR_ID  0x1556
#define DEVICE_DEVICE_ID  0x1100

/* Host Bridge Internal Registers */
#define XR3PCI_BASIC_STATUS		0x18
#define    XR3PCI_BS_LINK_MASK		0xff
#define    XR3PCI_BS_GEN_MASK		(0xf << 8)
#define    XR3PCI_BS_NEG_PAYLOAD_MASK	(0xf << 24)
#define    XR3PCI_BS_NEG_REQSIZE_MASK   (0xf << 28)

#define XR3PCI_LOCAL_INT_MASK		0x180
#define XR3PCI_LOCAL_INT_STATUS		0x184
#define XR3PCI_MSI_INT_STATUS		0x194

#define XR3PCI_INT_A			(1 << 24)
#define XR3PCI_INT_B			(1 << 25)
#define XR3PCI_INT_C			(1 << 26)
#define XR3PCI_INT_D			(1 << 27)
#define XR3PCI_INT_INTx			(XR3PCI_INT_A | XR3PCI_INT_B | \
					XR3PCI_INT_C | XR3PCI_INT_D)
#define XR3PCI_INT_MSI			(1 << 28)


#define XR3PCI_VIRTCHAN_CREDITS		0x90
#define XR3PCI_PEX_SPC2			0xd8

/* Address Translation Register */
#define XR3PCI_ATR_PCIE_WIN0		0x600
#define XR3PCI_ATR_PCIE_WIN1		0x700
#define XR3PCI_ATR_AXI4_SLV0		0x800

#define XR3PCI_ATR_TABLE_SIZE		0x20
#define XR3PCI_ATR_SRC_ADDR_LOW		0x0
#define XR3PCI_ATR_SRC_ADDR_HIGH	0x4
#define XR3PCI_ATR_TRSL_ADDR_LOW	0x8
#define XR3PCI_ATR_TRSL_ADDR_HIGH	0xc
#define XR3PCI_ATR_TRSL_PARAM		0x10
/* IDs used in the XR3PCI_ATR_TRSL_PARAM */
#define XR3PCI_ATR_TRSLID_AXIDEVICE	(0x420004)
#define XR3PCI_ATR_TRSLID_AXIMEMORY	(0x4e0004)  /* Write-through, read/write allocate */
#define XR3PCI_ATR_TRSLID_PCIE_CONF	(0x000001)
#define XR3PCI_ATR_TRSLID_PCIE_IO	(0x020000)
#define XR3PCI_ATR_TRSLID_PCIE_MEMORY	(0x000000)

#define XR3PCI_ECAM_OFFSET(b, d, o)	(((b) << 20) | \
					(PCI_SLOT(d) << 15) | \
					(PCI_FUNC(d) << 12) | o)


#define JUNO_RESET_CTRL			0x1004
#define JUNO_RESET_CTRL_PHY		(1 << 0)
#define JUNO_RESET_CTRL_RC		(1 << 1)

#define JUNO_RESET_STATUS		0x1008
#define JUNO_RESET_STATUS_PLL		(1 << 0)
#define JUNO_RESET_STATUS_PHY		(1 << 1)
#define JUNO_RESET_STATUS_RC		(1 << 2)
#define JUNO_RESET_STATUS_MASK		(JUNO_RESET_STATUS_PLL | \
					 JUNO_RESET_STATUS_PHY | \
					 JUNO_RESET_STATUS_RC)

#endif
