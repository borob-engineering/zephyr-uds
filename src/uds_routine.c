/*
 * Copyright (c) 2026 borob-engineering
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Implementation of UDS Service 0x31 (Routine Control) with cyclic NRC 0x78 handling.
 *
 * This file orchestrates long-running application-specific test routines asynchronously
 * using the Zephyr system workqueue while maintaining standard-compliant response-pending mechanics.
 */

#include "uds_routine.h"
#include "uds_session.h"
#include "uds_app_interface.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_DECLARE(uds_server, LOG_LEVEL_INF);

#define UDS_ROUTINE_NRC78_PERIOD_MS 20

static volatile routine_status_t erase_routine_status = ROUTINE_IDLE;
static uint8_t routine_exit_info;
static uint16_t active_routine_id;

static struct k_work routine_work;
static struct k_timer routine_nrc78_timer;

static uint8_t local_routine_tx_buf[5];
static uint8_t stored_routine_sid = 0x31;

static void (*stored_routine_send_cb)(const uint8_t *, size_t);
static void (*stored_routine_nrc_cb)(uint8_t, uint8_t);

/**
 * @brief Cyclic timer callback issuing standard response-pending (NRC 0x78) frames.
 *
 * @param timer Pointer to the expiring kernel timer instance.
 */
static void routine_nrc78_timer_cb(struct k_timer *timer)
{
	ARG_UNUSED(timer);

	if (stored_routine_nrc_cb != NULL) {
		stored_routine_nrc_cb(stored_routine_sid, UDS_NRC_RESPONSE_PENDING);
	}
}

/**
 * @brief Asynchronous routine worker running inside the system workqueue context.
 *
 * @param work Pointer to the triggered work queue tracking structure.
 */
static void routine_worker_handler(struct k_work *work)
{
	uint8_t app_info = 0;
	int ret;

	ARG_UNUSED(work);
	
	/* Execute long-running hardware or flash layout validation routine */
	ret = uds_app_routine_start(active_routine_id, &app_info);

	if (ret == 0) {
		erase_routine_status = ROUTINE_COMPLETED;
		routine_exit_info = app_info;
	} else {
		erase_routine_status = ROUTINE_FAILED;
		routine_exit_info = 0x0F;
	}

	/* Stop the cyclic response pending supervisor timer immediately upon completion */
	k_timer_stop(&routine_nrc78_timer);

	if (stored_routine_send_cb != NULL) {
		local_routine_tx_buf[0] = 0x71; /* SID 0x31 + 0x40 */
		local_routine_tx_buf[1] = 0x01; /* startRoutine */
		local_routine_tx_buf[2] = (uint8_t)(active_routine_id >> 8);
		local_routine_tx_buf[3] = (uint8_t)(active_routine_id & 0xFF);
		local_routine_tx_buf[4] = (erase_routine_status == ROUTINE_COMPLETED) ? 0x00 : 0x02;
		
		stored_routine_send_cb(local_routine_tx_buf, 5);
	}
}

void uds_routine_init(void)
{
	k_work_init(&routine_work, routine_worker_handler);
	k_timer_init(&routine_nrc78_timer, routine_nrc78_timer_cb, NULL);
}

void uds_routine_handle_control(uint8_t *req, size_t len, 
                                void (*send_cb)(const uint8_t *, size_t), 
                                void (*nrc_cb)(uint8_t, uint8_t))
{
	uint8_t sid;
	uint8_t sub_function;
	uint16_t routine_id;
	uint8_t app_status;
	uint8_t app_exit;
	uint8_t sync_tx_buf[6];
	int ret;

	if (len == 0 || req == NULL) {
		return;
	}

	sid = req[0];

	if (uds_session_get() == UDS_SESSION_DEFAULT) {
		nrc_cb(sid, UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED_IN_ACTIVE_SESS);
		return;
	}

	if (len < 4) {
		nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
		return;
	}

	sub_function = req[1];
	routine_id = ((uint16_t)req[2] << 8) | req[3];

	switch (sub_function) {
	case 0x01: /* startRoutine */
		if (erase_routine_status == ROUTINE_RUNNING) {
			nrc_cb(sid, UDS_NRC_CONDITIONS_NOT_CORRECT);
			return;
		}
		
		active_routine_id = routine_id;
		erase_routine_status = ROUTINE_RUNNING;
		
		stored_routine_send_cb = send_cb;
		stored_routine_nrc_cb = nrc_cb;
		stored_routine_sid = sid;

		/* Fire the periodic NRC 0x78 supervisor layout timer loop */
		k_timer_start(&routine_nrc78_timer, K_NO_WAIT, K_MSEC(UDS_ROUTINE_NRC78_PERIOD_MS));
		
		k_work_submit(&routine_work);
		break;

	case 0x03: /* requestRoutineResults */
		app_status = 0;
		app_exit = 0;
		ret = uds_app_routine_request_results(routine_id, &app_status, &app_exit);
		if (ret != 0) {
			nrc_cb(sid, UDS_NRC_REQUEST_OUT_OF_RANGE);
			return;
		}
			
		sync_tx_buf[0] = sid + 0x40;
		sync_tx_buf[1] = sub_function;
		sync_tx_buf[2] = req[2];
		sync_tx_buf[3] = req[3];
		sync_tx_buf[4] = app_status;
		sync_tx_buf[5] = app_exit;
			
		send_cb(sync_tx_buf, 6);
		break;

	default:
		nrc_cb(sid, UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED);
		break;
	}
}
