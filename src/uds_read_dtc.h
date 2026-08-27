/*
 * Copyright (c) 2026 borob-engineering
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public interface definitions for the UDS Read DTC service (0x19).
 */

#ifndef ZEPHYR_SRC_UDS_READ_DTC_H_
#define ZEPHYR_SRC_UDS_READ_DTC_H_

#include <stdint.h>
#include <stddef.h>
#include "uds_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the internal RAM-based mock DTC database.
 *
 * populates the internal fault memory layout with predefined sample Diagnostic
 * Trouble Codes and status bitmasks during system boot.
 */
void uds_read_dtc_init(void);

/**
 * @brief Central request handler for UDS Service 0x19 (Read DTC Information).
 *
 * Parses incoming sub-functions like reportDTCByStatusMask (0x02) or
 * reportDTCStoredDataByDTCNumber (0x04) and extracts active fault data or freeze frames.
 *
 * @param req     Pointer to the raw received request buffer payload.
 * @param len     Length of the request payload in bytes.
 * @param tx_buf  Pointer to the shared scratchpad transmission buffer.
 * @param send_cb Pointer to the callback function for transmitting a positive response.
 * @param nrc_cb  Pointer to the callback function for transmitting a negative response code (NRC).
 */
void uds_read_dtc_handle(uint8_t *req, size_t len, uint8_t *tx_buf, 
                         void (*send_cb)(const uint8_t *, size_t), 
                         void (*nrc_cb)(uint8_t, uint8_t));

/**
 * @brief Clears active Diagnostic Trouble Codes inside the internal RAM database.
 *
 * Iterates through active fault elements and resets status masks to 0x00 if the
 * requested DTC group parameter (e.g., 0xFFFFFF for all groups) matches.
 *
 * @param dtc_group The 3-byte unique UDS DTC group identifier mask to evaluate.
 */
void uds_read_dtc_clear_all(uint32_t dtc_group);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_SRC_UDS_READ_DTC_H_ */
