// SPDX-License-Identifier: GPL-2.0
/*
 * (C) Copyright 2015 Google, Inc
 * (C) 2017 Theobroma Systems Design und Consulting GmbH
 */

#include <common.h>
#include <clk-uclass.h>
#include <dm.h>
#include <dt-structs.h>
#include <errno.h>
#include <log.h>
#include <malloc.h>
#include <mapmem.h>
#include <syscon.h>
#include <bitfield.h>
#include <asm/io.h>
#include <asm/arch-rockchip/clock.h>
#include <asm/arch-rockchip/cru.h>
#include <asm/arch-rockchip/hardware.h>
#include <asm/global_data.h>
#include <dm/device-internal.h>
#include <dm/lists.h>
#include <dt-bindings/clock/rk3399-cru.h>
#include <linux/bitops.h>
#include <linux/delay.h>

DECLARE_GLOBAL_DATA_PTR;

#if CONFIG_IS_ENABLED(OF_PLATDATA)
struct rk3399_clk_plat {
	struct dtd_rockchip_rk3399_cru dtd;
};

struct rk3399_pmuclk_plat {
	struct dtd_rockchip_rk3399_pmucru dtd;
};
#endif

#define RATE_TO_DIV(input_rate, output_rate) \
	((input_rate) / (output_rate) - 1)
#define DIV_TO_RATE(input_rate, div)		((input_rate) / ((div) + 1))

static struct rockchip_pll_rate_table rk3399_pll_rates[] = {
	/* _mhz, _refdiv, _fbdiv, _postdiv1, _postdiv2, _dsmpd, _frac */
	RK3036_PLL_RATE(2208000000, 1, 92, 1, 1, 1, 0),
	RK3036_PLL_RATE(2184000000, 1, 91, 1, 1, 1, 0),
	RK3036_PLL_RATE(2160000000, 1, 90, 1, 1, 1, 0),
	RK3036_PLL_RATE(2136000000, 1, 89, 1, 1, 1, 0),
	RK3036_PLL_RATE(2112000000, 1, 88, 1, 1, 1, 0),
	RK3036_PLL_RATE(2088000000, 1, 87, 1, 1, 1, 0),
	RK3036_PLL_RATE(2064000000, 1, 86, 1, 1, 1, 0),
	RK3036_PLL_RATE(2040000000, 1, 85, 1, 1, 1, 0),
	RK3036_PLL_RATE(2016000000, 1, 84, 1, 1, 1, 0),
	RK3036_PLL_RATE(1992000000, 1, 83, 1, 1, 1, 0),
	RK3036_PLL_RATE(1968000000, 1, 82, 1, 1, 1, 0),
	RK3036_PLL_RATE(1944000000, 1, 81, 1, 1, 1, 0),
	RK3036_PLL_RATE(1920000000, 1, 80, 1, 1, 1, 0),
	RK3036_PLL_RATE(1896000000, 1, 79, 1, 1, 1, 0),
	RK3036_PLL_RATE(1872000000, 1, 78, 1, 1, 1, 0),
	RK3036_PLL_RATE(1848000000, 1, 77, 1, 1, 1, 0),
	RK3036_PLL_RATE(1824000000, 1, 76, 1, 1, 1, 0),
	RK3036_PLL_RATE(1800000000, 1, 75, 1, 1, 1, 0),
	RK3036_PLL_RATE(1776000000, 1, 74, 1, 1, 1, 0),
	RK3036_PLL_RATE(1752000000, 1, 73, 1, 1, 1, 0),
	RK3036_PLL_RATE(1728000000, 1, 72, 1, 1, 1, 0),
	RK3036_PLL_RATE(1704000000, 1, 71, 1, 1, 1, 0),
	RK3036_PLL_RATE(1680000000, 1, 70, 1, 1, 1, 0),
	RK3036_PLL_RATE(1656000000, 1, 69, 1, 1, 1, 0),
	RK3036_PLL_RATE(1632000000, 1, 68, 1, 1, 1, 0),
	RK3036_PLL_RATE(1608000000, 1, 67, 1, 1, 1, 0),
	RK3036_PLL_RATE(1600000000, 3, 200, 1, 1, 1, 0),
	RK3036_PLL_RATE(1584000000, 1, 66, 1, 1, 1, 0),
	RK3036_PLL_RATE(1560000000, 1, 65, 1, 1, 1, 0),
	RK3036_PLL_RATE(1536000000, 1, 64, 1, 1, 1, 0),
	RK3036_PLL_RATE(1512000000, 1, 63, 1, 1, 1, 0),
	RK3036_PLL_RATE(1488000000, 1, 62, 1, 1, 1, 0),
	RK3036_PLL_RATE(1464000000, 1, 61, 1, 1, 1, 0),
	RK3036_PLL_RATE(1440000000, 1, 60, 1, 1, 1, 0),
	RK3036_PLL_RATE(1416000000, 1, 59, 1, 1, 1, 0),
	RK3036_PLL_RATE(1392000000, 1, 58, 1, 1, 1, 0),
	RK3036_PLL_RATE(1368000000, 1, 57, 1, 1, 1, 0),
	RK3036_PLL_RATE(1344000000, 1, 56, 1, 1, 1, 0),
	RK3036_PLL_RATE(1320000000, 1, 55, 1, 1, 1, 0),
	RK3036_PLL_RATE(1296000000, 1, 54, 1, 1, 1, 0),
	RK3036_PLL_RATE(1272000000, 1, 53, 1, 1, 1, 0),
	RK3036_PLL_RATE(1248000000, 1, 52, 1, 1, 1, 0),
	RK3036_PLL_RATE(1200000000, 1, 50, 1, 1, 1, 0),
	RK3036_PLL_RATE(1188000000, 2, 99, 1, 1, 1, 0),
	RK3036_PLL_RATE(1104000000, 1, 46, 1, 1, 1, 0),
	RK3036_PLL_RATE(1100000000, 12, 550, 1, 1, 1, 0),
	RK3036_PLL_RATE(1008000000, 1, 84, 2, 1, 1, 0),
	RK3036_PLL_RATE(1000000000, 1, 125, 3, 1, 1, 0),
	RK3036_PLL_RATE( 984000000, 1, 82, 2, 1, 1, 0),
	RK3036_PLL_RATE( 960000000, 1, 80, 2, 1, 1, 0),
	RK3036_PLL_RATE( 936000000, 1, 78, 2, 1, 1, 0),
	RK3036_PLL_RATE( 912000000, 1, 76, 2, 1, 1, 0),
	RK3036_PLL_RATE( 900000000, 4, 300, 2, 1, 1, 0),
	RK3036_PLL_RATE( 888000000, 1, 74, 2, 1, 1, 0),
	RK3036_PLL_RATE( 864000000, 1, 72, 2, 1, 1, 0),
	RK3036_PLL_RATE( 840000000, 1, 70, 2, 1, 1, 0),
	RK3036_PLL_RATE( 816000000, 1, 68, 2, 1, 1, 0),
	RK3036_PLL_RATE( 800000000, 1, 100, 3, 1, 1, 0),
	RK3036_PLL_RATE( 700000000, 6, 350, 2, 1, 1, 0),
	RK3036_PLL_RATE( 696000000, 1, 58, 2, 1, 1, 0),
	RK3036_PLL_RATE( 676000000, 3, 169, 2, 1, 1, 0),
	RK3036_PLL_RATE( 600000000, 1, 75, 3, 1, 1, 0),
	RK3036_PLL_RATE( 594000000, 1, 99, 4, 1, 1, 0),
	RK3036_PLL_RATE( 533250000, 8, 711, 4, 1, 1, 0),
	RK3036_PLL_RATE( 504000000, 1, 63, 3, 1, 1, 0),
	RK3036_PLL_RATE( 500000000, 6, 250, 2, 1, 1, 0),
	RK3036_PLL_RATE( 408000000, 1, 68, 2, 2, 1, 0),
	RK3036_PLL_RATE( 312000000, 1, 52, 2, 2, 1, 0),
	RK3036_PLL_RATE( 297000000, 1, 99, 4, 2, 1, 0),
	RK3036_PLL_RATE( 216000000, 1, 72, 4, 2, 1, 0),
	RK3036_PLL_RATE( 148500000, 1, 99, 4, 4, 1, 0),
	RK3036_PLL_RATE( 106500000, 1, 71, 4, 4, 1, 0),
	RK3036_PLL_RATE(  96000000, 1, 64, 4, 4, 1, 0),
	RK3036_PLL_RATE(  74250000, 2, 99, 4, 4, 1, 0),
	RK3036_PLL_RATE(  65000000, 1, 65, 6, 4, 1, 0),
	RK3036_PLL_RATE(  54000000, 1, 54, 6, 4, 1, 0),
	RK3036_PLL_RATE(  27000000, 1, 27, 6, 4, 1, 0),
	{ /* sentinel */ },
};

static const struct rockchip_pll_rate_table *apll_cfgs[] = {
	[APLL_1600_MHZ] = &rk3399_pll_rates[26],
	[APLL_816_MHZ] = &rk3399_pll_rates[56],
	[APLL_600_MHZ] = &rk3399_pll_rates[61],
};

#define RK3399_PLL_CON(x)		((x) * 0x4)
#define RK3399_PMU_PLL_CON(x)		((x) * 0x4)
#define RKCLK_PLL_SYNC_RATE		1
static struct rockchip_pll_clock rk3399_pll_clks[] = {
	/* _type, _id, _con, _mode, _mshift, _lshift, _pflags, _rtable */
	[APLLL] = PLL(pll_rk3399, PLL_APLLL, RK3399_PLL_CON(0),
		      RK3399_PLL_CON(3), 8, 31, 0, rk3399_pll_rates),
	[APLLB] = PLL(pll_rk3399, PLL_APLLB, RK3399_PLL_CON(8),
		      RK3399_PLL_CON(11), 8, 31, 0, rk3399_pll_rates),
	[DPLL] = PLL(pll_rk3399, PLL_DPLL, RK3399_PLL_CON(16),
		     RK3399_PLL_CON(19), 8, 31, 0, NULL),
	[CPLL] = PLL(pll_rk3399, PLL_CPLL, RK3399_PLL_CON(24),
		     RK3399_PLL_CON(27), 8, 31, RKCLK_PLL_SYNC_RATE, rk3399_pll_rates),
	[GPLL] = PLL(pll_rk3399, PLL_GPLL, RK3399_PLL_CON(32),
		     RK3399_PLL_CON(35), 8, 31, RKCLK_PLL_SYNC_RATE, rk3399_pll_rates),
	[NPLL] = PLL(pll_rk3399, PLL_NPLL, RK3399_PLL_CON(40),
		     RK3399_PLL_CON(43), 8, 31, RKCLK_PLL_SYNC_RATE, rk3399_pll_rates),
	[VPLL] = PLL(pll_rk3399, PLL_VPLL, RK3399_PLL_CON(48),
		     RK3399_PLL_CON(51), 8, 31, RKCLK_PLL_SYNC_RATE, rk3399_pll_rates),
	[PPLL] = PLL(pll_rk3399, PLL_PPLL, RK3399_PMU_PLL_CON(0),
		     RK3399_PMU_PLL_CON(3), 8, 31, RKCLK_PLL_SYNC_RATE, rk3399_pll_rates),
};

