/* arch/arm/mach-exynos/mx_debug_show_irq.c
 *
 * Copyright (C) 2012 Meizu Technology Co.Ltd, Zhuhai, China
 *
 * Author:  Lvcha qiu <lvcha@meizu.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
 
static unsigned long mx_wakeup_type;

unsigned long mx_get_wakeup_type(void) {return mx_wakeup_type;}

static unsigned long mx_set_wakeup_type(unsigned long mask)
{
	return mask ? (mx_wakeup_type |= mask) :
				  (mx_wakeup_type = mask);
}

static inline void m030_set_wakeup_type(mx_int_type group, int pending)
{
	switch(group) {
	case EINT_GROUP0:
		if (pending & (1 << 4))
			mx_set_wakeup_type(MX_LOWBAT_WAKE);
		if (pending & (1 << 5))
			mx_set_wakeup_type(MX_CHARG_WAKE | MX_ALARM_WAKE);
		break;
	case EINT_GROUP1:
		if (pending & (1 << 0))
			mx_set_wakeup_type(MX_WIFI_WAKE);
		if (pending & (1 << 1))
			mx_set_wakeup_type(MX_BLUETOOTH_WAKE);
		if (pending & (1 << 3))
			mx_set_wakeup_type(MX_MODEM_RST_WAKE);
		if (pending & (1 << 6))
			mx_set_wakeup_type(MX_MODEM_WAKE);
		break;
	case EINT_GROUP2:
		if (pending & (1 << 1))
			mx_set_wakeup_type(MX_JACK_WAKE);
		if (pending & (1 << 3))
			mx_set_wakeup_type(MX_USB_HOST_WAKE);
		if (pending & (1 << 5))
			mx_set_wakeup_type(MX_USB_WAKE);
		if (pending & (1 << 6))
			mx_set_wakeup_type(MX_PLUS_KEY_WAKE);
		break;
	case EINT_GROUP3:
		if (pending & (1 << 0))
			mx_set_wakeup_type(MX_MINUS_KEY_WAKE);
		if (pending & (1 << 1))
			mx_set_wakeup_type(MX_KEY_POWER_WAKE);
		if (pending & (1 << 3))
			mx_set_wakeup_type(MX_KEY_HOME_WAKE);
		if (pending & (1 << 4))
			mx_set_wakeup_type(MX_IR_WAKE);
		break;
	case OTHER_INT:
		if (pending & S5P_WAKEUP_STAT_RTCALARM)
			mx_set_wakeup_type(MX_ALARM_WAKE);
		else if (pending & S5P_WAKEUP_STAT_RTCALARM)
			mx_set_wakeup_type(MX_TICK_WAKE);
		else if (pending & S5P_WAKEUP_STAT_AUDIO)
			mx_set_wakeup_type(MX_I2S_WAKE);
		else if (pending & S5P_WAKEUP_STAT_SYSTIMER)
			mx_set_wakeup_type(MX_SYSTIMER_WAKE);
	default:
		break;
	}
}

static inline void m032_set_wakeup_type(mx_int_type group, int pending)
{
	switch(group) {
	case EINT_GROUP0:
		if (pending & (1 << 0))
			mx_set_wakeup_type(MX_KEY_POWER_WAKE);
		if (pending & (1 << 2))
			mx_set_wakeup_type(MX_KEY_HOME_WAKE);
		if (pending & (1 << 3))
			mx_set_wakeup_type(MX_USB_WAKE);
		if (pending & (1 << 4))
			mx_set_wakeup_type(MX_ALARM_WAKE);
		if (pending & (1 << 5))
			mx_set_wakeup_type(MX_CHARG_WAKE);
		if (pending & (1 << 6))
			mx_set_wakeup_type(MX_IR_WAKE);
		break;
	case EINT_GROUP1:
		if (pending & (1 << 2))
			mx_set_wakeup_type(MX_USB_HOST_WAKE);//USB HOST IRQ
		if (pending & (1 << 7))
			mx_set_wakeup_type(MX_JACK_WAKE);
		break;
	case EINT_GROUP2:
		if (pending & (1 << 0))
			mx_set_wakeup_type(MX_MINUS_KEY_WAKE);
		if (pending & (1 << 1))
			mx_set_wakeup_type(MX_LOWBAT_WAKE);
		if (pending & (1 << 3))
			mx_set_wakeup_type(MX_WIFI_WAKE);
		if (pending & (1 << 4))
			mx_set_wakeup_type(MX_BLUETOOTH_WAKE);
		if (pending & (1 << 5))
			mx_set_wakeup_type(MX_PLUS_KEY_WAKE);
		break;
	case EINT_GROUP3:
		if (pending & (1 << 0))
			mx_set_wakeup_type(MX_MODEM_RST_WAKE);
		if (pending & (1 << 5))
			mx_set_wakeup_type(MX_MODEM_WAKE);
		break;
	case OTHER_INT:
		if (pending & S5P_WAKEUP_STAT_RTCALARM) {
			mx_set_wakeup_type(MX_ALARM_WAKE);
		} else if (pending & S5P_WAKEUP_STAT_RTCTICK) {
			mx_set_wakeup_type(MX_TICK_WAKE);
		} else if (pending & S5P_WAKEUP_STAT_AUDIO) {
			mx_set_wakeup_type(MX_I2S_WAKE);
		} else if (pending & S5P_WAKEUP_STAT_SYSTIMER) {
			mx_set_wakeup_type(MX_SYSTIMER_WAKE);
		}
	default:
		break;
	}
}

static inline void set_wakeup_type(mx_int_type group, int pending)
{
	if(machine_is_m030())
		m030_set_wakeup_type(group, pending);
	else if (machine_is_m032() || machine_is_m031())
		m032_set_wakeup_type(group, pending);
	else
		pr_err("Unknow machine type\n");
}

static void mx_show_wakeup_name(void)
{
	int len;
	char wakeup_str[256] = {0,};

	len = sprintf(wakeup_str, "%s", "MX Wakeup By:");

	if(mx_wakeup_type & MX_USB_WAKE)
		len +=  sprintf(wakeup_str+len, " USB_WAKE");
	if(mx_wakeup_type & MX_LOWBAT_WAKE)
		len +=  sprintf(wakeup_str+len, " LOWBAT_WAKE");
	if(mx_wakeup_type & MX_CHARG_WAKE)
		len +=  sprintf(wakeup_str+len, " CHARG_WAKE");
	if(mx_wakeup_type & MX_WIFI_WAKE)
		len +=  sprintf(wakeup_str+len, " WIFI_WAKE");
	if(mx_wakeup_type & MX_BLUETOOTH_WAKE)
		len +=  sprintf(wakeup_str+len, " BLUETOOTH_WAKE");
	if(mx_wakeup_type & MX_MODEM_RST_WAKE)
		len +=  sprintf(wakeup_str+len, " MODEM_RST_WAKE");
	if(mx_wakeup_type & MX_MODEM_WAKE)
		len +=  sprintf(wakeup_str+len, " MODEM_WAKE");
	if(mx_wakeup_type & MX_KEY_POWER_WAKE)
		len +=  sprintf(wakeup_str+len, " KEY_POWER_WAKE");
	if(mx_wakeup_type & MX_KEY_HOME_WAKE)
		len +=  sprintf(wakeup_str+len, " KEY_HOME_WAKE");
	if(mx_wakeup_type & MX_USB_HOST_WAKE)
		len +=  sprintf(wakeup_str+len, " MX_USB_HOST_WAKE");
	if(mx_wakeup_type & MX_ALARM_WAKE)
		len +=  sprintf(wakeup_str+len, " ALARM_WAKE");
	if(mx_wakeup_type & MX_TICK_WAKE)
		len +=  sprintf(wakeup_str+len, " TICK_WAKE");
	if(mx_wakeup_type & MX_I2S_WAKE)
		len +=  sprintf(wakeup_str+len, " I2S_WAKE");
	if(mx_wakeup_type & MX_SYSTIMER_WAKE)
		len +=  sprintf(wakeup_str+len, " SYSTIMER_WAKE");
	if(mx_wakeup_type & MX_MINUS_KEY_WAKE)
		len +=  sprintf(wakeup_str+len, " VOLUMEDOWN_KEY_WAKE");
	if(mx_wakeup_type & MX_PLUS_KEY_WAKE)
		len +=  sprintf(wakeup_str+len, " VOLUMEUP_KEY_WAKE");
	if(mx_wakeup_type & MX_JACK_WAKE)
		len +=  sprintf(wakeup_str+len, " JACK_WAKE");
	if(!mx_wakeup_type)
		len +=  sprintf(wakeup_str+len, " UNKNOW_WAKE");

	pr_info("%s\n", wakeup_str);
}

static void mx_record_pending(void)
{
 	unsigned int state;

	/* get wakeup info to handle */
	state = __raw_readl(S5P_WAKEUP_STAT);
	if (state & S5P_WAKEUP_STAT_EINT) {
		int eint_pend, i;
		for (i=EINT_GROUP0; i<=EINT_GROUP3; i++) {
			eint_pend = __raw_readl(S5P_EINT_PEND(i))&0xff;
			if (eint_pend)
				set_wakeup_type(i, eint_pend);
		}
	}

	if (state & ~S5P_WAKEUP_STAT_EINT)
		set_wakeup_type(OTHER_INT, state);
 }
