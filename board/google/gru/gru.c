// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2018 Google
 */

#include <common.h>
#include <dm.h>
#include <init.h>
#include <linux/delay.h>
#include <log.h>
#include <panel.h>
#include <pwm.h>

#ifdef CONFIG_SPL_BUILD
/* provided to defeat compiler optimisation in board_init_f() */
void gru_dummy_function(int i)
{
}

int board_early_init_f(void)
{
# if defined(CONFIG_TARGET_CHROMEBOOK_BOB) || defined(CONFIG_TARGET_CHROMEBOOK_KEVIN)
	int sum, i;

	/*
	 * Add a delay and ensure that the compiler does not optimise this out.
	 * This is needed since the power rails tail a while to turn on, and
	 * we get garbage serial output otherwise.
	 */
	sum = 0;
	for (i = 0; i < 150000; i++)
		sum += i;
	gru_dummy_function(sum);
#endif /* CONFIG_TARGET_CHROMEBOOK_BOB */

	return 0;
}
#endif

#ifndef CONFIG_SPL_BUILD
int board_early_init_r(void)
{
	struct udevice *clk;
	int ret;

	/*
	 * This init is done in SPL, but when chain-loading U-Boot SPL will
	 * have been skipped. Allow the clock driver to check if it needs
	 * setting up.
	 */
	ret = uclass_get_device_by_driver(UCLASS_CLK,
					  DM_DRIVER_GET(clk_rk3399), &clk);
	if (ret) {
		debug("%s: CLK init failed: %d\n", __func__, ret);
		return ret;
	}

	return 0;
}
#endif

#ifdef CONFIG_BOARD_LATE_INIT
int rk_board_late_init(void)
{
	struct udevice *panel;
	struct udevice *cros_ec;
	struct udevice *cros_ec_pwm;
	int ret;

	printf("Trying to get cros_ec device\n");
	ret = uclass_first_device_err(UCLASS_CROS_EC, &cros_ec);
	if (ret) {
		printf("Failed to get cros_ec device: %d\n", ret);
		return 0;
	}

	printf("Sleeping for udelay(10000000)...\n");
	udelay(10000000);

	printf("Trying to get cros_ec_pwm device\n");
	ret = uclass_get_device_by_driver(UCLASS_PWM, DM_DRIVER_GET(cros_ec_pwm), &cros_ec_pwm);
	if (ret) {
		printf("Failed to get cros_ec_pwm device: %d\n", ret);
		return 0;

	} else {
		printf("Trying to set cros_ec_pwm duty\n");
		ret = pwm_set_config(cros_ec_pwm, 1, 0xffff, 0x4fff);
		if (ret) {
			printf("Failed to set cros_ec_pwm duty: %d\n", ret);
			return 0;
		}

		printf("Trying to enable cros_ec_pwm channel 1\n");
		ret = pwm_set_enable(cros_ec_pwm, 1, true);
		if (ret) {
			printf("Failed to enable cros_ec_pwm channel 1: %d\n", ret);
			return 0;
		}
	}

	printf("Sleeping for udelay(10000000)...\n");
	udelay(10000000);

	printf("Trying to get panel device\n");
	ret = uclass_first_device_err(UCLASS_PANEL, &panel);
	if (ret) {
		printf("Failed to get panel device: %d\n", ret);
		return 0;

	} else {
		printf("Trying to enable panel backlight\n");
		ret = panel_enable_backlight(panel);
		if (ret) {
			printf("Failed to enable panel backlight: %d\n", ret);
			return 0;
		}

		printf("Trying to set panel backlight to 70%%\n");
		panel_set_backlight(panel, 70);
		if (ret) {
			printf("Failed to set panel backlight: %d\n", ret);
			return 0;
		}
	}

	printf("Sleeping for udelay(10000000)...\n");
	udelay(10000000);

	return 0;
}
#endif
