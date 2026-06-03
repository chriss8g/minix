/*
 * Architecture-dependent framebuffer driver for i386.
 *
 * Uses the VESA linear framebuffer set up by the bootloader (GRUB)
 * via the Multiboot protocol. The framebuffer physical address, pitch,
 * dimensions, and pixel format are read from the multiboot_info struct
 * preserved in kinfo.
 */

#include <minix/chardriver.h>
#include <minix/drivers.h>
#include <minix/fb.h>
#include <minix/type.h>
#include <minix/vm.h>
#include <minix/log.h>
#include <minix/syslib.h>
#include <minix/param.h>
#include <machine/multiboot.h>
#include <assert.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "fb.h"

/* globals */
static vir_bytes fb_vir;
static phys_bytes fb_phys;
static size_t fb_size;
static int initialized = 0;

static struct fb_fix_screeninfo i386_fbfs[FB_DEV_NR];
static struct fb_var_screeninfo i386_fbvs[FB_DEV_NR];

/* logging */
static struct log log = {
	.name = "fb",
	.log_level = LEVEL_INFO,
	.log_func = default_log
};

int
arch_get_device(int minor, struct device *dev)
{
	if (!initialized) return ENXIO;
	if (minor != 0) return ENXIO;
	dev->dv_base = fb_vir;
	dev->dv_size = fb_size;
	return OK;
}

int
arch_get_varscreeninfo(int minor, struct fb_var_screeninfo *fbvsp)
{
	if (!initialized) return ENXIO;
	if (minor != 0) return ENXIO;
	*fbvsp = i386_fbvs[minor];
	return OK;
}

int
arch_put_varscreeninfo(int minor, struct fb_var_screeninfo *fbvsp)
{
	if (!initialized) return ENXIO;
	if (minor != 0) return ENXIO;

	/* Only allow yoffset changes for pan/double-buffering */
	if (fbvsp->yoffset != i386_fbvs[minor].yoffset) {
		if (fbvsp->yoffset > i386_fbvs[minor].yres) {
			return EINVAL;
		}
		i386_fbvs[minor].yoffset = fbvsp->yoffset;
	}

	return OK;
}

int
arch_get_fixscreeninfo(int minor, struct fb_fix_screeninfo *fbfsp)
{
	if (!initialized) return ENXIO;
	if (minor != 0) return ENXIO;
	*fbfsp = i386_fbfs[minor];
	return OK;
}

int
arch_pan_display(int minor, struct fb_var_screeninfo *fbvsp)
{
	return arch_put_varscreeninfo(minor, fbvsp);
}

int
arch_fb_init(int minor, struct edid_info *info)
{
	struct kinfo kinfo;
	struct multiboot_info *mbi;
	struct minix_mem_range mr;
	uint32_t width, height, bpp, pitch;

	if (minor != 0) return ENXIO;

	if (initialized) {
		return OK;
	}

	/* Get kernel info to access multiboot framebuffer data */
	if (sys_getkinfo(&kinfo) != OK) {
		log_warn(&log, "Could not get kinfo\n");
		return ENXIO;
	}

	mbi = &kinfo.mbi;

	/* Check that the bootloader provided framebuffer info */
	if (!(mbi->mi_flags & MULTIBOOT_INFO_HAS_FRAMEBUFFER)) {
		log_warn(&log, "No framebuffer info from bootloader\n");
		log_warn(&log, "Ensure GRUB is configured with a video mode\n");
		return ENXIO;
	}

	if (mbi->framebuffer_type != MULTIBOOT_FRAMEBUFFER_TYPE_RGB) {
		log_warn(&log, "Framebuffer is not RGB (type=%u)\n",
		    mbi->framebuffer_type);
		return ENXIO;
	}

	fb_phys = (phys_bytes) mbi->framebuffer_addr;
	width = mbi->framebuffer_width;
	height = mbi->framebuffer_height;
	bpp = mbi->framebuffer_bpp;
	pitch = mbi->framebuffer_pitch;
	fb_size = (size_t) pitch * height;

	log_info(&log, "VESA framebuffer: %ux%u %ubpp at 0x%08x, pitch=%u\n",
	    width, height, bpp, (unsigned int) fb_phys, pitch);

	/* Request permission to access the framebuffer physical memory */
	mr.mr_base = fb_phys;
	mr.mr_limit = fb_phys + fb_size;
	if (sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr) != OK) {
		log_warn(&log, "Unable to request framebuffer memory access\n");
		return ENXIO;
	}

	/* Map the framebuffer into our address space */
	fb_vir = (vir_bytes) vm_map_phys(SELF, (void *) fb_phys, fb_size);
	if (fb_vir == (vir_bytes) MAP_FAILED) {
		log_warn(&log, "Unable to map framebuffer memory\n");
		return ENXIO;
	}

	/* Fill in fixed screen info */
	memset(&i386_fbfs[minor], 0, sizeof(i386_fbfs[minor]));
	i386_fbfs[minor].line_length = pitch;

	/* Fill in variable screen info */
	memset(&i386_fbvs[minor], 0, sizeof(i386_fbvs[minor]));
	i386_fbvs[minor].xres = width;
	i386_fbvs[minor].yres = height;
	i386_fbvs[minor].xres_virtual = width;
	i386_fbvs[minor].yres_virtual = height;
	i386_fbvs[minor].xoffset = 0;
	i386_fbvs[minor].yoffset = 0;
	i386_fbvs[minor].bits_per_pixel = bpp;

	/* RGB field positions from multiboot info */
	i386_fbvs[minor].red.offset = mbi->framebuffer_red_field_position;
	i386_fbvs[minor].red.length = mbi->framebuffer_red_mask_size;
	i386_fbvs[minor].red.msb_right = 0;

	i386_fbvs[minor].green.offset = mbi->framebuffer_green_field_position;
	i386_fbvs[minor].green.length = mbi->framebuffer_green_mask_size;
	i386_fbvs[minor].green.msb_right = 0;

	i386_fbvs[minor].blue.offset = mbi->framebuffer_blue_field_position;
	i386_fbvs[minor].blue.length = mbi->framebuffer_blue_mask_size;
	i386_fbvs[minor].blue.msb_right = 0;

	/* Transparency - assume remaining bits if 32bpp */
	if (bpp == 32) {
		/* Typically offset=24, length=8 for ARGB */
		uint8_t used = mbi->framebuffer_red_mask_size
		    + mbi->framebuffer_green_mask_size
		    + mbi->framebuffer_blue_mask_size;
		if (used < 32) {
			/* Find the unused offset */
			uint8_t max_off = 0;
			if (mbi->framebuffer_red_field_position +
			    mbi->framebuffer_red_mask_size > max_off)
				max_off = mbi->framebuffer_red_field_position +
				    mbi->framebuffer_red_mask_size;
			if (mbi->framebuffer_green_field_position +
			    mbi->framebuffer_green_mask_size > max_off)
				max_off = mbi->framebuffer_green_field_position +
				    mbi->framebuffer_green_mask_size;
			if (mbi->framebuffer_blue_field_position +
			    mbi->framebuffer_blue_mask_size > max_off)
				max_off = mbi->framebuffer_blue_field_position +
				    mbi->framebuffer_blue_mask_size;

			i386_fbvs[minor].transp.offset = max_off;
			i386_fbvs[minor].transp.length = 32 - used;
			i386_fbvs[minor].transp.msb_right = 0;
		}
	}

	initialized = 1;
	log_info(&log, "Framebuffer initialized successfully\n");

	return OK;
}
