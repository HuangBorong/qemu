/*
 * Xiangshan Kunminghu SoC emulation
 *
 * Copyright (c) 2025 Beijing Institute of Open Source Chip (BOSC)
 *
 * Provides a board compatible with the Xiangshan Kunminghu FPGA platform:
 *
 * 0) UART
 * 1) CLINT (Core Level Interruptor)
 * 2) IMSIC (Incoming MSI Controller)
 * 3) APLIC (Advanced Platform-Level Interrupt Controller)
 * 4) Xilinx PCIe host controller
 * 4) 
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

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "exec/address-spaces.h"
#include "hw/boards.h"
#include "hw/char/serial-mm.h"
#include "hw/intc/riscv_aclint.h"
#include "hw/intc/riscv_aplic.h"
#include "hw/intc/riscv_imsic.h"
#include "hw/pci-host/xilinx-pcie.h"
#include "hw/qdev-properties.h"
#include "hw/riscv/boot.h"
#include "hw/riscv/xiangshan_kmh.h"
#include "hw/riscv/riscv_hart.h"
#include "system/system.h"

static const MemMapEntry xiangshan_kmh_memmap[] = {
    [XIANGSHAN_KMH_DEBUG] =        {        0x0,         0x100 },
    [XIANGSHAN_KMH_ROM] =          {     0x1000,        0xF000 },
    [XIANGSHAN_KMH_UART0] =        { 0x310B0000,       0x10000 },
    [XIANGSHAN_KMH_CLINT] =        { 0x38000000,       0x10000 },
    [XIANGSHAN_KMH_APLIC_M] =      { 0x31100000,        0x4000 },
    [XIANGSHAN_KMH_APLIC_S] =      { 0x31120000,        0x4000 },
    [XIANGSHAN_KMH_IMSIC_M] =      { 0x3A800000,       0x10000 },
    [XIANGSHAN_KMH_IMSIC_S] =      { 0x3B000000,       0x80000 },
    [XIANGSHAN_KMH_PCIE_MMIO] =    { 0x40000000,     0x8000000 },
    [XIANGSHAN_KMH_PCIE_CFG] =     { 0x48000000,     0x2000000 },
    [XIANGSHAN_KMH_DRAM] =         { 0x80000000,           0x0 },
};

static DeviceState *xiangshan_kmh_create_aia(const MemMapEntry *memmap,
											 uint32_t num_harts)
{
	int i;
	hwaddr addr = 0;
	DeviceState *aplic_s = NULL;
	DeviceState *aplic_m = NULL;

	/* M-level IMSICs */
	addr = memmap[XIANGSHAN_KMH_IMSIC_M].base;
	for (i = 0; i < num_harts; i++) {
		riscv_imsic_create(addr + i * IMSIC_HART_SIZE(0), i, true,
						   1, XIANGSHAN_KMH_IMSIC_NUM_IDS);
	}

	/* S-level IMSICs */
	addr = memmap[XIANGSHAN_KMH_IMSIC_S].base;
	for (i = 0; i < num_harts; i++) {
		riscv_imsic_create(addr +
						   i * IMSIC_HART_SIZE(XIANGSHAN_KMH_IMSIC_GUEST_BITS),
                    	   i, false, 1 + XIANGSHAN_KMH_IMSIC_GUEST_BITS,
				   		   XIANGSHAN_KMH_IMSIC_NUM_IDS);
	}

	/* M-level APLIC */
	aplic_m = riscv_aplic_create(memmap[XIANGSHAN_KMH_APLIC_M].base,
				     memmap[XIANGSHAN_KMH_APLIC_M].size,
				     0, 0, XIANGSHAN_KMH_APLIC_NUM_SOURCES,
				     0, true, true, NULL);

	/* S-level APLIC */
	aplic_s = riscv_aplic_create(memmap[XIANGSHAN_KMH_APLIC_S].base,
				     memmap[XIANGSHAN_KMH_APLIC_S].size,
				     0, 0, XIANGSHAN_KMH_APLIC_NUM_SOURCES,
				     0, true, false, aplic_m);

	return aplic_s;
}

