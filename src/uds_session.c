/*
 * Copyright (c) 2026 borob-engineering
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief UDS Session Management and S3 Server Timeout Monitoring.
 *
 * This file implements the state tracking for diagnostic sessions (Service 0x10)
 * and manages a dedicated kernel timer to monitor tester connection keep-alives (Service 0x3E).
 * If the S3 timeout expires, the server automatically falls back to the default session.
 */

#include "uds_session.h"
#include "uds_security.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(uds_server, LOG_LEVEL_INF);

static uds_session_type_t current_session = UDS_SESSION_DEFAULT;
static struct k_timer s3_timer;

/**
 * @brief S3 timeout timer expiry callback. Resets the ECU to default diagnostic state.
 *
 * @param timer Pointer to the expiring kernel timer instance.
 */
static void s3_timer_expiry_cb(struct k_timer *timer)
{
	ARG_UNUSED(timer);
    
	if (current_session != UDS_SESSION_DEFAULT) {
		LOG_WRN("S3 Server timeout expired! No tester activity detected.");
		current_session = UDS_SESSION_DEFAULT;
		uds_security_reset_lock();
		LOG_INF("Automatically reverted to DEFAULT_SESSION and locked security access.");
	}
}

void uds_session_init(void)
{
	k_timer_init(&s3_timer, s3_timer_expiry_cb, NULL);
	current_session = UDS_SESSION_DEFAULT;
}

void uds_session_set(uds_session_type_t new_session)
{
	current_session = new_session;
	uds_session_refresh_timer();
}

uds_session_type_t uds_session_get(void)
{
	return current_session;
}

void uds_session_refresh_timer(void)
{
	if (current_session != UDS_SESSION_DEFAULT) {
		k_timer_start(&s3_timer, K_MSEC(CONFIG_UDS_S3_TIMEOUT_MS), K_NO_WAIT);
	} else {
		k_timer_stop(&s3_timer);
	}
}
