/*
 * Copyright (c) 2026 borob-engineering
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief UDS Service 0x14 (Clear Diagnostic Information) with cyclic NRC 0x78 handling.
 *
 * This file handles the asynchronous memory clearing process via the Zephyr system workqueue
 * and manages a kernel timer to periodically send Response Pending (NRC 0x78) messages
 * to prevent tester timeouts.
 */

#include "uds_clear_dtc.h"
#include "uds_read_dtc.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(uds_server, LOG_LEVEL_INF);

#define UDS_NRC78_PERIOD_MS 20

static struct k_work clear_dtc_work;
static struct k_timer nrc78_timer;

static uint32_t target_dtc_group;
static uint8_t stored_sid = 0x14;

/** @brief Atomic flag to prevent race conditions during timer shutdown. */
static volatile bool worker_done;

static void (*stored_send_cb)(const uint8_t *, size_t);
static void (*stored_nrc_cb)(uint8_t, uint8_t);

/**
 * @brief One-Shot Timer callback. Reschedules itself dynamically if needed.
 *
 * @param timer Pointer to the expiring kernel timer instance.
 */
static void nrc78_timer_expiry_cb(struct k_timer *timer)
{
	ARG_UNUSED(timer);
	
	if (worker_done) {
		return;
	}

	if (stored_nrc_cb != NULL) {
		stored_nrc_cb(stored_sid, UDS_NRC_RESPONSE_PENDING);
	}

	if (!worker_done) {
		k_timer_start(&nrc78_timer, K_MSEC(UDS_NRC78_PERIOD_MS), K_NO_WAIT);
	}
}

/**
 * @brief Asynchronous Flash erasure worker running inside the system workqueue context.
 *
 * @param work Pointer to the triggered work queue tracking structure.
 */
static void clear_dtc_worker_handler(struct k_work *work)
{
	uint8_t local_tx_buf[1] = {0x54}; /* SID 0x14 + 0x40 */

	ARG_UNUSED(work);
	LOG_INF("Background flash deletion active...");
	
	/* Simulate real intensive physical flash hardware erasure overhead */
	k_msleep(1200); 

	/* Clean up the internal RAM-based DTC database */
	uds_read_dtc_clear_all(target_dtc_group);

	/* Safe module shutdown sequence: lock flag and stop timer execution loop */
	worker_done = true;
	k_timer_stop(&nrc78_timer);

	/* Provide the CAN transceiver buffer stack a brief breathing window */
	k_msleep(10);

	LOG_INF("Erasure finished. Sending final positive UDS response.");

	if (stored_send_cb != NULL) {
		stored_send_cb(local_tx_buf, sizeof(local_tx_buf));
	}
}

void uds_clear_dtc_init(void)
{
	k_work_init(&clear_dtc_work, clear_dtc_worker_handler);
	k_timer_init(&nrc78_timer, nrc78_timer_expiry_cb, NULL);
}

void uds_clear_dtc_handle(uint8_t *req, size_t len, 
                          void (*send_cb)(const uint8_t *, size_t), 
                          void (*nrc_cb)(uint8_t, uint8_t))
{
	uint8_t sid = req[0];
	
	if (len != 4) {
		nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
		return;
	}

	if (k_work_is_pending(&clear_dtc_work)) {
		nrc_cb(sid, UDS_NRC_CONDITIONS_NOT_CORRECT);
		return;
	}

	target_dtc_group = ((uint32_t)req[1] << 16) | 
	                   ((uint32_t)req[2] << 8)  | 
	                   ((uint32_t)req[3]);

	stored_send_cb = send_cb;
	stored_nrc_cb = nrc_cb;
	stored_sid = sid;

	worker_done = false;

	k_timer_start(&nrc78_timer, K_MSEC(UDS_NRC78_PERIOD_MS), K_NO_WAIT);
	k_work_submit(&clear_dtc_work);
}
