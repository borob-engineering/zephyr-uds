/*
 * Copyright (c) 2026 borob-engineering
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public interface definitions for the UDS Clear DTC service (0x14).
 */

#ifndef ZEPHYR_SRC_UDS_CLEAR_DTC_H_
#define ZEPHYR_SRC_UDS_CLEAR_DTC_H_

#include <stdint.h>
#include <stddef.h>
#include "uds_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the Clear DTC module.
 *
 * This function sets up the asynchronous kernel work objects and timers required
 * for handling Service 0x14 background processing and Response Pending (NRC 0x78) tracking.
 */
void uds_clear_dtc_init(void);

/**
 * @brief Central request handler for UDS Service 0x14 (Clear Diagnostic Information).
 *
 * Parses the incoming frame, validates formatting and execution state, and defers
 * the memory erasure task to the background system work queue if preconditions are met.
 *
 * @param req     Pointer to the raw received UDS request buffer payload.
 * @param len     Length of the request payload in bytes.
 * @param send_cb Pointer to the callback function for transmitting a positive response.
 * @param nrc_cb  Pointer to the callback function for transmitting a negative response code (NRC).
 */
void uds_clear_dtc_handle(uint8_t *req, size_t len,
			  void (*send_cb)(const uint8_t *, size_t),
			  void (*nrc_cb)(uint8_t, uint8_t));

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_SRC_UDS_CLEAR_DTC_H_ */
