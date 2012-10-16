/*
 * Copyright (C) 2008 Samsung Electronics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ASM_ARCH_MX_HEADSET_H
#define __ASM_ARCH_MX_HEADSET_H

#ifdef __KERNEL__

enum {
	MX_JACK_NO_DEVICE		= 0x0,
	MX_HEADSET_4POLE		= 0x01 << 0,
	MX_HEADSET_3POLE		= 0x01 << 1,
	MX_UNKNOWN_DEVICE	= 0x01 << 8,
};

struct mx_jack_zone {
	unsigned int adc_high;
	unsigned int delay_ms;
	unsigned int check_count;
	unsigned int jack_type;
};

struct mx_jack_buttons_zone {
	unsigned int code;
	unsigned int adc_low;
	unsigned int adc_high;
};

struct mx_jack_platform_data {
	struct s3c_adc_client *padc;
	int (*set_micbias_state) (bool);
	struct mx_jack_zone *zones;
	struct mx_jack_buttons_zone *buttons_zones;
	int	num_zones;
	int	num_buttons_zones;
	int	det_gpio;
	int	send_end_gpio;
	bool det_active_high;
	bool send_end_active_high;
	int adc_channel;
};

#endif	/* __KERNEL__ */
#endif	/* __ASM_ARCH_MX_HEADSET_H */
