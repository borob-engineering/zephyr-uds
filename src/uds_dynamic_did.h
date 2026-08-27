/*
 * Copyright (c) 2026 borob-engineering
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public interface definitions for the UDS Dynamic DID module (Service 0x2C).
 */

#ifndef ZEPHYR_SRC_UDS_DYNAMIC_DID_H_
#define ZEPHYR_SRC_UDS_DYNAMIC_DID_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Requests processing for defining a dynamic Data Identifier (Service 0x2C).
 *
 * Parses the incoming request frame, configures the internal mapping layout if formatting
 * matches the supported sub-functions, and transmits the positive response confirmation.
 *
 * @param req     Pointer to the raw received request byte stream.
 * @param len     Length of the incoming byte stream.
 * @param send_cb Callback reference to emit a positive response frame.
 * @param nrc_cb  Callback reference to emit an error state code (NRC).
 */
void uds_handle_define_dynamic_did(uint8_t *req, size_t len,
				   void (*send_cb)(const uint8_t *, size_t),
				   void (*nrc_cb)(uint8_t, uint8_t));

/**
 * @brief Constructs the composite data payload of a configured dynamic DID.
 *
 * Cycles through all configured mapping entries, extracts the respective slices via the
 * application layer DID interface, and aggregates them into the target output buffer.
 *
 * @param did     The requested dynamic 16-bit DID to assemble.
 * @param buf_out Output buffer where sliced source segments should be stored.
 * @param len_out Pointer to write the accumulated size of the compiled payload.
 *
 * @retval 0       On success.
 * @retval -ENOENT Requested identifier is not configured or maps to an inactive entry.
 */
int uds_read_dynamic_did_payload(uint16_t did, uint8_t *buf_out, size_t *len_out);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_SRC_UDS_DYNAMIC_DID_H_ */
