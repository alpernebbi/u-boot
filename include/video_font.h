/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2000
 * Paolo Scaffardi, AIRVENT SAM s.p.a - RIMINI(ITALY), arsenio@tin.it
 */

#ifndef _VIDEO_FONT_
#define _VIDEO_FONT_

#ifdef CONFIG_VIDEO_FONT_4X6
#include <video_font_4x6.h>
#elif defined(CONFIG_VIDEO_FONT_SUN12X22)
#include <video_font_sun12x22.h>
#elif defined(CONFIG_VIDEO_FONT_TER16X32)
#include <video_font_ter16x32.h>
#else
#include <video_font_data.h>
#endif

#endif /* _VIDEO_FONT_ */
