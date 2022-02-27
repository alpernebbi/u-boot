// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2022 Alexander Graf
 */

#include <common.h>
#include <dm.h>
#include <log.h>
#include <video.h>
#include <asm/global_data.h>
#include <qfw.h>

#define fourcc_code(a, b, c, d) ((u32)(a) | ((u32)(b) << 8) | \
				 ((u32)(c) << 16) | ((u32)(d) << 24))
#define DRM_FORMAT_XRGB8888 fourcc_code('X', 'R', '2', '4')

#define RAMFB_WIDTH	CONFIG_VIDEO_RAMFB_SIZE_X
#define RAMFB_HEIGHT	CONFIG_VIDEO_RAMFB_SIZE_Y
#define RAMFB_BPIX	VIDEO_BPP32
#define RAMFB_FORMAT	VIDEO_X8R8G8B8

static int ramfb_probe(struct udevice *dev)
{
	struct video_uc_plat *plat = dev_get_uclass_plat(dev);
	struct video_priv *uc_priv = dev_get_uclass_priv(dev);
	u32 selector;
	int ret;
	struct fw_file *file;
	struct udevice *qfw;
	struct dm_qfw_ops *ops;
	struct qfw_dma dma = {};
	struct ramfb_cfg cfg = {
		.addr = cpu_to_be64(plat->base),
		.fourcc = cpu_to_be32(DRM_FORMAT_XRGB8888),
		.flags = 0,
		.width = cpu_to_be32(RAMFB_WIDTH),
		.height = cpu_to_be32(RAMFB_HEIGHT),
		.stride = 0,
	};

	if (plat->base == 0)
		return -EPROBE_DEFER;

	debug("%s: Frame buffer base %lx\n", __func__, plat->base);

	ret = qfw_get_dev(&qfw);
	if (ret)
		return -EPROBE_DEFER;

	ops = dm_qfw_get_ops(qfw);
	if (!ops)
		return -EPROBE_DEFER;

	file = qfw_find_file(qfw, "etc/ramfb");
	if (!file) {
		/* No ramfb available. At least we tried. */
		return -ENOENT;
	}

	uc_priv->xsize = RAMFB_WIDTH;
	uc_priv->ysize = RAMFB_HEIGHT;
	uc_priv->bpix = RAMFB_BPIX;
	uc_priv->format = RAMFB_FORMAT;
	uc_priv->fb = (void *)plat->base;
	uc_priv->fb_size = plat->size;

	selector = be16_to_cpu(file->cfg.select);
	dma.length = cpu_to_be32(sizeof(cfg));
	dma.address = cpu_to_be64((uintptr_t)&cfg);
	dma.control = cpu_to_be32(FW_CFG_DMA_WRITE | FW_CFG_DMA_SELECT |
				  (selector << 16));

	barrier();

	/* Send a DMA write request which enables the screen */
	ops->read_entry_dma(qfw, &dma);

	return 0;
}

static int ramfb_bind(struct udevice *dev)
{
	struct video_uc_plat *uc_plat = dev_get_uclass_plat(dev);

	/* Set the maximum supported resolution */
	uc_plat->size = RAMFB_WIDTH * RAMFB_HEIGHT * VNBYTES(RAMFB_BPIX);
	debug("%s: Frame buffer size %x\n", __func__, uc_plat->size);

	return 0;
}

U_BOOT_DRIVER(ramfb) = {
	.name	= "ramfb",
	.id	= UCLASS_VIDEO,
	.probe	= ramfb_probe,
	.bind   = ramfb_bind,
};