static void xiangshan_kmh_machine_init(MachineState *machine)
{
	XiangshanKmhState *s = XIANGSHAN_KMH_MACHINE(machine);
	const MemMapEntry *memmap = xiangshan_kmh_memmap;
	MemoryRegion *system_memory = get_system_memory();
	hwaddr start_addr = memmap[XIANGSHAN_KMH_DRAM].base;

	/* Initialize SoC */
	object_initialize_child(OBJECT(machine), "soc", &s->soc,
							TYPE_XIANGSHAN_KMH_SOC);
	qdev_realize(DEVICE(&s->soc), NULL, &error_fatal);

	/* Register RAM */
	memory_region_add_subregion(system_memory,
								memmap[XIANGSHAN_KMH_DRAM].base,
								machine->ram);

	/* ROM reset vector */
	riscv_setup_rom_reset_vec(machine, &s->soc.cpus,
				  start_addr,
				  memmap[XIANGSHAN_KMH_ROM].base,
				  memmap[XIANGSHAN_KMH_ROM].size, 0, 0);
	if (machine->firmware) {
		riscv_load_firmware(machine->firmware, &start_addr, NULL);
	}

	/* Note: dtb has been integrated into firmware(OpenSBI) when compiling */
}

static void xiangshan_kmh_machine_class_init(ObjectClass *klass, void *data)
{
	MachineClass *mc = MACHINE_CLASS(klass);
	static const char *const valid_cpu_types[] = {
		TYPE_RISCV_CPU_XIANGSHAN_KMH,
		NULL
	};

	mc->desc = "RISC-V Board compatible with Xiangshan Kunminghu FPGA platform";
	mc->init = xiangshan_kmh_machine_init;
	mc->max_cpus = XIANGSHAN_KMH_MAX_CPUS;
	mc->default_cpu_type = TYPE_RISCV_CPU_XIANGSHAN_KMH;
	mc->valid_cpu_types = valid_cpu_types;
	mc->default_ram_id = "xiangshan.kunminghu.ram";
}

static const TypeInfo xiangshan_kmh_machine_info = {
	.name = TYPE_XIANGSHAN_KMH_MACHINE,
	.parent = TYPE_MACHINE,
	.instance_size = sizeof(XiangshanKmhState),
	.class_init = xiangshan_kmh_machine_class_init,
};

static void xiangshan_kmh_machine_register_types(void)
{
	type_register_static(&xiangshan_kmh_machine_info);
}

type_init(xiangshan_kmh_machine_register_types)

static inline XilinxPCIEHost *xilinx_pcie_init(MemoryRegion * sys_mem,
					       uint32_t bus_nr, hwaddr cfg_base,
					       uint64_t cfg_size,
					       hwaddr mmio_base,
					       uint64_t mmio_size, qemu_irq irq)
{
	DeviceState *dev;
	MemoryRegion *cfg, *mmio;

	dev = qdev_new(TYPE_XILINX_PCIE_HOST);

	qdev_prop_set_uint32(dev, "bus_nr", bus_nr);
	qdev_prop_set_uint64(dev, "cfg_base", cfg_base);
	qdev_prop_set_uint64(dev, "cfg_size", cfg_size);
	qdev_prop_set_uint64(dev, "mmio_base", mmio_base);
	qdev_prop_set_uint64(dev, "mmio_size", mmio_size);

	sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

	cfg = sysbus_mmio_get_region(SYS_BUS_DEVICE(dev), 0);
	memory_region_add_subregion_overlap(sys_mem, cfg_base, cfg, 0);

	mmio = sysbus_mmio_get_region(SYS_BUS_DEVICE(dev), 1);
	memory_region_add_subregion_overlap(sys_mem, 0, mmio, 0);

	qdev_connect_gpio_out_named(dev, "interrupt_out", 0, irq);

	return XILINX_PCIE_HOST(dev);
}

