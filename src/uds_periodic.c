/*
 * Copyright (c) 2026 borob-engineering
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Implementation of UDS Service 0x2A (Read Data By Periodic Identifier) via dedicated kernel thread.
 *
 * Manages rapid, cyclic transmission of high-frequency diagnostic variables scheduled
 * inside an independent low-priority background thread execution loop.
 */

#include "uds_periodic.h"
#include "uds_types.h"
#include "uds_app_interface.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(uds_periodic, LOG_LEVEL_INF);

/** @brief Active list containing registered periodic diagnostic identifiers. */
static uint8_t active_periodic_dids[5];
/** @brief Current registration density of tracked periodic identifiers. */
static uint8_t active_did_count;
static void (*periodic_send_cb)(const uint8_t *, size_t);

/**
 * @brief Background kernel thread sampling and broadcasting active periodic DIDs.
 *
 * @param p1 Unused thread argument reference.
 * @param p2 Unused thread argument reference.
 * @param p3 Unused thread argument reference.
 */
static void uds_periodic_thread_handler(void *p1, void *p2, void *p3)
{
	uint8_t periodic_tx_buf[16];
	size_t data_len;
	int i;
	int ret;

	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	for (;;) {
		/* Cyclic sample rate interval cadence: 10 ms */
		k_msleep(10); 

		if (active_did_count == 0 || periodic_send_cb == NULL) {
			continue;
		}

		for (i = 0; i < active_did_count; i++) {
			periodic_tx_buf[0] = 0x2A;
			periodic_tx_buf[1] = active_periodic_dids[i];
			
			ret = uds_app_get_periodic_did(active_periodic_dids[i], &periodic_tx_buf[2], &data_len);
			if (ret == 0) {
				periodic_send_cb(periodic_tx_buf, 2 + data_len);
			}
		}
	}
}

K_THREAD_DEFINE(uds_periodic_id, 1024, uds_periodic_thread_handler, NULL, NULL, NULL, 6, 0, 0);

void uds_handle_periodic_request(uint8_t *req, size_t len,
				 void (*send_cb)(const uint8_t *, size_t),
				 void (*nrc_cb)(uint8_t, uint8_t))
{
	uint8_t sid = req[0];
	uint8_t transmission_mode;
	uint8_t stop_pos_buf[] = {0x6A, 0x04};
	uint8_t start_pos_buf[3];

	periodic_send_cb = send_cb;

	if (len < 3) {
		nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
		return;
	}

	transmission_mode = req[1];

	if (transmission_mode == 0x04) { /* stopTransmission */
		active_did_count = 0;
		stop_pos_buf[1] = transmission_mode;
		send_cb(stop_pos_buf, sizeof(stop_pos_buf));
		return;
	}

	if (transmission_mode == 0x01) { /* sendAtFastRate */
		if (active_did_count >= 5) {
			nrc_cb(sid, UDS_NRC_REQUEST_OUT_OF_RANGE);
			return;
		}
		active_periodic_dids[active_did_count++] = req[2];
		
		start_pos_buf[0] = 0x6A;
		start_pos_buf[1] = transmission_mode;
		start_pos_buf[2] = req[2];
		send_cb(start_pos_buf, sizeof(start_pos_buf));
	}
}
