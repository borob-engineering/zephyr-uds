/*
 * Copyright (c) 2026 borob-engineering
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief UDS Service 0x14 (Clear Diagnostic Information) with hardware NVS execution.
 */

#include "uds_clear_dtc.h"
#include "uds_app_interface.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/kvss/nvs.h>
#include <errno.h>

LOG_MODULE_DECLARE(uds_server, LOG_LEVEL_INF);

#define UDS_NRC78_PERIOD_MS 20

static struct k_work clear_dtc_work;
static struct k_timer nrc78_timer;

static uint32_t target_dtc_group;
static uint8_t stored_sid = 0x14;
static volatile bool worker_done;

static void (*stored_send_cb)(const uint8_t *, size_t);
static void (*stored_nrc_cb)(uint8_t, uint8_t);

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

static void clear_dtc_worker_handler(struct k_work *work)
{
	uint8_t local_tx_buf[] = {0x54};
	struct nvs_fs *fs;
	int ret;

	ARG_UNUSED(work);
	LOG_INF("Asynchronous persistent NVS DTC clearance loop active...");
	
	fs = uds_app_get_nvs_context();
	if (fs != NULL) {
		ret = uds_app_clear_persistent_dtcs(fs, target_dtc_group);
		if (ret != 0) {
			LOG_ERR("Application flash memory clearing failed: %d", ret);
		}
	}

	worker_done = true;
	k_timer_stop(&nrc78_timer);
	k_msleep(10);

	LOG_INF("Persistent clearance finished. Transmitting confirmation frame.");

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
