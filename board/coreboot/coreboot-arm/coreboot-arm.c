// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2017 Tuomas Tynkkynen
 */

#include <cpu_func.h>
#include <cb_sysinfo.h>
#include <dm.h>
#include <efi.h>
#include <efi_loader.h>
#include <fdtdec.h>
#include <init.h>
#include <log.h>
#include <usb.h>
#include <virtio_types.h>
#include <virtio.h>

#include <linux/kernel.h>
#include <linux/sizes.h>

#ifdef CONFIG_ARM64
#include <asm/armv8/mmu.h>

#define MAX_MEM_MAP_REGIONS 16
static struct mm_region coreboot_mem_map[MAX_MEM_MAP_REGIONS] __section(".data") = {0};
struct mm_region *mem_map = coreboot_mem_map;
#endif

int arch_cpu_init(void)
{
	int ret = 0;

	ret = get_coreboot_info(&lib_sysinfo);
	if (ret != 0) {
		log_err("Failed to parse coreboot tables.\n");
		return ret;
	}

	debug("sysinfo version:         %s\n", lib_sysinfo.version);
	debug("sysinfo extra_version:   %s\n", lib_sysinfo.extra_version);
	debug("sysinfo build:           %s\n", lib_sysinfo.build);
	debug("sysinfo compile_time:    %s\n", lib_sysinfo.compile_time);
	debug("sysinfo compile_by:      %s\n", lib_sysinfo.compile_by);
	debug("sysinfo compile_host:    %s\n", lib_sysinfo.compile_host);
	debug("sysinfo compile_domain:  %s\n", lib_sysinfo.compile_domain);
	debug("sysinfo compiler:        %s\n", lib_sysinfo.compiler);
	debug("sysinfo linker:          %s\n", lib_sysinfo.linker);
	debug("sysinfo assembler:       %s\n", lib_sysinfo.assembler);
	debug("sysinfo cb_version:      %s\n", lib_sysinfo.cb_version);

	debug("sysinfo framebuffer:     %p\n", lib_sysinfo.framebuffer);
	debug("sysinfo framebuffer.tag:                %x\n",  lib_sysinfo.framebuffer->tag);
	debug("sysinfo framebuffer.size:               %x\n",  lib_sysinfo.framebuffer->size);
	debug("sysinfo framebuffer.physical_address:   %llx\n", lib_sysinfo.framebuffer->physical_address);
	debug("sysinfo framebuffer.x_resolution:       %d\n",  lib_sysinfo.framebuffer->x_resolution);
	debug("sysinfo framebuffer.y_resolution:       %d\n",  lib_sysinfo.framebuffer->y_resolution);
	debug("sysinfo framebuffer.bytes_per_line:     %x\n",  lib_sysinfo.framebuffer->bytes_per_line);
	debug("sysinfo framebuffer.bits_per_pixel:     %d\n",  lib_sysinfo.framebuffer->bits_per_pixel);
	debug("sysinfo framebuffer.red_mask_pos:       %x\n",  lib_sysinfo.framebuffer->red_mask_pos);
	debug("sysinfo framebuffer.red_mask_size:      %x\n",  lib_sysinfo.framebuffer->red_mask_size);
	debug("sysinfo framebuffer.green_mask_pos:     %x\n",  lib_sysinfo.framebuffer->green_mask_pos);
	debug("sysinfo framebuffer.green_mask_size:    %x\n",  lib_sysinfo.framebuffer->green_mask_size);
	debug("sysinfo framebuffer.blue_mask_pos:      %x\n",  lib_sysinfo.framebuffer->blue_mask_pos);
	debug("sysinfo framebuffer.blue_mask_size:     %x\n",  lib_sysinfo.framebuffer->blue_mask_size);
	debug("sysinfo framebuffer.reserved_mask_pos:  %x\n",  lib_sysinfo.framebuffer->reserved_mask_pos);
	debug("sysinfo framebuffer.reserved_mask_size: %x\n",  lib_sysinfo.framebuffer->reserved_mask_size);

	return 0;
}

int board_init(void)
{
	return 0;
}

int board_late_init(void)
{
	/*
	 * Make sure virtio bus is enumerated so that peripherals
	 * on the virtio bus can be discovered by their drivers
	 */
	virtio_init();

	/* start usb so that usb keyboard can be used as input device */
	if (CONFIG_IS_ENABLED(USB_KEYBOARD))
		usb_init();

	return 0;
}

