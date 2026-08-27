/*
 * Copyright (c) 2026 borob-engineering
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Implementation of UDS Service 0x2C (Dynamically Define Data Identifier).
 *
 * Provides functions to dynamically group parts of existing Data Identifiers (DIDs)
 * into a single custom dynamic DID structure, accelerating structured data retrieval.
 */

#include <zephyr/kernel.h>
#include <string.h>
#include <errno.h>
#include "uds_types.h"
#include "uds_app_interface.h"

/**
 * @brief Representation of a single dynamic DID source slicing reference.
 */
typedef struct {
	/** Source Data Identifier to slice bytes from */
	uint16_t source_did;
	/** 1-based start byte position within the source data array */
	uint8_t position;
	/** Number of sequential bytes to copy from the source data */
	uint8_t length;
} dynamic_entry_t;

/**
 * @brief Map layout connecting a dynamic DID to its underlying source segments.
 */
typedef struct {
	/** The newly created 16-bit virtual Dynamic Data Identifier */
	uint16_t dyn_did;
	/** Array containing source slice mapping configuration rules */
	dynamic_entry_t entries[4];
	/** Count of active slice mapping entries assigned to this DID */
	uint8_t entry_count;
} dynamic_did_map_t;

/** @brief Active volatile dynamic DID configuration mapping table. */
static dynamic_did_map_t dyn_map;

/**
 * @brief Requests processing for defining a dynamic Data Identifier (Service 0x2C).
 *
 * @param req     Pointer to the raw received request byte stream.
 * @param len     Length of the incoming byte stream.
 * @param send_cb Callback reference to emit a positive response frame.
 * @param nrc_cb  Callback reference to emit an error state code.
 */
void uds_handle_define_dynamic_did(uint8_t *req, size_t len,
				   void (*send_cb)(const uint8_t *, size_t),
				   void (*nrc_cb)(uint8_t, uint8_t))
{
	uint8_t sid = req[0];
	uint8_t sub_function;
	uint8_t pos_buf[4];

	if (len < 9) {
		nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
		return;
	}

	sub_function = req[1];
	if (sub_function != 0x01) {
		nrc_cb(sid, UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED);
		return;
	}

	dyn_map.dyn_did = ((uint16_t)req[2] << 8) | req[3];
	dyn_map.entries[0].source_did = ((uint16_t)req[4] << 8) | req[5];
	dyn_map.entries[0].position = req[6];
	dyn_map.entries[0].length = req[7];
	dyn_map.entry_count = 1;

	pos_buf[0] = 0x6C; /* SID 0x2C + 0x40 */
	pos_buf[1] = sub_function;
	pos_buf[2] = req[2];
	pos_buf[3] = req[3];

	send_cb(pos_buf, sizeof(pos_buf));
}

/**
 * @brief Constructs the composite data payload of a configured dynamic DID.
 *
 * @param did     The requested dynamic 16-bit DID to assemble.
 * @param buf_out Output buffer where sliced source segments should be stored.
 * @param len_out Pointer to write the accumulated size of the compiled payload.
 *
 * @retval 0       On success.
 * @retval -ENOENT Requested identifier is not configured or maps to an inactive entry.
 */
int uds_read_dynamic_did_payload(uint16_t did, uint8_t *buf_out, size_t *len_out)
{
	size_t total_len = 0;
	uint8_t temp_src_buf[64];
	size_t src_len;
	int i;
	int ret;

	if (dyn_map.dyn_did != did || dyn_map.entry_count == 0) {
		return -ENOENT;
	}

	for (i = 0; i < dyn_map.entry_count; i++) {
		ret = uds_app_read_did(dyn_map.entries[i].source_did, temp_src_buf,
				       &src_len, sizeof(temp_src_buf));
		if (ret == 0) {
			memcpy(&buf_out[total_len],
			       &temp_src_buf[dyn_map.entries[i].position - 1],
			       dyn_map.entries[i].length);
			total_len += dyn_map.entries[i].length;
		}
	}

	*len_out = total_len;
	return 0;
}