#ifndef CONFIG_SPL_BUILD
#define RK3399_CLK_DUMP(_id, _name, _iscru)    \
{                                              \
	.id = _id,                              \
	.name = _name,                          \
	.is_cru = _iscru,                       \
}

static const struct rk3399_clk_info clks_dump[] = {
	RK3399_CLK_DUMP(PLL_APLLL, "aplll", true),
	RK3399_CLK_DUMP(PLL_APLLB, "apllb", true),
	RK3399_CLK_DUMP(PLL_DPLL, "dpll", true),
	RK3399_CLK_DUMP(PLL_CPLL, "cpll", true),
	RK3399_CLK_DUMP(PLL_GPLL, "gpll", true),
	RK3399_CLK_DUMP(PLL_NPLL, "npll", true),
	RK3399_CLK_DUMP(PLL_VPLL, "vpll", true),
	RK3399_CLK_DUMP(ACLK_PERIHP, "aclk_perihp", true),
	RK3399_CLK_DUMP(HCLK_PERIHP, "hclk_perihp", true),
	RK3399_CLK_DUMP(PCLK_PERIHP, "pclk_perihp", true),
	RK3399_CLK_DUMP(ACLK_PERILP0, "aclk_perilp0", true),
	RK3399_CLK_DUMP(HCLK_PERILP0, "hclk_perilp0", true),
	RK3399_CLK_DUMP(PCLK_PERILP0, "pclk_perilp0", true),
	RK3399_CLK_DUMP(HCLK_PERILP1, "hclk_perilp1", true),
	RK3399_CLK_DUMP(PCLK_PERILP1, "pclk_perilp1", true),
};
#endif

enum {
	/* PLL_CON0 */
	PLL_FBDIV_MASK			= 0xfff,
	PLL_FBDIV_SHIFT			= 0,

	/* PLL_CON1 */
	PLL_POSTDIV2_SHIFT		= 12,
	PLL_POSTDIV2_MASK		= 0x7 << PLL_POSTDIV2_SHIFT,
	PLL_POSTDIV1_SHIFT		= 8,
	PLL_POSTDIV1_MASK		= 0x7 << PLL_POSTDIV1_SHIFT,
	PLL_REFDIV_MASK			= 0x3f,
	PLL_REFDIV_SHIFT		= 0,

	/* PLL_CON2 */
	PLL_LOCK_STATUS_SHIFT		= 31,
	PLL_LOCK_STATUS_MASK		= 1 << PLL_LOCK_STATUS_SHIFT,
	PLL_FRACDIV_MASK		= 0xffffff,
	PLL_FRACDIV_SHIFT		= 0,

	/* PLL_CON3 */
	PLL_MODE_SHIFT			= 8,
	PLL_MODE_MASK			= 3 << PLL_MODE_SHIFT,
	PLL_MODE_SLOW			= 0,
	PLL_MODE_NORM,
	PLL_MODE_DEEP,
	PLL_DSMPD_SHIFT			= 3,
	PLL_DSMPD_MASK			= 1 << PLL_DSMPD_SHIFT,
	PLL_INTEGER_MODE		= 1,

	/* PMUCRU_CLKSEL_CON0 */
	PMU_PCLK_DIV_CON_MASK		= 0x1f,
	PMU_PCLK_DIV_CON_SHIFT		= 0,

	/* PMUCRU_CLKSEL_CON1 */
	SPI3_PLL_SEL_SHIFT		= 7,
	SPI3_PLL_SEL_MASK		= 1 << SPI3_PLL_SEL_SHIFT,
	SPI3_PLL_SEL_24M		= 0,
	SPI3_PLL_SEL_PPLL		= 1,
	SPI3_DIV_CON_SHIFT		= 0x0,
	SPI3_DIV_CON_MASK		= 0x7f,

	/* PMUCRU_CLKSEL_CON2 */
	I2C_DIV_CON_MASK		= 0x7f,
	CLK_I2C8_DIV_CON_SHIFT		= 8,
	CLK_I2C0_DIV_CON_SHIFT		= 0,

	/* PMUCRU_CLKSEL_CON3 */
	CLK_I2C4_DIV_CON_SHIFT		= 0,

	/* CLKSEL_CON0 / CLKSEL_CON2 */
	ACLKM_CORE_DIV_CON_SHIFT	= 8,
	ACLKM_CORE_DIV_CON_MASK		= 0x1f << ACLKM_CORE_DIV_CON_SHIFT,
	CLK_CORE_PLL_SEL_SHIFT		= 6,
	CLK_CORE_PLL_SEL_MASK		= 3 << CLK_CORE_PLL_SEL_SHIFT,
	CLK_CORE_PLL_SEL_ALPLL		= 0x0,
	CLK_CORE_PLL_SEL_ABPLL		= 0x1,
	CLK_CORE_PLL_SEL_DPLL		= 0x10,
	CLK_CORE_PLL_SEL_GPLL		= 0x11,
	CLK_CORE_DIV_SHIFT		= 0,
	CLK_CORE_DIV_MASK		= 0x1f << CLK_CORE_DIV_SHIFT,

	/* CLKSEL_CON1 / CLKSEL_CON3 */
	PCLK_DBG_DIV_SHIFT		= 0x8,
	PCLK_DBG_DIV_MASK		= 0x1f << PCLK_DBG_DIV_SHIFT,
	ATCLK_CORE_DIV_SHIFT		= 0,
	ATCLK_CORE_DIV_MASK		= 0x1f << ATCLK_CORE_DIV_SHIFT,

	/* CLKSEL_CON14 */
	PCLK_PERIHP_DIV_CON_SHIFT	= 12,
	PCLK_PERIHP_DIV_CON_MASK	= 0x7 << PCLK_PERIHP_DIV_CON_SHIFT,
	HCLK_PERIHP_DIV_CON_SHIFT	= 8,
	HCLK_PERIHP_DIV_CON_MASK	= 3 << HCLK_PERIHP_DIV_CON_SHIFT,
	ACLK_PERIHP_PLL_SEL_SHIFT	= 7,
	ACLK_PERIHP_PLL_SEL_MASK	= 1 << ACLK_PERIHP_PLL_SEL_SHIFT,
	ACLK_PERIHP_PLL_SEL_CPLL	= 0,
	ACLK_PERIHP_PLL_SEL_GPLL	= 1,
	ACLK_PERIHP_DIV_CON_SHIFT	= 0,
	ACLK_PERIHP_DIV_CON_MASK	= 0x1f,

	/* CLKSEL_CON21 */
	ACLK_EMMC_PLL_SEL_SHIFT         = 7,
	ACLK_EMMC_PLL_SEL_MASK          = 0x1 << ACLK_EMMC_PLL_SEL_SHIFT,
	ACLK_EMMC_PLL_SEL_GPLL          = 0x1,
	ACLK_EMMC_DIV_CON_SHIFT         = 0,
	ACLK_EMMC_DIV_CON_MASK          = 0x1f,

	/* CLKSEL_CON22 */
	CLK_EMMC_PLL_SHIFT              = 8,
	CLK_EMMC_PLL_MASK               = 0x7 << CLK_EMMC_PLL_SHIFT,
	CLK_EMMC_PLL_SEL_GPLL           = 0x1,
	CLK_EMMC_PLL_SEL_24M            = 0x5,
	CLK_EMMC_DIV_CON_SHIFT          = 0,
	CLK_EMMC_DIV_CON_MASK           = 0x7f << CLK_EMMC_DIV_CON_SHIFT,

	/* CLKSEL_CON23 */
	PCLK_PERILP0_DIV_CON_SHIFT	= 12,
	PCLK_PERILP0_DIV_CON_MASK	= 0x7 << PCLK_PERILP0_DIV_CON_SHIFT,
	HCLK_PERILP0_DIV_CON_SHIFT	= 8,
	HCLK_PERILP0_DIV_CON_MASK	= 3 << HCLK_PERILP0_DIV_CON_SHIFT,
	ACLK_PERILP0_PLL_SEL_SHIFT	= 7,
	ACLK_PERILP0_PLL_SEL_MASK	= 1 << ACLK_PERILP0_PLL_SEL_SHIFT,
	ACLK_PERILP0_PLL_SEL_CPLL	= 0,
	ACLK_PERILP0_PLL_SEL_GPLL	= 1,
	ACLK_PERILP0_DIV_CON_SHIFT	= 0,
	ACLK_PERILP0_DIV_CON_MASK	= 0x1f,

	/* CRU_CLK_SEL24_CON */
	CRYPTO0_PLL_SEL_SHIFT		= 6,
	CRYPTO0_PLL_SEL_MASK		= 3 << CRYPTO0_PLL_SEL_SHIFT,
	CRYPTO_PLL_SEL_CPLL		= 0,
	CRYPTO_PLL_SEL_GPLL,
	CRYPTO_PLL_SEL_PPLL		= 0,
	CRYPTO0_DIV_SHIFT		= 0,
	CRYPTO0_DIV_MASK		= 0x1f << CRYPTO0_DIV_SHIFT,

	/* CLKSEL_CON25 */
	PCLK_PERILP1_DIV_CON_SHIFT	= 8,
	PCLK_PERILP1_DIV_CON_MASK	= 0x7 << PCLK_PERILP1_DIV_CON_SHIFT,
	HCLK_PERILP1_PLL_SEL_SHIFT	= 7,
	HCLK_PERILP1_PLL_SEL_MASK	= 1 << HCLK_PERILP1_PLL_SEL_SHIFT,
	HCLK_PERILP1_PLL_SEL_CPLL	= 0,
	HCLK_PERILP1_PLL_SEL_GPLL	= 1,
	HCLK_PERILP1_DIV_CON_SHIFT	= 0,
	HCLK_PERILP1_DIV_CON_MASK	= 0x1f,

	/* CLKSEL_CON26 */
	CLK_SARADC_DIV_CON_SHIFT	= 8,
	CLK_SARADC_DIV_CON_MASK		= GENMASK(15, 8),
	CLK_SARADC_DIV_CON_WIDTH	= 8,
	CRYPTO1_PLL_SEL_SHIFT		= 6,
	CRYPTO1_PLL_SEL_MASK		= 3 << CRYPTO1_PLL_SEL_SHIFT,
	CRYPTO1_DIV_SHIFT		= 0,
	CRYPTO1_DIV_MASK		= 0x1f << CRYPTO1_DIV_SHIFT,

	/* CLKSEL_CON27 */
	CLK_TSADC_SEL_X24M		= 0x0,
	CLK_TSADC_SEL_SHIFT		= 15,
	CLK_TSADC_SEL_MASK		= 1 << CLK_TSADC_SEL_SHIFT,
	CLK_TSADC_DIV_CON_SHIFT		= 0,
	CLK_TSADC_DIV_CON_MASK		= 0x3ff,

	/* CLKSEL_CON47 & CLKSEL_CON48 */
	ACLK_VOP_PLL_SEL_SHIFT		= 6,
	ACLK_VOP_PLL_SEL_MASK		= 0x3 << ACLK_VOP_PLL_SEL_SHIFT,
	ACLK_VOP_PLL_SEL_CPLL		= 0x1,
	ACLK_VOP_PLL_SEL_GPLL		= 0x2,
	ACLK_VOP_DIV_CON_SHIFT		= 0,
	ACLK_VOP_DIV_CON_MASK		= 0x1f << ACLK_VOP_DIV_CON_SHIFT,

