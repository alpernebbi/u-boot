// SPDX-License-Identifier: GPL-2.0+

#include <common.h>
#include <dm.h>
#include <init.h>
#include <log.h>
#include <qfw.h>

int board_early_init_f(void)
{
	struct udevice *dev;
	int ret;

	/*
	 * Make sure we enumerate the QEMU Firmware device to bind ramfb
	 * so video_reserve() can reserve memory for it.
	 */
	if (IS_ENABLED(CONFIG_QFW)) {
		ret = qfw_get_dev(&dev);
		if (ret) {
			log_err("Failed to get QEMU FW device: %d\n", ret);
			return ret;
		}
	}

	return 0;
}

int board_early_init_r(void)
{
	struct udevice *dev;
	int ret;

	/*
	 * Make sure we enumerate the QEMU Firmware device to find ramfb
	 * before console_init.
	 */
	if (IS_ENABLED(CONFIG_QFW)) {
		ret = qfw_get_dev(&dev);
		if (ret) {
			log_err("Failed to get QEMU FW device: %d\n", ret);
			return ret;
		}
	}

	return 0;
}
