/*
 * Copyright (C) 2010 Meizu, Inc.
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

 struct mx_rfkill_pd {
	int bt_rxd;//BT_RXD
	int bt_txd;//BT_TXD
	int bt_cts;//BT_CTS
	int bt_rts;//BT_RTS

	int bt_power;//BT_POWER
	int bt_reset;//BT_RESET
	int bt_wake;//BT_WAKE
	int bt_host_wake;//BT_HOST_WAKE

	int wifi_power;//WL_POWER
	int wifi_reset;//WL_RESET
	int wifi_cd;//WL_CD_PIN
 };
extern void bt_uart_wake_peer(struct uart_port *port);