	/* CLKSEL_CON49 & CLKSEL_CON50 */
	DCLK_VOP_DCLK_SEL_SHIFT         = 11,
	DCLK_VOP_DCLK_SEL_MASK          = 1 << DCLK_VOP_DCLK_SEL_SHIFT,
	DCLK_VOP_DCLK_SEL_DIVOUT        = 0,
	DCLK_VOP_PLL_SEL_SHIFT          = 8,
	DCLK_VOP_PLL_SEL_MASK           = 3 << DCLK_VOP_PLL_SEL_SHIFT,
	DCLK_VOP_PLL_SEL_VPLL           = 0,
	DCLK_VOP_PLL_SEL_CPLL           = 1,
	DCLK_VOP_DIV_CON_MASK           = 0xff,
	DCLK_VOP_DIV_CON_SHIFT          = 0,

	/* CLKSEL_CON57 */
	PCLK_ALIVE_DIV_CON_SHIFT        = 0,
	PCLK_ALIVE_DIV_CON_MASK         = 0x1f << PCLK_ALIVE_DIV_CON_SHIFT,

	/* CLKSEL_CON58 */
	CLK_SPI_PLL_SEL_WIDTH = 1,
	CLK_SPI_PLL_SEL_MASK = ((1 < CLK_SPI_PLL_SEL_WIDTH) - 1),
	CLK_SPI_PLL_SEL_CPLL = 0,
	CLK_SPI_PLL_SEL_GPLL = 1,
	CLK_SPI_PLL_DIV_CON_WIDTH = 7,
	CLK_SPI_PLL_DIV_CON_MASK = ((1 << CLK_SPI_PLL_DIV_CON_WIDTH) - 1),

	CLK_SPI5_PLL_DIV_CON_SHIFT      = 8,
	CLK_SPI5_PLL_SEL_SHIFT	        = 15,

	/* CLKSEL_CON59 */
	CLK_SPI1_PLL_SEL_SHIFT		= 15,
	CLK_SPI1_PLL_DIV_CON_SHIFT	= 8,
	CLK_SPI0_PLL_SEL_SHIFT		= 7,
	CLK_SPI0_PLL_DIV_CON_SHIFT	= 0,

	/* CLKSEL_CON60 */
	CLK_SPI4_PLL_SEL_SHIFT		= 15,
	CLK_SPI4_PLL_DIV_CON_SHIFT	= 8,
	CLK_SPI2_PLL_SEL_SHIFT		= 7,
	CLK_SPI2_PLL_DIV_CON_SHIFT	= 0,

	/* CLKSEL_CON61 */
	CLK_I2C_PLL_SEL_MASK		= 1,
	CLK_I2C_PLL_SEL_CPLL		= 0,
	CLK_I2C_PLL_SEL_GPLL		= 1,
	CLK_I2C5_PLL_SEL_SHIFT		= 15,
	CLK_I2C5_DIV_CON_SHIFT		= 8,
	CLK_I2C1_PLL_SEL_SHIFT		= 7,
	CLK_I2C1_DIV_CON_SHIFT		= 0,

	/* CLKSEL_CON62 */
	CLK_I2C6_PLL_SEL_SHIFT		= 15,
	CLK_I2C6_DIV_CON_SHIFT		= 8,
	CLK_I2C2_PLL_SEL_SHIFT		= 7,
	CLK_I2C2_DIV_CON_SHIFT		= 0,

	/* CLKSEL_CON63 */
	CLK_I2C7_PLL_SEL_SHIFT		= 15,
	CLK_I2C7_DIV_CON_SHIFT		= 8,
	CLK_I2C3_PLL_SEL_SHIFT		= 7,
	CLK_I2C3_DIV_CON_SHIFT		= 0,

	/* CRU_SOFTRST_CON4 */
	RESETN_DDR0_REQ_SHIFT		= 8,
	RESETN_DDR0_REQ_MASK		= 1 << RESETN_DDR0_REQ_SHIFT,
	RESETN_DDRPHY0_REQ_SHIFT	= 9,
	RESETN_DDRPHY0_REQ_MASK		= 1 << RESETN_DDRPHY0_REQ_SHIFT,
	RESETN_DDR1_REQ_SHIFT		= 12,
	RESETN_DDR1_REQ_MASK		= 1 << RESETN_DDR1_REQ_SHIFT,
	RESETN_DDRPHY1_REQ_SHIFT	= 13,
	RESETN_DDRPHY1_REQ_MASK		= 1 << RESETN_DDRPHY1_REQ_SHIFT,
};

void rk3399_configure_cpu(struct rockchip_cru *cru,
			  enum apll_frequencies freq,
			  enum cpu_cluster cluster)
{
	u32 aclkm_div;
	u32 pclk_dbg_div;
	u32 atclk_div, apll_hz;
	int con_base, parent;
	enum rk3399_pll_id pll;

	switch (cluster) {
	case CPU_CLUSTER_LITTLE:
		con_base = 0;
		parent = CLK_CORE_PLL_SEL_ALPLL;
		pll = APLLL;
		break;
	case CPU_CLUSTER_BIG:
	default:
		con_base = 2;
		parent = CLK_CORE_PLL_SEL_ABPLL;
		pll = APLLB;
		break;
	}

	apll_hz = apll_cfgs[freq]->rate;
	rockchip_pll_set_rate(&rk3399_pll_clks[pll], cru, pll, apll_hz);

	aclkm_div = apll_hz / ACLKM_CORE_HZ - 1;
	assert((aclkm_div + 1) * ACLKM_CORE_HZ <= apll_hz &&
	       aclkm_div < 0x1f);

	pclk_dbg_div = apll_hz / PCLK_DBG_HZ - 1;
	assert((pclk_dbg_div + 1) * PCLK_DBG_HZ <= apll_hz &&
	       pclk_dbg_div < 0x1f);

	atclk_div = apll_hz / ATCLK_CORE_HZ - 1;
	assert((atclk_div + 1) * ATCLK_CORE_HZ <= apll_hz &&
	       atclk_div < 0x1f);

	rk_clrsetreg(&cru->clksel_con[con_base],
		     ACLKM_CORE_DIV_CON_MASK | CLK_CORE_PLL_SEL_MASK |
		     CLK_CORE_DIV_MASK,
		     aclkm_div << ACLKM_CORE_DIV_CON_SHIFT |
		     parent << CLK_CORE_PLL_SEL_SHIFT |
		     0 << CLK_CORE_DIV_SHIFT);

	rk_clrsetreg(&cru->clksel_con[con_base + 1],
		     PCLK_DBG_DIV_MASK | ATCLK_CORE_DIV_MASK,
		     pclk_dbg_div << PCLK_DBG_DIV_SHIFT |
		     atclk_div << ATCLK_CORE_DIV_SHIFT);
}

