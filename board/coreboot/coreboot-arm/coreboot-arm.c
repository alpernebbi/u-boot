// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2017 Tuomas Tynkkynen
 */

#define LOG_DEBUG 1
#define DEBUG 1

#include <cpu_func.h>
#include <cb_sysinfo.h>
#include <debug_uart.h>
#include <dm.h>
#include <efi.h>
#include <efi_loader.h>
#include <fdtdec.h>
#include <init.h>
#include <log.h>
#include <mapmem.h>
#include <usb.h>
#include <virtio_types.h>
#include <virtio.h>

#include <linux/kernel.h>
#include <linux/sizes.h>
#include <linux/delay.h>

#ifdef CONFIG_ARM64
#include <asm/armv8/mmu.h>

#define MAX_MEM_MAP_REGIONS 16
/* static struct mm_region coreboot_mem_map[MAX_MEM_MAP_REGIONS] __section(".data") = { */
/* 	{ */
/* 		/1* Map 4GiB as device memory *1/ */
/* 		.virt = 0, */
/* 		.phys = 0, */
/* 		.size = 0x100000000UL, */
/* 		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) | */
/* 			 PTE_BLOCK_NON_SHARE | */
/* 			 PTE_BLOCK_PXN | PTE_BLOCK_UXN */
/* 	}, { */
/* 		/1* List terminator *1/ */
/* 		0, */
/* 	} */
/* }; */
static struct mm_region coreboot_mem_map[MAX_MEM_MAP_REGIONS] __section(".data") = {0};
struct mm_region *mem_map = coreboot_mem_map;
#endif

#if false
void step(bool ut)
{
	void* fb = (void *)(uintptr_t)0x10200000;
	static int l = 1;
	int h = 10;
	int w = 2400;

	memset(fb + 4 * w * h * l,
	       ut ? 0xff : 0x7f,
	       4 * w * h);
	l += 2;

	if (l > 1600 / h)
		l = 0;
}
#endif

int arch_cpu_init(void)
{
	log_err("arch_cpu_init().\n");
	int ret = 0;

	debug_uart_init();

	log_err("Sleeping for 10s\n");
	mdelay(10000);

	uintptr_t addr = locate_coreboot_table();
	if (!addr) {
		log_err("no coreboot table found\n");
		return -1;
	}

	log_err("coreboot table @ %#lx\n", addr);

#if false
	log_err("dumping coreboot table:\n");
	uint8_t *mem = (uint8_t *)(phys_addr_t)addr;
	for (int i = 0; i < 0x3000; i++) {
		if (i % 0x10 == 0) {
			mdelay(5);
			printascii("\n");
			printhex8((phys_addr_t)&mem[i]);
			printascii(":  ");
		}
		printhex2(mem[i]);
		if (i % 0x10 == 0x8)
			printascii("  ");
		else
			printascii(" ");
	}
	printascii("\n");
	mdelay(500);
#endif

	ret = get_coreboot_info(&lib_sysinfo);

	if (ret != 0) {
		log_err("Failed to parse coreboot tables.\n");
		return ret;
	}

	return 0;
}

int board_init(void)
{
	log_err("board_init().\n");
	return 0;
}

int board_late_init(void)
{
	log_err("board_late_init().\n");
	/*
	 * Make sure virtio bus is enumerated so that peripherals
	 * on the virtio bus can be discovered by their drivers
	 */
	/* if (CONFIG_IS_ENABLED(VIRTIO_MMIO)) */
	/* 	virtio_init(); */

	/* start usb so that usb keyboard can be used as input device */
	/* if (CONFIG_IS_ENABLED(USB_KEYBOARD)) */
	/* 	usb_init(); */

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

	log_err("board_get_usable_ram_top().\n");
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

	log_err("board_get_usable_ram_top() returns %#lx\n", (ulong)dest_addr);
	return (ulong)dest_addr;
}

