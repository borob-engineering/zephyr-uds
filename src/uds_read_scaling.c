/*
 * Copyright (c) 2026 borob-engineering
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Implementation of UDS Service 0x24 (Read Scaling Data By Identifier).
 */

#include "uds_app_interface.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <errno.h>

LOG_MODULE_DECLARE(uds_server, LOG_LEVEL_INF);

void map_nrc_from_errno(uint8_t sid, int err, void (*nrc_cb)(uint8_t, uint8_t))
{
	if (err == -ENOMEM) {
		nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
	} else if (err == -ENOENT) {
		nrc_cb(sid, UDS_NRC_REQUEST_OUT_OF_RANGE);
	} else {
		nrc_cb(sid, UDS_NRC_CONDITIONS_NOT_CORRECT);
	}
}

void uds_handle_read_scaling_data(uint8_t *req, size_t len, uint8_t *tx_buf,
				  void (*send_cb)(const uint8_t *, size_t),
				  void (*nrc_cb)(uint8_t, uint8_t))
{
	uint8_t sid;
	uint16_t did;
	size_t scaling_len;
	int ret;

	if (len == 0 || req == NULL) {
		return;
	}

	sid = req[0];
	scaling_len = 0;

	if (len < 3) {
		nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
		return;
	}

	did = ((uint16_t)req[1] << 8) | req[2];

	/* Formatiere die positive Antwort vor: [SID + 0x40] [DID MSB] [DID LSB] */
	tx_buf[0] = sid + 0x40;
	tx_buf[1] = req[1];
	tx_buf[2] = req[2];

	/* Hole die Skalierungsdaten ab Byte-Index 3 aus der Applikationsschicht */
	ret = uds_app_read_scaling_data(did, &tx_buf[3], &scaling_len, UDS_BUFF_SIZE - 3);

	if (ret == 0) {
		send_cb(tx_buf, 3 + scaling_len);
	} else {
		map_nrc_from_errno(sid, ret, nrc_cb);
	}
}