#define I2C_CLK_REG_MASK(bus) \
	(I2C_DIV_CON_MASK << CLK_I2C ##bus## _DIV_CON_SHIFT | \
	 CLK_I2C_PLL_SEL_MASK << CLK_I2C ##bus## _PLL_SEL_SHIFT)

#define I2C_CLK_REG_VALUE(bus, clk_div) \
	((clk_div - 1) << CLK_I2C ##bus## _DIV_CON_SHIFT | \
	 CLK_I2C_PLL_SEL_GPLL << CLK_I2C ##bus## _PLL_SEL_SHIFT)

#define I2C_CLK_DIV_VALUE(con, bus) \
	((con >> CLK_I2C ##bus## _DIV_CON_SHIFT) & I2C_DIV_CON_MASK)

#define I2C_PMUCLK_REG_MASK(bus) \
	(I2C_DIV_CON_MASK << CLK_I2C ##bus## _DIV_CON_SHIFT)

#define I2C_PMUCLK_REG_VALUE(bus, clk_div) \
	((clk_div - 1) << CLK_I2C ##bus## _DIV_CON_SHIFT)

static ulong rk3399_i2c_get_clk(struct rockchip_cru *cru, ulong clk_id)
{
	u32 div, con;

	switch (clk_id) {
	case SCLK_I2C1:
		con = readl(&cru->clksel_con[61]);
		div = I2C_CLK_DIV_VALUE(con, 1);
		break;
	case SCLK_I2C2:
		con = readl(&cru->clksel_con[62]);
		div = I2C_CLK_DIV_VALUE(con, 2);
		break;
	case SCLK_I2C3:
		con = readl(&cru->clksel_con[63]);
		div = I2C_CLK_DIV_VALUE(con, 3);
		break;
	case SCLK_I2C5:
		con = readl(&cru->clksel_con[61]);
		div = I2C_CLK_DIV_VALUE(con, 5);
		break;
	case SCLK_I2C6:
		con = readl(&cru->clksel_con[62]);
		div = I2C_CLK_DIV_VALUE(con, 6);
		break;
	case SCLK_I2C7:
		con = readl(&cru->clksel_con[63]);
		div = I2C_CLK_DIV_VALUE(con, 7);
		break;
	default:
		printf("do not support this i2c bus\n");
		return -EINVAL;
	}

	return DIV_TO_RATE(GPLL_HZ, div);
}

static ulong rk3399_i2c_set_clk(struct rockchip_cru *cru, ulong clk_id, uint hz)
{
	int src_clk_div;

	/* i2c0,4,8 src clock from ppll, i2c1,2,3,5,6,7 src clock from gpll*/
	src_clk_div = GPLL_HZ / hz;
	assert(src_clk_div - 1 <= 127);

	switch (clk_id) {
	case SCLK_I2C1:
		rk_clrsetreg(&cru->clksel_con[61], I2C_CLK_REG_MASK(1),
			     I2C_CLK_REG_VALUE(1, src_clk_div));
		break;
	case SCLK_I2C2:
		rk_clrsetreg(&cru->clksel_con[62], I2C_CLK_REG_MASK(2),
			     I2C_CLK_REG_VALUE(2, src_clk_div));
		break;
	case SCLK_I2C3:
		rk_clrsetreg(&cru->clksel_con[63], I2C_CLK_REG_MASK(3),
			     I2C_CLK_REG_VALUE(3, src_clk_div));
		break;
	case SCLK_I2C5:
		rk_clrsetreg(&cru->clksel_con[61], I2C_CLK_REG_MASK(5),
			     I2C_CLK_REG_VALUE(5, src_clk_div));
		break;
	case SCLK_I2C6:
		rk_clrsetreg(&cru->clksel_con[62], I2C_CLK_REG_MASK(6),
			     I2C_CLK_REG_VALUE(6, src_clk_div));
		break;
	case SCLK_I2C7:
		rk_clrsetreg(&cru->clksel_con[63], I2C_CLK_REG_MASK(7),
			     I2C_CLK_REG_VALUE(7, src_clk_div));
		break;
	default:
		printf("do not support this i2c bus\n");
		return -EINVAL;
	}

	return rk3399_i2c_get_clk(cru, clk_id);
}

/*
 * RK3399 SPI clocks have a common divider-width (7 bits) and a single bit
 * to select either CPLL or GPLL as the clock-parent. The location within
 * the enclosing CLKSEL_CON (i.e. div_shift and sel_shift) are variable.
 */

struct spi_clkreg {
	u8 reg;  /* CLKSEL_CON[reg] register in CRU */
	u8 div_shift;
	u8 sel_shift;
};

/*
 * The entries are numbered relative to their offset from SCLK_SPI0.
 *
 * Note that SCLK_SPI3 (which is configured via PMUCRU and requires different
 * logic is not supported).
 */
static const struct spi_clkreg spi_clkregs[] = {
	[0] = { .reg = 59,
		.div_shift = CLK_SPI0_PLL_DIV_CON_SHIFT,
		.sel_shift = CLK_SPI0_PLL_SEL_SHIFT, },
	[1] = { .reg = 59,
		.div_shift = CLK_SPI1_PLL_DIV_CON_SHIFT,
		.sel_shift = CLK_SPI1_PLL_SEL_SHIFT, },
	[2] = { .reg = 60,
		.div_shift = CLK_SPI2_PLL_DIV_CON_SHIFT,
		.sel_shift = CLK_SPI2_PLL_SEL_SHIFT, },
	[3] = { .reg = 60,
		.div_shift = CLK_SPI4_PLL_DIV_CON_SHIFT,
		.sel_shift = CLK_SPI4_PLL_SEL_SHIFT, },
	[4] = { .reg = 58,
		.div_shift = CLK_SPI5_PLL_DIV_CON_SHIFT,
		.sel_shift = CLK_SPI5_PLL_SEL_SHIFT, },
};

static ulong rk3399_spi_get_clk(struct rockchip_cru *cru, ulong clk_id)
{
	const struct spi_clkreg *spiclk = NULL;
	u32 div, val;

	switch (clk_id) {
	case SCLK_SPI0 ... SCLK_SPI5:
		spiclk = &spi_clkregs[clk_id - SCLK_SPI0];
		break;

	default:
		pr_err("%s: SPI clk-id %ld not supported\n", __func__, clk_id);
		return -EINVAL;
	}

	val = readl(&cru->clksel_con[spiclk->reg]);
	div = bitfield_extract(val, spiclk->div_shift,
			       CLK_SPI_PLL_DIV_CON_WIDTH);

	return DIV_TO_RATE(GPLL_HZ, div);
}

static ulong rk3399_spi_set_clk(struct rockchip_cru *cru, ulong clk_id, uint hz)
{
	const struct spi_clkreg *spiclk = NULL;
	int src_clk_div;

	src_clk_div = DIV_ROUND_UP(GPLL_HZ, hz) - 1;
	assert(src_clk_div < 128);

	switch (clk_id) {
	case SCLK_SPI1 ... SCLK_SPI5:
		spiclk = &spi_clkregs[clk_id - SCLK_SPI0];
		break;

	default:
		pr_err("%s: SPI clk-id %ld not supported\n", __func__, clk_id);
		return -EINVAL;
	}

	rk_clrsetreg(&cru->clksel_con[spiclk->reg],
		     ((CLK_SPI_PLL_DIV_CON_MASK << spiclk->div_shift) |
		       (CLK_SPI_PLL_SEL_GPLL << spiclk->sel_shift)),
		     ((src_clk_div << spiclk->div_shift) |
		      (CLK_SPI_PLL_SEL_GPLL << spiclk->sel_shift)));

	return rk3399_spi_get_clk(cru, clk_id);
}

#define RK3399_LIMIT_PLL_ACLK_VOP	(400 * 1000000)

static ulong rk3399_vop_set_clk(struct rockchip_cru *cru, ulong clk_id, u32 hz)
{
	int aclk_vop = RK3399_LIMIT_PLL_ACLK_VOP;
	void *aclkreg_addr, *dclkreg_addr;
	u32 div = 1;

	switch (clk_id) {
	case DCLK_VOP0:
		aclkreg_addr = &cru->clksel_con[47];
		dclkreg_addr = &cru->clksel_con[49];
		break;
	case DCLK_VOP1:
		aclkreg_addr = &cru->clksel_con[48];
		dclkreg_addr = &cru->clksel_con[50];
		break;
	default:
		return -EINVAL;
	}
	/* vop aclk source clk: cpll */
	div = GPLL_HZ / aclk_vop;
	assert(div - 1 <= 31);

	rk_clrsetreg(aclkreg_addr,
		     ACLK_VOP_PLL_SEL_MASK | ACLK_VOP_DIV_CON_MASK,
		     ACLK_VOP_PLL_SEL_GPLL << ACLK_VOP_PLL_SEL_SHIFT |
		     (div - 1) << ACLK_VOP_DIV_CON_SHIFT);

	if (readl(dclkreg_addr) & DCLK_VOP_PLL_SEL_MASK) {
		rockchip_pll_set_rate(&rk3399_pll_clks[CPLL], cru, CPLL, hz);
	} else {
		rockchip_pll_set_rate(&rk3399_pll_clks[VPLL], cru, VPLL, hz);
	}

	rk_clrsetreg(dclkreg_addr,
		     DCLK_VOP_DCLK_SEL_MASK | DCLK_VOP_DIV_CON_MASK,
		     DCLK_VOP_DCLK_SEL_DIVOUT << DCLK_VOP_DCLK_SEL_SHIFT |
		     (1 - 1) << DCLK_VOP_DIV_CON_SHIFT);

	return hz;
}

static ulong rk3399_mmc_get_clk(struct rockchip_cru *cru, uint clk_id)
{
	u32 div, con;

	switch (clk_id) {
	case HCLK_SDMMC:
	case SCLK_SDMMC:
		con = readl(&cru->clksel_con[16]);
		/* dwmmc controller have internal div 2 */
		div = 2;
		break;
	case SCLK_EMMC:
		con = readl(&cru->clksel_con[22]);
		div = 1;
		break;
	default:
		return -EINVAL;
	}

	div *= (con & CLK_EMMC_DIV_CON_MASK) >> CLK_EMMC_DIV_CON_SHIFT;
	if ((con & CLK_EMMC_PLL_MASK) >> CLK_EMMC_PLL_SHIFT
			== CLK_EMMC_PLL_SEL_24M)
		return DIV_TO_RATE(OSC_HZ, div);
	else
		return DIV_TO_RATE(GPLL_HZ, div);
}

static ulong rk3399_mmc_set_clk(struct rockchip_cru *cru,
				ulong clk_id, ulong set_rate)
{
	int src_clk_div;
	int aclk_emmc = 198 * MHz;

	switch (clk_id) {
	case HCLK_SDMMC:
	case SCLK_SDMMC:
		/* Select clk_sdmmc source from GPLL by default */
		/* mmc clock defaulg div 2 internal, provide double in cru */
		src_clk_div = DIV_ROUND_UP(GPLL_HZ / 2, set_rate);

		if (src_clk_div > 128) {
			/* use 24MHz source for 400KHz clock */
			src_clk_div = DIV_ROUND_UP(OSC_HZ / 2, set_rate);
			assert(src_clk_div - 1 < 128);
			rk_clrsetreg(&cru->clksel_con[16],
				     CLK_EMMC_PLL_MASK | CLK_EMMC_DIV_CON_MASK,
				     CLK_EMMC_PLL_SEL_24M << CLK_EMMC_PLL_SHIFT |
				     (src_clk_div - 1) << CLK_EMMC_DIV_CON_SHIFT);
		} else {
			rk_clrsetreg(&cru->clksel_con[16],
				     CLK_EMMC_PLL_MASK | CLK_EMMC_DIV_CON_MASK,
				     CLK_EMMC_PLL_SEL_GPLL << CLK_EMMC_PLL_SHIFT |
				     (src_clk_div - 1) << CLK_EMMC_DIV_CON_SHIFT);
		}
		break;
	case SCLK_EMMC:
		/* Select aclk_emmc source from GPLL */
		src_clk_div = DIV_ROUND_UP(GPLL_HZ, aclk_emmc);
		assert(src_clk_div - 1 < 32);

		rk_clrsetreg(&cru->clksel_con[21],
			     ACLK_EMMC_PLL_SEL_MASK | ACLK_EMMC_DIV_CON_MASK,
			     ACLK_EMMC_PLL_SEL_GPLL << ACLK_EMMC_PLL_SEL_SHIFT |
			     (src_clk_div - 1) << ACLK_EMMC_DIV_CON_SHIFT);

		/* Select clk_emmc source from GPLL too */
		src_clk_div = DIV_ROUND_UP(GPLL_HZ, set_rate);
		if (src_clk_div > 128) {
			/* use 24MHz source for 400KHz clock */
			src_clk_div = DIV_ROUND_UP(OSC_HZ, set_rate);
			assert(src_clk_div - 1 < 128);
			rk_clrsetreg(&cru->clksel_con[22],
				     CLK_EMMC_PLL_MASK | CLK_EMMC_DIV_CON_MASK,
				     CLK_EMMC_PLL_SEL_24M << CLK_EMMC_PLL_SHIFT |
				     (src_clk_div - 1) << CLK_EMMC_DIV_CON_SHIFT);
		} else {
			rk_clrsetreg(&cru->clksel_con[22],
				     CLK_EMMC_PLL_MASK | CLK_EMMC_DIV_CON_MASK,
				     CLK_EMMC_PLL_SEL_GPLL << CLK_EMMC_PLL_SHIFT |
				     (src_clk_div - 1) << CLK_EMMC_DIV_CON_SHIFT);
		}
		break;
	default:
		return -EINVAL;
	}
	return rk3399_mmc_get_clk(cru, clk_id);
}

static ulong rk3399_gmac_set_clk(struct rockchip_cru *cru, ulong rate)
{
	ulong ret;

	/*
	 * The RGMII CLK can be derived either from an external "clkin"
	 * or can be generated from internally by a divider from SCLK_MAC.
	 */
	if (readl(&cru->clksel_con[19]) & BIT(4)) {
		/* An external clock will always generate the right rate... */
		ret = rate;
	} else {
		/*
		 * No platform uses an internal clock to date.
		 * Implement this once it becomes necessary and print an error
		 * if someone tries to use it (while it remains unimplemented).
		 */
		pr_err("%s: internal clock is UNIMPLEMENTED\n", __func__);
		ret = 0;
	}

	return ret;
}

#define PMUSGRF_DDR_RGN_CON16 0xff330040
static ulong rk3399_ddr_set_clk(struct rockchip_cru *cru,
				ulong set_rate)
{
	struct rockchip_pll_rate_table dpll_cfg;

	/*  IC ECO bug, need to set this register */
	writel(0xc000c000, PMUSGRF_DDR_RGN_CON16);

	/*  clk_ddrc == DPLL = 24MHz / refdiv * fbdiv / postdiv1 / postdiv2 */
	switch (set_rate) {
	case 50 * MHz:
		dpll_cfg = (struct rockchip_pll_rate_table)
		{.refdiv = 1, .fbdiv = 12, .postdiv1 = 3, .postdiv2 = 2};
		break;
	case 200 * MHz:
		dpll_cfg = (struct rockchip_pll_rate_table)
		{.refdiv = 1, .fbdiv = 50, .postdiv1 = 6, .postdiv2 = 1};
		break;
	case 300 * MHz:
		dpll_cfg = (struct rockchip_pll_rate_table)
		{.refdiv = 2, .fbdiv = 100, .postdiv1 = 4, .postdiv2 = 1};
		break;
	case 400 * MHz:
		dpll_cfg = (struct rockchip_pll_rate_table)
		{.refdiv = 1, .fbdiv = 50, .postdiv1 = 3, .postdiv2 = 1};
		break;
	case 666 * MHz:
		dpll_cfg = (struct rockchip_pll_rate_table)
		{.refdiv = 2, .fbdiv = 111, .postdiv1 = 2, .postdiv2 = 1};
		break;
	case 800 * MHz:
		dpll_cfg = (struct rockchip_pll_rate_table)
		{.refdiv = 1, .fbdiv = 100, .postdiv1 = 3, .postdiv2 = 1};
		break;
	case 933 * MHz:
		dpll_cfg = (struct rockchip_pll_rate_table)
		{.refdiv = 1, .fbdiv = 116, .postdiv1 = 3, .postdiv2 = 1};
		break;
	default:
		pr_err("Unsupported SDRAM frequency!,%ld\n", set_rate);
	}
	rockchip_pll_set_rate(&rk3399_pll_clks[DPLL], cru, DPLL, set_rate);

	return set_rate;
}

static ulong rk3399_alive_get_clk(struct rockchip_cru *cru)
{
        u32 div, val;

        val = readl(&cru->clksel_con[57]);
        div = (val & PCLK_ALIVE_DIV_CON_MASK) >>
	       PCLK_ALIVE_DIV_CON_SHIFT;

        return DIV_TO_RATE(GPLL_HZ, div);
}

static ulong rk3399_saradc_get_clk(struct rockchip_cru *cru)
{
	u32 div, val;

	val = readl(&cru->clksel_con[26]);
	div = bitfield_extract(val, CLK_SARADC_DIV_CON_SHIFT,
			       CLK_SARADC_DIV_CON_WIDTH);

	return DIV_TO_RATE(OSC_HZ, div);
}

static ulong rk3399_saradc_set_clk(struct rockchip_cru *cru, uint hz)
{
	int src_clk_div;

	src_clk_div = DIV_ROUND_UP(OSC_HZ, hz) - 1;
	assert(src_clk_div <= 255);

	rk_clrsetreg(&cru->clksel_con[26],
		     CLK_SARADC_DIV_CON_MASK,
		     src_clk_div << CLK_SARADC_DIV_CON_SHIFT);

	return rk3399_saradc_get_clk(cru);
}

static ulong rk3399_tsadc_get_clk(struct rockchip_cru *cru)
{
	u32 div, val;

	val = readl(&cru->clksel_con[27]);
	div = bitfield_extract(val, CLK_TSADC_SEL_SHIFT,
			       10);

	return DIV_TO_RATE(OSC_HZ, div);
}

static ulong rk3399_tsadc_set_clk(struct rockchip_cru *cru, uint hz)
{
	int src_clk_div;

	src_clk_div = DIV_ROUND_UP(OSC_HZ, hz) - 1;
	assert(src_clk_div <= 255);

	rk_clrsetreg(&cru->clksel_con[27],
		     CLK_TSADC_DIV_CON_MASK | CLK_TSADC_SEL_MASK,
		     (CLK_TSADC_SEL_X24M << CLK_TSADC_SEL_SHIFT) |
		     (src_clk_div << CLK_TSADC_DIV_CON_SHIFT));

	return rk3399_tsadc_get_clk(cru);
}

static ulong rk3399_crypto_get_clk(struct rk3399_clk_priv *priv, ulong clk_id)
{
	struct rockchip_cru *cru = priv->cru;
	u32 div, con, parent;

	switch (clk_id) {
	case SCLK_CRYPTO0:
		con = readl(&cru->clksel_con[24]);
		div = (con & CRYPTO0_DIV_MASK) >> CRYPTO0_DIV_SHIFT;
		parent = GPLL_HZ;
		break;
	case SCLK_CRYPTO1:
		con = readl(&cru->clksel_con[26]);
		div = (con & CRYPTO1_DIV_MASK) >> CRYPTO1_DIV_SHIFT;
		parent = GPLL_HZ;
		break;
	default:
		return -ENOENT;
	}

	return DIV_TO_RATE(parent, div);
}

static ulong rk3399_crypto_set_clk(struct rk3399_clk_priv *priv, ulong clk_id,
				   ulong hz)
{
	struct rockchip_cru *cru = priv->cru;
	int src_clk_div;

	src_clk_div = DIV_ROUND_UP(GPLL_HZ, hz);
	assert(src_clk_div - 1 <= 31);

	/*
	 * select gpll as crypto clock source and
	 * set up dependent divisors for crypto clocks.
	 */
	switch (clk_id) {
	case SCLK_CRYPTO0:
		rk_clrsetreg(&cru->clksel_con[24],
			     CRYPTO0_PLL_SEL_MASK | CRYPTO0_DIV_MASK,
			     CRYPTO_PLL_SEL_GPLL << CRYPTO0_PLL_SEL_SHIFT |
			     (src_clk_div - 1) << CRYPTO0_DIV_SHIFT);
		break;
	case SCLK_CRYPTO1:
		rk_clrsetreg(&cru->clksel_con[26],
			     CRYPTO1_PLL_SEL_MASK | CRYPTO1_DIV_MASK,
			     CRYPTO_PLL_SEL_GPLL << CRYPTO1_PLL_SEL_SHIFT |
			     (src_clk_div - 1) << CRYPTO1_DIV_SHIFT);
		break;
	default:
		printf("do not support this peri freq\n");
		return -EINVAL;
	}

	return rk3399_crypto_get_clk(priv, clk_id);
}

#ifndef CONFIG_SPL_BUILD
static ulong rk3399_peri_get_clk(struct rk3399_clk_priv *priv, ulong clk_id)
{
	struct rockchip_cru *cru = priv->cru;
	u32 div, con, parent;

	switch (clk_id) {
	case ACLK_PERIHP:
		con = readl(&cru->clksel_con[14]);
		div = (con & ACLK_PERIHP_DIV_CON_MASK) >>
		      ACLK_PERIHP_DIV_CON_SHIFT;
		parent = GPLL_HZ;
		break;
	case PCLK_PERIHP:
		con = readl(&cru->clksel_con[14]);
		div = (con & PCLK_PERIHP_DIV_CON_MASK) >>
		      PCLK_PERIHP_DIV_CON_SHIFT;
		parent = rk3399_peri_get_clk(priv, ACLK_PERIHP);
		break;
	case HCLK_PERIHP:
		con = readl(&cru->clksel_con[14]);
		div = (con & HCLK_PERIHP_DIV_CON_MASK) >>
		      HCLK_PERIHP_DIV_CON_SHIFT;
		parent = rk3399_peri_get_clk(priv, ACLK_PERIHP);
		break;
	case ACLK_PERILP0:
		con = readl(&cru->clksel_con[23]);
		div = (con & ACLK_PERILP0_DIV_CON_MASK) >>
		      ACLK_PERILP0_DIV_CON_SHIFT;
		parent = GPLL_HZ;
		break;
	case HCLK_PERILP0:
		con = readl(&cru->clksel_con[23]);
		div = (con & HCLK_PERILP0_DIV_CON_MASK) >>
		      HCLK_PERILP0_DIV_CON_SHIFT;
		parent = rk3399_peri_get_clk(priv, ACLK_PERILP0);
		break;
	case PCLK_PERILP0:
		con = readl(&cru->clksel_con[23]);
		div = (con & PCLK_PERILP0_DIV_CON_MASK) >>
		      PCLK_PERILP0_DIV_CON_SHIFT;
		parent = rk3399_peri_get_clk(priv, ACLK_PERILP0);
		break;
	case HCLK_PERILP1:
		con = readl(&cru->clksel_con[25]);
		div = (con & HCLK_PERILP1_DIV_CON_MASK) >>
		      HCLK_PERILP1_DIV_CON_SHIFT;
		parent = GPLL_HZ;
		break;
	case PCLK_PERILP1:
		con = readl(&cru->clksel_con[25]);
		div = (con & PCLK_PERILP1_DIV_CON_MASK) >>
		      PCLK_PERILP1_DIV_CON_SHIFT;
		parent = rk3399_peri_get_clk(priv, HCLK_PERILP1);
		break;
	default:
		return -ENOENT;
	}

	return DIV_TO_RATE(parent, div);
}

#endif

static ulong rk3399_clk_get_rate(struct clk *clk)
{
	struct rk3399_clk_priv *priv = dev_get_priv(clk->dev);
	ulong rate = 0;

	switch (clk->id) {
	case PLL_APLLL:
		rate = rockchip_pll_get_rate(&rk3399_pll_clks[APLLL], priv->cru, APLLL);
		break;
	case PLL_APLLB:
		rate = rockchip_pll_get_rate(&rk3399_pll_clks[APLLB], priv->cru, APLLB);
		break;
	case PLL_DPLL:
		rate = rockchip_pll_get_rate(&rk3399_pll_clks[DPLL], priv->cru, DPLL);
		break;
	case PLL_CPLL:
		rate = rockchip_pll_get_rate(&rk3399_pll_clks[CPLL], priv->cru, CPLL);
		break;
	case PLL_GPLL:
		rate = rockchip_pll_get_rate(&rk3399_pll_clks[GPLL], priv->cru, GPLL);
		break;
	case PLL_NPLL:
		rate = rockchip_pll_get_rate(&rk3399_pll_clks[NPLL], priv->cru, NPLL);
		break;
	case PLL_VPLL:
		rate = rockchip_pll_get_rate(&rk3399_pll_clks[VPLL], priv->cru, VPLL);
		break;
	case HCLK_SDMMC:
	case SCLK_SDMMC:
	case SCLK_EMMC:
		rate = rk3399_mmc_get_clk(priv->cru, clk->id);
		break;
	case SCLK_I2C1:
	case SCLK_I2C2:
	case SCLK_I2C3:
	case SCLK_I2C5:
	case SCLK_I2C6:
	case SCLK_I2C7:
		rate = rk3399_i2c_get_clk(priv->cru, clk->id);
		break;
	case SCLK_SPI0...SCLK_SPI5:
		rate = rk3399_spi_get_clk(priv->cru, clk->id);
		break;
	case SCLK_UART0:
	case SCLK_UART1:
	case SCLK_UART2:
	case SCLK_UART3:
		return 24000000;
	case PCLK_HDMI_CTRL:
		break;
	case DCLK_VOP0:
	case DCLK_VOP1:
		break;
	case PCLK_EFUSE1024NS:
		break;
	case SCLK_SARADC:
		rate = rk3399_saradc_get_clk(priv->cru);
		break;
	case SCLK_TSADC:
		rate = rk3399_tsadc_get_clk(priv->cru);
		break;
	case SCLK_CRYPTO0:
	case SCLK_CRYPTO1:
		rate = rk3399_crypto_get_clk(priv, clk->id);
		break;
#ifndef CONFIG_SPL_BUILD
	case ACLK_PERIHP:
	case HCLK_PERIHP:
	case PCLK_PERIHP:
	case ACLK_PERILP0:
	case HCLK_PERILP0:
	case PCLK_PERILP0:
	case HCLK_PERILP1:
	case PCLK_PERILP1:
		rate = rk3399_peri_get_clk(priv, clk->id);
		break;
#endif
	case ACLK_VIO:
	case ACLK_HDCP:
	case ACLK_GIC_PRE:
	case PCLK_DDR:
		break;
	case PCLK_ALIVE:
	case PCLK_WDT:
		rate = rk3399_alive_get_clk(priv->cru);
		break;
	default:
		log_debug("Unknown clock %lu\n", clk->id);
		return -ENOENT;
	}

	return rate;
}

static ulong rk3399_clk_set_rate(struct clk *clk, ulong rate)
{
	struct rk3399_clk_priv *priv = dev_get_priv(clk->dev);
	ulong ret = 0;

	switch (clk->id) {
	case 0 ... 63:
		return 0;

	case ACLK_PERIHP:
	case HCLK_PERIHP:
	case PCLK_PERIHP:
		return 0;

	case ACLK_PERILP0:
	case HCLK_PERILP0:
	case PCLK_PERILP0:
		return 0;

	case ACLK_CCI:
		return 0;

	case HCLK_PERILP1:
	case PCLK_PERILP1:
		return 0;

	case HCLK_SDMMC:
	case SCLK_SDMMC:
	case SCLK_EMMC:
		ret = rk3399_mmc_set_clk(priv->cru, clk->id, rate);
		break;
	case SCLK_MAC:
		ret = rk3399_gmac_set_clk(priv->cru, rate);
		break;
	case SCLK_I2C1:
	case SCLK_I2C2:
	case SCLK_I2C3:
	case SCLK_I2C5:
	case SCLK_I2C6:
	case SCLK_I2C7:
		ret = rk3399_i2c_set_clk(priv->cru, clk->id, rate);
		break;
	case SCLK_SPI0...SCLK_SPI5:
		ret = rk3399_spi_set_clk(priv->cru, clk->id, rate);
		break;
	case PCLK_HDMI_CTRL:
	case PCLK_VIO_GRF:
		/* the PCLK gates for video are enabled by default */
		break;
	case DCLK_VOP0:
	case DCLK_VOP1:
		ret = rk3399_vop_set_clk(priv->cru, clk->id, rate);
		break;
	case ACLK_VOP1:
	case HCLK_VOP1:
	case HCLK_SD:
	case SCLK_UPHY0_TCPDCORE:
	case SCLK_UPHY1_TCPDCORE:
		/**
		 * assigned-clocks handling won't require for vopl, so
		 * return 0 to satisfy clk_set_defaults during device probe.
		 */
		return 0;
	case SCLK_DDRCLK:
		ret = rk3399_ddr_set_clk(priv->cru, rate);
		break;
	case PCLK_EFUSE1024NS:
		break;
	case SCLK_SARADC:
		ret = rk3399_saradc_set_clk(priv->cru, rate);
		break;
	case SCLK_TSADC:
		ret = rk3399_tsadc_set_clk(priv->cru, rate);
		break;
	case SCLK_CRYPTO0:
	case SCLK_CRYPTO1:
		ret = rk3399_crypto_set_clk(priv, clk->id, rate);
		break;
	case ACLK_VIO:
	case ACLK_HDCP:
	case ACLK_GIC_PRE:
	case PCLK_DDR:
		return 0;
	default:
		log_debug("Unknown clock %lu\n", clk->id);
		return -ENOENT;
	}

	return ret;
}

static int __maybe_unused rk3399_gmac_set_parent(struct clk *clk,
						 struct clk *parent)
{
	struct rk3399_clk_priv *priv = dev_get_priv(clk->dev);
	const char *clock_output_name;
	int ret;

	/*
	 * If the requested parent is in the same clock-controller and
	 * the id is SCLK_MAC ("clk_gmac"), switch to the internal clock.
	 */
	if (parent->dev == clk->dev && parent->id == SCLK_MAC) {
		debug("%s: switching RGMII to SCLK_MAC\n", __func__);
		rk_clrreg(&priv->cru->clksel_con[19], BIT(4));
		return 0;
	}

	/*
	 * Otherwise, we need to check the clock-output-names of the
	 * requested parent to see if the requested id is "clkin_gmac".
	 */
	ret = dev_read_string_index(parent->dev, "clock-output-names",
				    parent->id, &clock_output_name);
	if (ret < 0)
		return -ENODATA;

	/* If this is "clkin_gmac", switch to the external clock input */
	if (!strcmp(clock_output_name, "clkin_gmac")) {
		debug("%s: switching RGMII to CLKIN\n", __func__);
		rk_setreg(&priv->cru->clksel_con[19], BIT(4));
		return 0;
	}

	return -EINVAL;
}

static int __maybe_unused rk3399_dclk_vop_set_parent(struct clk *clk,
						     struct clk *parent)
{
	struct rk3399_clk_priv *priv = dev_get_priv(clk->dev);
	void *dclkreg_addr;

	switch (clk->id) {
	case DCLK_VOP0_DIV:
		dclkreg_addr = &priv->cru->clksel_con[49];
		break;
	case DCLK_VOP1_DIV:
		dclkreg_addr = &priv->cru->clksel_con[50];
		break;
	default:
		return -EINVAL;
	}
	if (parent->id == PLL_CPLL) {
		rk_clrsetreg(dclkreg_addr, DCLK_VOP_PLL_SEL_MASK,
			     DCLK_VOP_PLL_SEL_CPLL << DCLK_VOP_PLL_SEL_SHIFT);
	} else {
		rk_clrsetreg(dclkreg_addr, DCLK_VOP_PLL_SEL_MASK,
			     DCLK_VOP_PLL_SEL_VPLL << DCLK_VOP_PLL_SEL_SHIFT);
	}

	return 0;
}

static int __maybe_unused rk3399_clk_set_parent(struct clk *clk,
						struct clk *parent)
{
	switch (clk->id) {
	case SCLK_RMII_SRC:
		return rk3399_gmac_set_parent(clk, parent);
	case DCLK_VOP0_DIV:
	case DCLK_VOP1_DIV:
		return rk3399_dclk_vop_set_parent(clk, parent);
	}

	debug("%s: unsupported clk %ld\n", __func__, clk->id);
	return -ENOENT;
}

static int rk3399_clk_enable(struct clk *clk)
{
	struct rk3399_clk_priv *priv = dev_get_priv(clk->dev);

	switch (clk->id) {
	case SCLK_MAC:
		rk_clrreg(&priv->cru->clkgate_con[5], BIT(5));
		break;
	case SCLK_MAC_RX:
		rk_clrreg(&priv->cru->clkgate_con[5], BIT(8));
		break;
	case SCLK_MAC_TX:
		rk_clrreg(&priv->cru->clkgate_con[5], BIT(9));
		break;
	case SCLK_MACREF:
		rk_clrreg(&priv->cru->clkgate_con[5], BIT(7));
		break;
	case SCLK_MACREF_OUT:
		rk_clrreg(&priv->cru->clkgate_con[5], BIT(6));
		break;
	case SCLK_USB2PHY0_REF:
		rk_clrreg(&priv->cru->clkgate_con[6], BIT(5));
		break;
	case SCLK_USB2PHY1_REF:
		rk_clrreg(&priv->cru->clkgate_con[6], BIT(6));
		break;
	case ACLK_GMAC:
		rk_clrreg(&priv->cru->clkgate_con[32], BIT(0));
		break;
	case PCLK_GMAC:
		rk_clrreg(&priv->cru->clkgate_con[32], BIT(2));
		break;
	case SCLK_USB3OTG0_REF:
		rk_clrreg(&priv->cru->clkgate_con[12], BIT(1));
		break;
	case SCLK_USB3OTG1_REF:
		rk_clrreg(&priv->cru->clkgate_con[12], BIT(2));
		break;
	case SCLK_USB3OTG0_SUSPEND:
		rk_clrreg(&priv->cru->clkgate_con[12], BIT(3));
		break;
	case SCLK_USB3OTG1_SUSPEND:
		rk_clrreg(&priv->cru->clkgate_con[12], BIT(4));
		break;
	case ACLK_USB3OTG0:
		rk_clrreg(&priv->cru->clkgate_con[30], BIT(1));
		break;
	case ACLK_USB3OTG1:
		rk_clrreg(&priv->cru->clkgate_con[30], BIT(2));
		break;
	case ACLK_USB3_RKSOC_AXI_PERF:
		rk_clrreg(&priv->cru->clkgate_con[30], BIT(3));
		break;
	case ACLK_USB3:
		rk_clrreg(&priv->cru->clkgate_con[12], BIT(0));
		break;
	case ACLK_USB3_GRF:
		rk_clrreg(&priv->cru->clkgate_con[30], BIT(4));
		break;
	case HCLK_HOST0:
		rk_clrreg(&priv->cru->clksel_con[20], BIT(5));
		break;
	case HCLK_HOST0_ARB:
		rk_clrreg(&priv->cru->clksel_con[20], BIT(6));
		break;
	case SCLK_USBPHY0_480M_SRC:
		return 0;
	case HCLK_HOST1:
		rk_clrreg(&priv->cru->clksel_con[20], BIT(7));
		break;
	case HCLK_HOST1_ARB:
		rk_clrreg(&priv->cru->clksel_con[20], BIT(8));
		break;
	case SCLK_USBPHY1_480M_SRC:
		return 0;
	case SCLK_UPHY0_TCPDPHY_REF:
		rk_clrreg(&priv->cru->clkgate_con[13], BIT(4));
		break;
	case SCLK_UPHY0_TCPDCORE:
		rk_clrreg(&priv->cru->clkgate_con[13], BIT(5));
		break;
	case SCLK_UPHY1_TCPDPHY_REF:
		rk_clrreg(&priv->cru->clkgate_con[13], BIT(6));
		break;
	case SCLK_UPHY1_TCPDCORE:
		rk_clrreg(&priv->cru->clkgate_con[13], BIT(7));
		break;
	case SCLK_PCIEPHY_REF:
		rk_clrreg(&priv->cru->clksel_con[18], BIT(10));
		break;
	default:
		debug("%s: unsupported clk %ld\n", __func__, clk->id);
		return -ENOENT;
	}

	return 0;
}

static int rk3399_clk_disable(struct clk *clk)
{
	struct rk3399_clk_priv *priv = dev_get_priv(clk->dev);

	switch (clk->id) {
	case SCLK_MAC:
		rk_setreg(&priv->cru->clkgate_con[5], BIT(5));
		break;
	case SCLK_MAC_RX:
		rk_setreg(&priv->cru->clkgate_con[5], BIT(8));
		break;
	case SCLK_MAC_TX:
		rk_setreg(&priv->cru->clkgate_con[5], BIT(9));
		break;
	case SCLK_MACREF:
		rk_setreg(&priv->cru->clkgate_con[5], BIT(7));
		break;
	case SCLK_MACREF_OUT:
		rk_setreg(&priv->cru->clkgate_con[5], BIT(6));
		break;
	case SCLK_USB2PHY0_REF:
		rk_setreg(&priv->cru->clkgate_con[6], BIT(5));
		break;
	case SCLK_USB2PHY1_REF:
		rk_setreg(&priv->cru->clkgate_con[6], BIT(6));
		break;
	case ACLK_GMAC:
		rk_setreg(&priv->cru->clkgate_con[32], BIT(0));
		break;
	case PCLK_GMAC:
		rk_setreg(&priv->cru->clkgate_con[32], BIT(2));
		break;
	case SCLK_USB3OTG0_REF:
		rk_setreg(&priv->cru->clkgate_con[12], BIT(1));
		break;
	case SCLK_USB3OTG1_REF:
		rk_setreg(&priv->cru->clkgate_con[12], BIT(2));
		break;
	case SCLK_USB3OTG0_SUSPEND:
		rk_setreg(&priv->cru->clkgate_con[12], BIT(3));
		break;
	case SCLK_USB3OTG1_SUSPEND:
		rk_setreg(&priv->cru->clkgate_con[12], BIT(4));
		break;
	case ACLK_USB3OTG0:
		rk_setreg(&priv->cru->clkgate_con[30], BIT(1));
		break;
	case ACLK_USB3OTG1:
		rk_setreg(&priv->cru->clkgate_con[30], BIT(2));
		break;
	case ACLK_USB3_RKSOC_AXI_PERF:
		rk_setreg(&priv->cru->clkgate_con[30], BIT(3));
		break;
	case ACLK_USB3:
		rk_setreg(&priv->cru->clkgate_con[12], BIT(0));
		break;
	case ACLK_USB3_GRF:
		rk_setreg(&priv->cru->clkgate_con[30], BIT(4));
		break;
	case HCLK_HOST0:
		rk_setreg(&priv->cru->clksel_con[20], BIT(5));
		break;
	case HCLK_HOST0_ARB:
		rk_setreg(&priv->cru->clksel_con[20], BIT(6));
		break;
	case HCLK_HOST1:
		rk_setreg(&priv->cru->clksel_con[20], BIT(7));
		break;
	case HCLK_HOST1_ARB:
		rk_setreg(&priv->cru->clksel_con[20], BIT(8));
		break;
	case SCLK_UPHY0_TCPDPHY_REF:
		rk_setreg(&priv->cru->clkgate_con[13], BIT(4));
		break;
	case SCLK_UPHY0_TCPDCORE:
		rk_setreg(&priv->cru->clkgate_con[13], BIT(5));
		break;
	case SCLK_UPHY1_TCPDPHY_REF:
		rk_setreg(&priv->cru->clkgate_con[13], BIT(6));
		break;
	case SCLK_UPHY1_TCPDCORE:
		rk_setreg(&priv->cru->clkgate_con[13], BIT(7));
		break;
	case SCLK_PCIEPHY_REF:
		rk_clrreg(&priv->cru->clksel_con[18], BIT(10));
		break;
	default:
		debug("%s: unsupported clk %ld\n", __func__, clk->id);
		return -ENOENT;
	}

	return 0;
}

static struct clk_ops rk3399_clk_ops = {
	.get_rate = rk3399_clk_get_rate,
	.set_rate = rk3399_clk_set_rate,
#if CONFIG_IS_ENABLED(OF_REAL)
	.set_parent = rk3399_clk_set_parent,
#endif
	.enable = rk3399_clk_enable,
	.disable = rk3399_clk_disable,
};

static void rkclk_init(struct rockchip_cru *cru)
{
	u32 aclk_div;
	u32 hclk_div;
	u32 pclk_div;

	rk3399_configure_cpu(cru, APLL_816_MHZ, CPU_CLUSTER_LITTLE);
	rk3399_configure_cpu(cru, APLL_816_MHZ, CPU_CLUSTER_BIG);

	/*
	 * some cru registers changed by bootrom, we'd better reset them to
	 * reset/default values described in TRM to avoid confusion in kernel.
	 * Please consider these three lines as a fix of bootrom bug.
	 */
	if (rockchip_pll_get_rate(&rk3399_pll_clks[NPLL], cru, NPLL) != NPLL_HZ)
		rockchip_pll_set_rate(&rk3399_pll_clks[NPLL], cru, NPLL, NPLL_HZ);

	if (rockchip_pll_get_rate(&rk3399_pll_clks[CPLL], cru, CPLL) != CPLL_HZ)
		rockchip_pll_set_rate(&rk3399_pll_clks[CPLL], cru, CPLL, CPLL_HZ);

	if (rockchip_pll_get_rate(&rk3399_pll_clks[GPLL], cru, GPLL) == GPLL_HZ)
		return;

	rk_clrsetreg(&cru->clksel_con[12], 0xffff, 0x4101);
	rk_clrsetreg(&cru->clksel_con[19], 0xffff, 0x033f);
	rk_clrsetreg(&cru->clksel_con[56], 0x0003, 0x0003);

	/* configure perihp aclk, hclk, pclk */
	aclk_div = DIV_ROUND_UP(GPLL_HZ, PERIHP_ACLK_HZ) - 1;

	hclk_div = PERIHP_ACLK_HZ / PERIHP_HCLK_HZ - 1;
	assert((hclk_div + 1) * PERIHP_HCLK_HZ <=
	       PERIHP_ACLK_HZ && (hclk_div <= 0x3));

	pclk_div = PERIHP_ACLK_HZ / PERIHP_PCLK_HZ - 1;
	assert((pclk_div + 1) * PERIHP_PCLK_HZ <=
	       PERIHP_ACLK_HZ && (pclk_div <= 0x7));

	rk_clrsetreg(&cru->clksel_con[14],
		     PCLK_PERIHP_DIV_CON_MASK | HCLK_PERIHP_DIV_CON_MASK |
		     ACLK_PERIHP_PLL_SEL_MASK | ACLK_PERIHP_DIV_CON_MASK,
		     pclk_div << PCLK_PERIHP_DIV_CON_SHIFT |
		     hclk_div << HCLK_PERIHP_DIV_CON_SHIFT |
		     ACLK_PERIHP_PLL_SEL_GPLL << ACLK_PERIHP_PLL_SEL_SHIFT |
		     aclk_div << ACLK_PERIHP_DIV_CON_SHIFT);

	/* configure perilp0 aclk, hclk, pclk */
	aclk_div = DIV_ROUND_UP(GPLL_HZ, PERILP0_ACLK_HZ) - 1;

	hclk_div = PERILP0_ACLK_HZ / PERILP0_HCLK_HZ - 1;
	assert((hclk_div + 1) * PERILP0_HCLK_HZ <=
	       PERILP0_ACLK_HZ && (hclk_div <= 0x3));

	pclk_div = PERILP0_ACLK_HZ / PERILP0_PCLK_HZ - 1;
	assert((pclk_div + 1) * PERILP0_PCLK_HZ <=
	       PERILP0_ACLK_HZ && (pclk_div <= 0x7));

	rk_clrsetreg(&cru->clksel_con[23],
		     PCLK_PERILP0_DIV_CON_MASK | HCLK_PERILP0_DIV_CON_MASK |
		     ACLK_PERILP0_PLL_SEL_MASK | ACLK_PERILP0_DIV_CON_MASK,
		     pclk_div << PCLK_PERILP0_DIV_CON_SHIFT |
		     hclk_div << HCLK_PERILP0_DIV_CON_SHIFT |
		     ACLK_PERILP0_PLL_SEL_GPLL << ACLK_PERILP0_PLL_SEL_SHIFT |
		     aclk_div << ACLK_PERILP0_DIV_CON_SHIFT);

	/* perilp1 hclk select gpll as source */
	hclk_div = DIV_ROUND_UP(GPLL_HZ, PERILP1_HCLK_HZ) - 1;
	assert((hclk_div + 1) * PERILP1_HCLK_HZ <=
	       GPLL_HZ && (hclk_div <= 0x1f));

	pclk_div = PERILP1_HCLK_HZ / PERILP1_PCLK_HZ - 1;
	assert((pclk_div + 1) * PERILP1_PCLK_HZ <=
	       PERILP1_HCLK_HZ && (pclk_div <= 0x7));

	rk_clrsetreg(&cru->clksel_con[25],
		     PCLK_PERILP1_DIV_CON_MASK | HCLK_PERILP1_DIV_CON_MASK |
		     HCLK_PERILP1_PLL_SEL_MASK,
		     pclk_div << PCLK_PERILP1_DIV_CON_SHIFT |
		     hclk_div << HCLK_PERILP1_DIV_CON_SHIFT |
		     HCLK_PERILP1_PLL_SEL_GPLL << HCLK_PERILP1_PLL_SEL_SHIFT);

	rk_clrsetreg(&cru->clksel_con[21],
		     ACLK_EMMC_PLL_SEL_MASK | ACLK_EMMC_DIV_CON_MASK,
		     ACLK_EMMC_PLL_SEL_GPLL << ACLK_EMMC_PLL_SEL_SHIFT |
		     (4 - 1) << ACLK_EMMC_DIV_CON_SHIFT);
	rk_clrsetreg(&cru->clksel_con[22], 0x3f << 0, 7 << 0);

	/*
	 * I2c MUx is in cpll by default, but cpll is for dclk_vop exclusive.
	 * If dclk_vop set rate after i2c init, the CPLL changed,
	 * but the i2c not perception, it will resulting the wrong
	 * frequency of the i2c.
	 * So set the i2c frequency according to the kernel configuration,
	 * and Hang I2C on the GPLL.
	 */
	rk_clrsetreg(&cru->clksel_con[61], I2C_CLK_REG_MASK(1),
		     I2C_CLK_REG_VALUE(1, 4));
	rk_clrsetreg(&cru->clksel_con[62], I2C_CLK_REG_MASK(2),
		     I2C_CLK_REG_VALUE(2, 4));
	rk_clrsetreg(&cru->clksel_con[63], I2C_CLK_REG_MASK(3),
		     I2C_CLK_REG_VALUE(3, 4));
	rk_clrsetreg(&cru->clksel_con[61], I2C_CLK_REG_MASK(5),
		     I2C_CLK_REG_VALUE(5, 4));
	rk_clrsetreg(&cru->clksel_con[62], I2C_CLK_REG_MASK(6),
		     I2C_CLK_REG_VALUE(6, 4));
	rk_clrsetreg(&cru->clksel_con[63], I2C_CLK_REG_MASK(7),
		     I2C_CLK_REG_VALUE(7, 4));

	rockchip_pll_set_rate(&rk3399_pll_clks[GPLL], cru, GPLL, GPLL_HZ);
}

static int rk3399_clk_probe(struct udevice *dev)
{
	struct rk3399_clk_priv *priv = dev_get_priv(dev);
	bool init_clocks = false;

#if CONFIG_IS_ENABLED(OF_PLATDATA)
	struct rk3399_clk_plat *plat = dev_get_plat(dev);

	priv->cru = map_sysmem(plat->dtd.reg[0], plat->dtd.reg[1]);
#endif

#if defined(CONFIG_SPL_BUILD)
	init_clocks = true;
#elif CONFIG_IS_ENABLED(HANDOFF)
	if (!(gd->flags & GD_FLG_RELOC)) {
		if (!(gd->spl_handoff))
			init_clocks = true;
	}
#endif

	priv->sync_kernel = false;
	if (!priv->armlclk_enter_hz)
		priv->armlclk_enter_hz =
		rockchip_pll_get_rate(&rk3399_pll_clks[APLLL], priv->cru, APLLL);
	if (!priv->armbclk_enter_hz)
		priv->armbclk_enter_hz =
		rockchip_pll_get_rate(&rk3399_pll_clks[APLLB], priv->cru, APLLB);

	if (init_clocks)
		rkclk_init(priv->cru);

	if (!priv->armlclk_init_hz)
		priv->armlclk_init_hz =
		rockchip_pll_get_rate(&rk3399_pll_clks[APLLL], priv->cru, APLLL);
	if (!priv->armbclk_init_hz)
		priv->armbclk_init_hz =
		rockchip_pll_get_rate(&rk3399_pll_clks[APLLB], priv->cru, APLLB);

	return 0;
}

static int rk3399_clk_of_to_plat(struct udevice *dev)
{
	if (CONFIG_IS_ENABLED(OF_REAL)) {
		struct rk3399_clk_priv *priv = dev_get_priv(dev);

		priv->cru = dev_read_addr_ptr(dev);
	}

	return 0;
}

static int rk3399_clk_bind(struct udevice *dev)
{
	int ret;
	struct udevice *sys_child;
	struct sysreset_reg *priv;

	/* The reset driver does not have a device node, so bind it here */
	ret = device_bind_driver(dev, "rockchip_sysreset", "sysreset",
				 &sys_child);
	if (ret) {
		debug("Warning: No sysreset driver: ret=%d\n", ret);
	} else {
		priv = malloc(sizeof(struct sysreset_reg));
		priv->glb_srst_fst_value = offsetof(struct rockchip_cru,
						    glb_srst_fst_value);
		priv->glb_srst_snd_value = offsetof(struct rockchip_cru,
						    glb_srst_snd_value);
		dev_set_priv(sys_child, priv);
	}

#if CONFIG_IS_ENABLED(RESET_ROCKCHIP)
	ret = offsetof(struct rockchip_cru, softrst_con[0]);
	ret = rockchip_reset_bind(dev, ret, 21);
	if (ret)
		debug("Warning: software reset driver bind faile\n");
#endif

	return 0;
}

static const struct udevice_id rk3399_clk_ids[] = {
	{ .compatible = "rockchip,rk3399-cru" },
	{ }
};

U_BOOT_DRIVER(clk_rk3399) = {
	.name		= "rockchip_rk3399_cru",
	.id		= UCLASS_CLK,
	.of_match	= rk3399_clk_ids,
	.priv_auto	= sizeof(struct rk3399_clk_priv),
	.of_to_plat = rk3399_clk_of_to_plat,
	.ops		= &rk3399_clk_ops,
	.bind		= rk3399_clk_bind,
	.probe		= rk3399_clk_probe,
#if CONFIG_IS_ENABLED(OF_PLATDATA)
	.plat_auto	= sizeof(struct rk3399_clk_plat),
#endif
};

static ulong rk3399_i2c_get_pmuclk(struct rk3399_pmucru *pmucru, ulong clk_id)
{
	u32 div, con;

	switch (clk_id) {
	case SCLK_I2C0_PMU:
		con = readl(&pmucru->pmucru_clksel[2]);
		div = I2C_CLK_DIV_VALUE(con, 0);
		break;
	case SCLK_I2C4_PMU:
		con = readl(&pmucru->pmucru_clksel[3]);
		div = I2C_CLK_DIV_VALUE(con, 4);
		break;
	case SCLK_I2C8_PMU:
		con = readl(&pmucru->pmucru_clksel[2]);
		div = I2C_CLK_DIV_VALUE(con, 8);
		break;
	default:
		printf("do not support this i2c bus\n");
		return -EINVAL;
	}

	return DIV_TO_RATE(PPLL_HZ, div);
}

static ulong rk3399_i2c_set_pmuclk(struct rk3399_pmucru *pmucru, ulong clk_id,
				   uint hz)
{
	int src_clk_div;

	src_clk_div = PPLL_HZ / hz;
	assert(src_clk_div - 1 < 127);

	switch (clk_id) {
	case SCLK_I2C0_PMU:
		rk_clrsetreg(&pmucru->pmucru_clksel[2], I2C_PMUCLK_REG_MASK(0),
			     I2C_PMUCLK_REG_VALUE(0, src_clk_div));
		break;
	case SCLK_I2C4_PMU:
		rk_clrsetreg(&pmucru->pmucru_clksel[3], I2C_PMUCLK_REG_MASK(4),
			     I2C_PMUCLK_REG_VALUE(4, src_clk_div));
		break;
	case SCLK_I2C8_PMU:
		rk_clrsetreg(&pmucru->pmucru_clksel[2], I2C_PMUCLK_REG_MASK(8),
			     I2C_PMUCLK_REG_VALUE(8, src_clk_div));
		break;
	default:
		printf("do not support this i2c bus\n");
		return -EINVAL;
	}

	return DIV_TO_RATE(PPLL_HZ, src_clk_div);
}

static ulong rk3399_pwm_get_clk(struct rk3399_pmucru *pmucru)
{
	u32 div, con;

	/* PWM closk rate is same as pclk_pmu */
	con = readl(&pmucru->pmucru_clksel[0]);
	div = con & PMU_PCLK_DIV_CON_MASK;

	return DIV_TO_RATE(PPLL_HZ, div);
}

static ulong rk3399_pmuclk_get_rate(struct clk *clk)
{
	struct rk3399_pmuclk_priv *priv = dev_get_priv(clk->dev);
	ulong rate = 0;

	switch (clk->id) {
	case PLL_PPLL:
		return PPLL_HZ;
	case PCLK_RKPWM_PMU:
	case PCLK_WDT_M0_PMU:
		rate = rk3399_pwm_get_clk(priv->pmucru);
		break;
	case SCLK_I2C0_PMU:
	case SCLK_I2C4_PMU:
	case SCLK_I2C8_PMU:
		rate = rk3399_i2c_get_pmuclk(priv->pmucru, clk->id);
		break;
	default:
		return -ENOENT;
	}

	return rate;
}

static ulong rk3399_pmuclk_set_rate(struct clk *clk, ulong rate)
{
	struct rk3399_pmuclk_priv *priv = dev_get_priv(clk->dev);
	ulong ret = 0;

	switch (clk->id) {
	case PLL_PPLL:
		/*
		 * This has already been set up and we don't want/need
		 * to change it here.  Accept the request though, as the
		 * device-tree has this in an 'assigned-clocks' list.
		 */
		return PPLL_HZ;
	case SCLK_I2C0_PMU:
	case SCLK_I2C4_PMU:
	case SCLK_I2C8_PMU:
		ret = rk3399_i2c_set_pmuclk(priv->pmucru, clk->id, rate);
		break;
	default:
		return -ENOENT;
	}

	return ret;
}

static struct clk_ops rk3399_pmuclk_ops = {
	.get_rate = rk3399_pmuclk_get_rate,
	.set_rate = rk3399_pmuclk_set_rate,
};

#ifndef CONFIG_SPL_BUILD
static void pmuclk_init(struct rk3399_pmucru *pmucru)
{
	u32 pclk_div;

	/*  configure pmu pll(ppll) */
	rockchip_pll_set_rate(&rk3399_pll_clks[PPLL], pmucru, PPLL, PPLL_HZ);

	/*  configure pmu pclk */
	pclk_div = PPLL_HZ / PMU_PCLK_HZ - 1;
	rk_clrsetreg(&pmucru->pmucru_clksel[0],
		     PMU_PCLK_DIV_CON_MASK,
		     pclk_div << PMU_PCLK_DIV_CON_SHIFT);
}
#endif

static int rk3399_pmuclk_probe(struct udevice *dev)
{
#if CONFIG_IS_ENABLED(OF_PLATDATA) || !defined(CONFIG_SPL_BUILD)
	struct rk3399_pmuclk_priv *priv = dev_get_priv(dev);
#endif

#if CONFIG_IS_ENABLED(OF_PLATDATA)
	struct rk3399_pmuclk_plat *plat = dev_get_plat(dev);

	priv->pmucru = map_sysmem(plat->dtd.reg[0], plat->dtd.reg[1]);
#endif

#ifndef CONFIG_SPL_BUILD
	pmuclk_init(priv->pmucru);
#endif
	return 0;
}

static int rk3399_pmuclk_of_to_plat(struct udevice *dev)
{
	if (CONFIG_IS_ENABLED(OF_REAL)) {
		struct rk3399_pmuclk_priv *priv = dev_get_priv(dev);

		priv->pmucru = dev_read_addr_ptr(dev);
	}

	return 0;
}

static int rk3399_pmuclk_bind(struct udevice *dev)
{
#if CONFIG_IS_ENABLED(RESET_ROCKCHIP)
	int ret;

	ret = offsetof(struct rk3399_pmucru, pmucru_softrst_con[0]);
	ret = rockchip_reset_bind(dev, ret, 2);
	if (ret)
		debug("Warning: software reset driver bind faile\n");
#endif
	return 0;
}

static const struct udevice_id rk3399_pmuclk_ids[] = {
	{ .compatible = "rockchip,rk3399-pmucru" },
	{ }
};

U_BOOT_DRIVER(rockchip_rk3399_pmuclk) = {
	.name		= "rockchip_rk3399_pmucru",
	.id		= UCLASS_CLK,
	.of_match	= rk3399_pmuclk_ids,
	.priv_auto	= sizeof(struct rk3399_pmuclk_priv),
	.of_to_plat = rk3399_pmuclk_of_to_plat,
	.ops		= &rk3399_pmuclk_ops,
	.probe		= rk3399_pmuclk_probe,
	.bind		= rk3399_pmuclk_bind,
#if CONFIG_IS_ENABLED(OF_PLATDATA)
	.plat_auto	= sizeof(struct rk3399_pmuclk_plat),
#endif
};

#ifndef CONFIG_SPL_BUILD
/**
 * soc_clk_dump() - Print clock frequencies
 * Returns zero on success
 *
 * Implementation for the clk dump command.
 */
int soc_clk_dump(void)
{
	struct udevice *cru_dev, *pmucru_dev;
	struct rk3399_clk_priv *priv;
	const struct rk3399_clk_info *clk_dump;
	struct clk clk;
	unsigned long clk_count = ARRAY_SIZE(clks_dump);
	unsigned long rate;
	int i, ret;

	ret = uclass_get_device_by_driver(UCLASS_CLK,
					  DM_DRIVER_GET(clk_rk3399),
					  &cru_dev);
	if (ret) {
		printf("%s failed to get cru device\n", __func__);
		return ret;
	}

	ret = uclass_get_device_by_driver(UCLASS_CLK,
					  DM_DRIVER_GET(rockchip_rk3399_pmuclk),
					  &pmucru_dev);
	if (ret) {
		printf("%s failed to get pmucru device\n", __func__);
		return ret;
	}

	priv = dev_get_priv(cru_dev);
	printf("CLK: (%s. arml: enter %lu KHz, init %lu KHz, kernel %lu%s)\n",
	       priv->sync_kernel ? "sync kernel" : "uboot",
	       priv->armlclk_enter_hz / 1000,
	       priv->armlclk_init_hz / 1000,
	       priv->set_armclk_rate ? priv->armlclk_hz / 1000 : 0,
	       priv->set_armclk_rate ? " KHz" : "N/A");
	printf("CLK: (%s. armb: enter %lu KHz, init %lu KHz, kernel %lu%s)\n",
	       priv->sync_kernel ? "sync kernel" : "uboot",
	       priv->armbclk_enter_hz / 1000,
	       priv->armbclk_init_hz / 1000,
	       priv->set_armclk_rate ? priv->armbclk_hz / 1000 : 0,
	       priv->set_armclk_rate ? " KHz" : "N/A");
	for (i = 0; i < clk_count; i++) {
		clk_dump = &clks_dump[i];
		if (clk_dump->name) {
			clk.id = clk_dump->id;
			if (clk_dump->is_cru)
				ret = clk_request(cru_dev, &clk);
			else
				ret = clk_request(pmucru_dev, &clk);
			if (ret < 0)
				return ret;

			rate = clk_get_rate(&clk);
			clk_free(&clk);
			if (i == 0) {
				if (rate < 0)
					printf("  %s %s\n", clk_dump->name,
					       "unknown");
				else
					printf("  %s %lu KHz\n", clk_dump->name,
					       rate / 1000);
			} else {
				if (rate < 0)
					printf("  %s %s\n", clk_dump->name,
					       "unknown");
				else
					printf("  %s %lu KHz\n", clk_dump->name,
					       rate / 1000);
			}
		}
	}

	return 0;
}
#endif