static void xiangshan_kmh_soc_realize(DeviceState *dev, Error **errp)
{
	MachineState *ms = MACHINE(qdev_get_machine());
	XiangshanKmhSoCState *s = XIANGSHAN_KMH_SOC(dev);
	const MemMapEntry *memmap = xiangshan_kmh_memmap;
	MemoryRegion *system_memory = get_system_memory();
	uint32_t num_harts = ms->smp.cpus;

	qdev_prop_set_uint32(DEVICE(&s->cpus), "num_harts", num_harts);
	qdev_prop_set_uint32(DEVICE(&s->cpus), "hartid_base", 0);
	qdev_prop_set_string(DEVICE(&s->cpus), "cpu_type",
		TYPE_RISCV_CPU_XIANGSHAN_KMH);
	sysbus_realize(SYS_BUS_DEVICE(&s->cpus), &error_fatal);

	/* AIA */
	s->irqchip = xiangshan_kmh_create_aia(memmap, num_harts);

	/* UART */
	serial_mm_init(system_memory, memmap[XIANGSHAN_KMH_UART0].base, 2,
		       qdev_get_gpio_in(s->irqchip, XIANGSHAN_KMH_UART0_IRQ),
		       115200, serial_hd(0), DEVICE_LITTLE_ENDIAN);

	/* CLINT */
	riscv_aclint_swi_create(memmap[XIANGSHAN_KMH_CLINT].base,
				0, num_harts, false);
	riscv_aclint_mtimer_create(memmap[XIANGSHAN_KMH_CLINT].base +
				   RISCV_ACLINT_SWI_SIZE,
				   RISCV_ACLINT_DEFAULT_MTIMER_SIZE, 
				   0, num_harts, RISCV_ACLINT_DEFAULT_MTIMECMP,
				   RISCV_ACLINT_DEFAULT_MTIME,
				   XIANGSHAN_KMH_CLINT_TIMEBASE_FREQ, true);

	/* PCIe */
	xilinx_pcie_init(system_memory, 0,
					 memmap[XIANGSHAN_KMH_PCIE_CFG].base,
					 memmap[XIANGSHAN_KMH_PCIE_CFG].size,
					 memmap[XIANGSHAN_KMH_PCIE_MMIO].base,
					 memmap[XIANGSHAN_KMH_PCIE_MMIO].size,
			 		 qdev_get_gpio_in(s->irqchip, XIANGSHAN_KMH_PCIE0_IRQ0));

	/* ROM */
	memory_region_init_rom(&s->rom, OBJECT(dev), "xiangshan.kunminghu.rom",
						   memmap[XIANGSHAN_KMH_ROM].size, &error_fatal);
	memory_region_add_subregion(system_memory,
								memmap[XIANGSHAN_KMH_ROM].base, &s->rom);
}

static void xiangshan_kmh_soc_class_init(ObjectClass *klass, void *data)
{
	DeviceClass *dc = DEVICE_CLASS(klass);

	dc->realize = xiangshan_kmh_soc_realize;
	dc->user_creatable = false;
}

static void xiangshan_kmh_soc_instance_init(Object * obj)
{
	XiangshanKmhSoCState *s = XIANGSHAN_KMH_SOC(obj);;

	object_initialize_child(obj, "cpus", &s->cpus, TYPE_RISCV_HART_ARRAY);
}

static const TypeInfo xiangshan_kmh_soc_info = {
	.name = TYPE_XIANGSHAN_KMH_SOC,
	.parent = TYPE_DEVICE,
	.instance_size = sizeof(XiangshanKmhSoCState),
	.instance_init = xiangshan_kmh_soc_instance_init,
	.class_init = xiangshan_kmh_soc_class_init,
};

static void xiangshan_kmh_soc_register_types(void)
{
	type_register_static(&xiangshan_kmh_soc_info);
}

type_init(xiangshan_kmh_soc_register_types)
