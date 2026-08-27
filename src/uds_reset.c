/*
 * Copyright (c) 2026 borob-engineering
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief UDS Service 0x11 (ECU Reset) implementation with delayed sys_reboot for Zephyr v4.4.0.
 *
 * This file handles parsing reset requests and coordinates a delayed hardware/software
 * register reset using native kernel timers. This ensures the CAN controller can empty
 * its transmission buffers before the SoC reboots.
 */

#include "uds_reset.h"
#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(uds_server, LOG_LEVEL_INF);

#define UDS_RESET_DELAY_MS 50

static struct k_timer reset_timer;
static int chosen_reset_type = SYS_REBOOT_COLD;

/**
 * @brief Callback triggered when the execution delay timer expires.
 *
 * Calls the native Zephyr architecture reboot abstraction to reset the controller.
 *
 * @param timer Pointer to the expiring kernel timer instance.
 */
static void reset_timer_expiry_cb(struct k_timer *timer)
{
	ARG_UNUSED(timer);
	LOG_INF("Triggering system reboot via Zephyr kernel...");
    
	sys_reboot(chosen_reset_type);
}

void uds_reset_init(void)
{
	k_timer_init(&reset_timer, reset_timer_expiry_cb, NULL);
}

void uds_reset_handle(uint8_t *req, size_t len, uint8_t *tx_buf, 
                      void (*send_cb)(const uint8_t *, size_t), 
                      void (*nrc_cb)(uint8_t, uint8_t))
{
	uint8_t sid = req[0];
	uint8_t reset_type;

	if (len < 2) {
		nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
		return;
	}

	reset_type = req[1];

	switch (reset_type) {
	case 0x01: /* hardReset */
		chosen_reset_type = SYS_REBOOT_COLD;
		LOG_INF("UDS Hard Reset requested.");
		break;

	case 0x03: /* softReset */
		chosen_reset_type = SYS_REBOOT_WARM;
		LOG_INF("UDS Soft Reset requested.");
		break;

	default:
		/* Other reset types (e.g. keyOffOnReset 0x02) are not supported */
		nrc_cb(sid, UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED);
		return;
	}

	/* Assemble positive response payload */
	tx_buf[0] = sid + 0x40;
	tx_buf[1] = reset_type;

	/* Transmit positive response over network link */
	send_cb(tx_buf, 2);

	/* Schedule asynchronous reboot to grant transceiver transmission overhead buffer */
	k_timer_start(&reset_timer, K_MSEC(UDS_RESET_DELAY_MS), K_NO_WAIT);
}
