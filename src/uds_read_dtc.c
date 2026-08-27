/*
 * Copyright (c) 2026 borob-engineering
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Implementation of UDS Service 0x19 (Read DTC Information) via persistent NVS storage.
 */

#include "uds_read_dtc.h"
#include "uds_app_interface.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/kvss/nvs.h>
#include <string.h>
#include <errno.h>

LOG_MODULE_DECLARE(uds_server, LOG_LEVEL_INF);

#define NVS_DTC_START_ID 0x0100
#define NVS_DTC_MAX_ENTRIES 16
#define DTC_STATUS_AVAILABILITY_MASK 0x09

void uds_read_dtc_init(void)
{
	LOG_INF("UDS Persistent DTC Reader Engine initialized.");
}

void uds_read_dtc_handle(uint8_t *req, size_t len, uint8_t *tx_buf, 
                         void (*send_cb)(const uint8_t *, size_t), 
                         void (*nrc_cb)(uint8_t, uint8_t))
{
	uint8_t sid = req[0];
	uint8_t sub_function;
	uint8_t client_mask;
	size_t tx_idx;
	uint32_t req_dtc;
	uint8_t record_num;
	size_t out_len;
	struct nvs_fs *fs;
	uds_dtc_t temp_dtc;
	int ret;
	int i;

	if (len < 2) {
		nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
		return;
	}

	sub_function = req[1];
	fs = uds_app_get_nvs_context();

	if (fs == NULL) {
		nrc_cb(sid, UDS_NRC_CONDITIONS_NOT_CORRECT);
		return;
	}

	if (sub_function == 0x02) { /* reportDTCByStatusMask */
		if (len < 3) {
			nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
			return;
		}
		client_mask = req[2];
		tx_idx = 0;

		tx_buf[tx_idx++] = sid + 0x40;
		tx_buf[tx_idx++] = sub_function;
		tx_buf[tx_idx++] = DTC_STATUS_AVAILABILITY_MASK;

		for (i = 0; i < NVS_DTC_MAX_ENTRIES; i++) {
			ret = nvs_read(fs, NVS_DTC_START_ID + i, &temp_dtc, sizeof(temp_dtc));
			if (ret == sizeof(temp_dtc)) {
				if (temp_dtc.status != 0x00 && (temp_dtc.status & client_mask) != 0) {
					tx_buf[tx_idx++] = temp_dtc.code[0];
					tx_buf[tx_idx++] = temp_dtc.code[1];
					tx_buf[tx_idx++] = temp_dtc.code[2];
					tx_buf[tx_idx++] = temp_dtc.status;
				}
			}
		}
		send_cb(tx_buf, tx_idx);
		return;
	} 
	
	if (sub_function == 0x04) { /* reportDTCStoredDataByDTCNumber */
		if (len < 6) {
			nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
			return;
		}
		req_dtc = ((uint32_t)req[2] << 16) | ((uint32_t)req[3] << 8) | req[4];
		record_num = req[5];
		out_len = 0;

		tx_buf[0] = sid + 0x40;
		tx_buf[1] = sub_function;
		tx_buf[2] = req[2];
		tx_buf[3] = req[3];
		tx_buf[4] = req[4];

		ret = uds_app_get_freeze_frame(req_dtc, record_num, &tx_buf[5], &out_len, UDS_BUFF_SIZE - 5);
		if (ret == 0) {
			send_cb(tx_buf, 5 + out_len);
		} else {
			nrc_cb(sid, UDS_NRC_REQUEST_OUT_OF_RANGE);
		}
		return;
	}

	nrc_cb(sid, UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED);
}