int dram_init(void)
{
	log_err("dram_init().\n");
	/* if (fdtdec_setup_mem_size_base() != 0) */
	/* 	return -EINVAL; */

	const struct sysinfo_t *sysinfo = cb_get_sysinfo();
	if (!sysinfo) {
		log_err("Failed to get sysinfo struct.\n");
		return 0;
	}

	log_err("sysinfo version:         %s\n", sysinfo->version);
	log_err("sysinfo extra_version:   %s\n", sysinfo->extra_version);
	log_err("sysinfo build:           %s\n", sysinfo->build);
	log_err("sysinfo compile_time:    %s\n", sysinfo->compile_time);
	log_err("sysinfo compile_by:      %s\n", sysinfo->compile_by);
	log_err("sysinfo compile_host:    %s\n", sysinfo->compile_host);
	log_err("sysinfo compile_domain:  %s\n", sysinfo->compile_domain);
	log_err("sysinfo compiler:        %s\n", sysinfo->compiler);
	log_err("sysinfo linker:          %s\n", sysinfo->linker);
	log_err("sysinfo assembler:       %s\n", sysinfo->assembler);
	log_err("sysinfo cb_version:      %s\n", sysinfo->cb_version);

	log_err("sysinfo framebuffer:     %p\n", sysinfo->framebuffer);
	log_err("sysinfo framebuffer.tag:                %x\n",  sysinfo->framebuffer->tag);
	log_err("sysinfo framebuffer.size:               %x\n",  sysinfo->framebuffer->size);
	log_err("sysinfo framebuffer.physical_address:   %llx\n", sysinfo->framebuffer->physical_address);
	log_err("sysinfo framebuffer.x_resolution:       %d\n",  sysinfo->framebuffer->x_resolution);
	log_err("sysinfo framebuffer.y_resolution:       %d\n",  sysinfo->framebuffer->y_resolution);
	log_err("sysinfo framebuffer.bytes_per_line:     %x\n",  sysinfo->framebuffer->bytes_per_line);
	log_err("sysinfo framebuffer.bits_per_pixel:     %d\n",  sysinfo->framebuffer->bits_per_pixel);
	log_err("sysinfo framebuffer.red_mask_pos:       %x\n",  sysinfo->framebuffer->red_mask_pos);
	log_err("sysinfo framebuffer.red_mask_size:      %x\n",  sysinfo->framebuffer->red_mask_size);
	log_err("sysinfo framebuffer.green_mask_pos:     %x\n",  sysinfo->framebuffer->green_mask_pos);
	log_err("sysinfo framebuffer.green_mask_size:    %x\n",  sysinfo->framebuffer->green_mask_size);
	log_err("sysinfo framebuffer.blue_mask_pos:      %x\n",  sysinfo->framebuffer->blue_mask_pos);
	log_err("sysinfo framebuffer.blue_mask_size:     %x\n",  sysinfo->framebuffer->blue_mask_size);
	log_err("sysinfo framebuffer.reserved_mask_pos:  %x\n",  sysinfo->framebuffer->reserved_mask_pos);
	log_err("sysinfo framebuffer.reserved_mask_size: %x\n",  sysinfo->framebuffer->reserved_mask_size);

	const struct memrange *range;
	for (int i = 0; i < sysinfo->n_memranges; i++) {
		range = &sysinfo->memrange[i];
		log_err("memrange[%d]->base = %#llx\n", i, range->base);
		log_err("memrange[%d]->size = %#llx\n", i, range->size);
		log_err("memrange[%d]->type = %#x\n",   i, range->type);
	}

	struct mm_region *region;
	int i = 0;
	int j = 0;
	for (i = 0; i < sysinfo->n_memranges; i++) {
		range = &sysinfo->memrange[i];
		region = &coreboot_mem_map[j];
		u64 attrs = 0;

		switch (range->type) {
		case CB_MEM_RAM:
		case CB_MEM_TABLE:
			attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
					PTE_BLOCK_INNER_SHARE;
			break;
		case CB_MEM_MMIO:
			attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
					PTE_BLOCK_NON_SHARE |
					PTE_BLOCK_PXN | PTE_BLOCK_UXN;
			break;
		default:
			continue;
		}

		region->virt = range->base;
		region->phys = range->base;
		region->size = range->size;
		region->attrs = attrs;
		j++;

	}

	for (int i = 0; i < MAX_MEM_MAP_REGIONS; i++) {
		region = &mem_map[i];
		if (!region->virt && !region->attrs)
			break;
		log_err("mem_map[%d]->virt  = %#llx\n", i, region->virt);
		log_err("mem_map[%d]->phys  = %#llx\n", i, region->phys);
		log_err("mem_map[%d]->size  = %#llx\n", i, region->size);
		log_err("mem_map[%d]->attrs = %#llx\n", i, region->attrs);
	}

	phys_size_t ram_size = 0;
	for (i = 0; i < lib_sysinfo.n_memranges; i++) {
		struct memrange *memrange = &lib_sysinfo.memrange[i];
		unsigned long long end = memrange->base + memrange->size;

		if (memrange->type == CB_MEM_RAM && end > ram_size)
			ram_size += memrange->size;
	}

	gd->ram_size = ram_size;
	if (ram_size == 0)
		return -1;

	log_err("dram_init() done.\n");
	return 0;
}

int dram_init_banksize(void)
{
	log_err("dram_init_banksize().\n");
	int i, j;

	if (CONFIG_NR_DRAM_BANKS) {
		for (i = 0, j = 0; i < lib_sysinfo.n_memranges; i++) {
			struct memrange *memrange = &lib_sysinfo.memrange[i];

			if (memrange->type == CB_MEM_RAM) {
				gd->bd->bi_dram[j].start = memrange->base;
				gd->bd->bi_dram[j].size = memrange->size;
				j++;
				if (j >= CONFIG_NR_DRAM_BANKS)
					break;
			}
		}
	}

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
	long addr = 0xf7fd0000;

	log_err("Trying coreboot table at %lx.\n", addr);
	addr = detect_coreboot_table_at(addr, 0x25000);
	if (addr > 0) {
		log_err("Found coreboot table at %#lx.\n", addr);
		return addr;
	}

	return 0;

	/* TODO: get from device-tree */
	for (int i = 10; i < 64; i++) {
		addr = 0x8000000 * i - 0x24000;
		log_err("Trying coreboot table at %#lx.\n", addr);
		addr = detect_coreboot_table_at(addr, 0xc00);
		if (addr > 0) {
			log_err("Found coreboot table at %#lx.\n", addr);
			break;
		}
	}

	return addr;
}