/*
 * This function looks for the highest region of memory lower than 4GB which
 * has enough space for U-Boot where U-Boot is aligned on a page boundary. It
 * overrides the default implementation found elsewhere which simply picks the
 * end of ram, wherever that may be. The location of the stack, the relocation
 * address, and how far U-Boot is moved by relocation are set in the global
 * data structure.
 */
phys_addr_t board_get_usable_ram_top(phys_size_t total_size)
{
	uintptr_t dest_addr = 0;
	int i;

	for (i = 0; i < lib_sysinfo.n_memranges; i++) {
		struct memrange *memrange = &lib_sysinfo.memrange[i];
		/* Force U-Boot to relocate to a page aligned address. */
		uint64_t start = roundup(memrange->base, 1 << 12);
		uint64_t end = memrange->base + memrange->size;

		/* Ignore non-memory regions. */
		if (memrange->type != CB_MEM_RAM)
			continue;

		/* Filter memory over 4GB. */
		if (end > 0xffffffffULL)
			end = 0x100000000ULL;
		/* Skip this region if it's too small. */
		if (end - start < total_size)
			continue;

		/* Use this address if it's the largest so far. */
		if (end > dest_addr)
			dest_addr = end;
	}

	/* If no suitable area was found, return an error. */
	if (!dest_addr)
		panic("No available memory found for relocation");

	return (ulong)dest_addr;
}

int dram_init(void)
{
	if (fdtdec_setup_mem_size_base() != 0)
		return -EINVAL;

	const struct sysinfo_t *sysinfo = cb_get_sysinfo();
	if (!sysinfo) {
		log_err("Failed to get sysinfo struct.\n");
		return 0;
	}

	const struct memrange *range;
	for (int i = 0; i < sysinfo->n_memranges; i++) {
		range = &sysinfo->memrange[i];
		debug("memrange[%d]->base = %#llx\n", i, range->base);
		debug("memrange[%d]->size = %#llx\n", i, range->size);
		debug("memrange[%d]->type = %#x\n",   i, range->type);
	}

	struct mm_region *region;
	int i = 0;
	for (i = 0; i < sysinfo->n_memranges; i++) {
		range = &sysinfo->memrange[i];
		region = &coreboot_mem_map[i];

		region->virt = range->base;
		region->phys = range->base;
		region->size = range->size;

		switch (range->type) {
		case CB_MEM_RAM:
			region->attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
					PTE_BLOCK_INNER_SHARE;
			break;
		default:
			region->attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
					PTE_BLOCK_NON_SHARE |
					PTE_BLOCK_PXN | PTE_BLOCK_UXN;
		}
	}

	for (int i = 0; i < MAX_MEM_MAP_REGIONS; i++) {
		region = &mem_map[i];
		if (!region->virt && !region->attrs)
			break;
		debug("mem_map[%d]->virt  = %#llx\n", i, region->virt);
		debug("mem_map[%d]->phys  = %#llx\n", i, region->phys);
		debug("mem_map[%d]->size  = %#llx\n", i, region->size);
		debug("mem_map[%d]->attrs = %#llx\n", i, region->attrs);
	}

	return 0;
}

int dram_init_banksize(void)
{
	fdtdec_setup_memory_banksize();

	return 0;
}

void *board_fdt_blob_setup(int *err)
{
	*err = 0;
	/* QEMU loads a generated DTB for us at the start of RAM. */
	return (void *)CFG_SYS_SDRAM_BASE;
}

long detect_coreboot_table_at(ulong start, ulong size)
{
	u32 *ptr, *end;

	size /= 4;
	for (ptr = (void *)start, end = ptr + size; ptr < end; ptr += 4) {
		if (*ptr == 0x4f49424c) /* "LBIO" */
			return (long)ptr;
	}

	return -ENOENT;
}

long locate_coreboot_table(void)
{
	long addr = 0;

	/* TODO: get from device-tree */
	for (int i = 10; i < 64; i++) {
		addr = 0x8000000 * i - 0x24000;
		debug("Trying coreboot table at %#lx.\n", addr);
		addr = detect_coreboot_table_at(addr, 0xc00);
		if (addr > 0) {
			debug("Found coreboot table at %#lx.\n", addr);
			break;
		}
	}

	return addr;
}
