/*
 * Xiangshan Kunminghu SoC emulation
 *
 * Copyright (c) 2025 Beijing Institute of Open Source Chip (BOSC)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HW_XIANGSHAN_KMH_H
#define HW_XIANGSHAN_KMH_H

#include "hw/boards.h"
#include "hw/pci-host/xilinx-pcie.h"
#include "hw/riscv/riscv_hart.h"

#define XIANGSHAN_KMH_MAX_CPUS       16

typedef struct XiangshanKmhSoCState {
	/*< private > */
	DeviceState parent_obj;

	/*< public > */
	RISCVHartArrayState cpus;
	DeviceState *irqchip;
	XilinxPCIEHost pcie;
	MemoryRegion rom;
} XiangshanKmhSoCState;

#define TYPE_XIANGSHAN_KMH_SOC "xiangshan.kunminghu.soc"
DECLARE_INSTANCE_CHECKER(XiangshanKmhSoCState, XIANGSHAN_KMH_SOC,
	TYPE_XIANGSHAN_KMH_SOC)
			 
typedef struct XiangshanKmhState {
	/*< private > */
	MachineState parent_obj;

	/*< public > */
	XiangshanKmhSoCState soc;
} XiangshanKmhState;

#define TYPE_XIANGSHAN_KMH_MACHINE MACHINE_TYPE_NAME("xiangshan-kunminghu")
DECLARE_INSTANCE_CHECKER(XiangshanKmhState, XIANGSHAN_KMH_MACHINE,
	TYPE_XIANGSHAN_KMH_MACHINE)

enum {
	XIANGSHAN_KMH_DEBUG,
	XIANGSHAN_KMH_ROM,
	XIANGSHAN_KMH_UART0,
	XIANGSHAN_KMH_CLINT,
	XIANGSHAN_KMH_APLIC_M,
	XIANGSHAN_KMH_APLIC_S,
	XIANGSHAN_KMH_IMSIC_M,
	XIANGSHAN_KMH_IMSIC_S,
	XIANGSHAN_KMH_PCIE_MMIO,
	XIANGSHAN_KMH_PCIE_CFG,
	XIANGSHAN_KMH_DRAM,
};

enum {
	XIANGSHAN_KMH_UART0_IRQ = 10,
	XIANGSHAN_KMH_PCIE0_IRQ0 = 51,
	XIANGSHAN_KMH_PCIE0_IRQ1 = 52,
	XIANGSHAN_KMH_PCIE0_IRQ2 = 53,
};

/* Indicating Timebase-freq (1MHZ) */
#define XIANGSHAN_KMH_CLINT_TIMEBASE_FREQ 1000000

#define XIANGSHAN_KMH_IMSIC_NUM_IDS 255
#define XIANGSHAN_KMH_IMSIC_NUM_GUESTS 7
#define XIANGSHAN_KMH_IMSIC_GUEST_BITS 3

#define XIANGSHAN_KMH_APLIC_NUM_SOURCES 96

#endif
