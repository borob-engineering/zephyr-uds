/*
 * Copyright (c) 2026 borob-engineering
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public interface definitions for the UDS ECU Reset module (Service 0x11).
 */

#ifndef ZEPHYR_SRC_UDS_RESET_H_
#define ZEPHYR_SRC_UDS_RESET_H_

#include <stdint.h>
#include <stddef.h>
#include "uds_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the ECU Reset module components.
 *
 * This function sets up the internal asynchronous kernel timers required to execute
 * the hardware/software reboot sequence after transmitting the response frame.
 */
void uds_reset_init(void);

/**
 * @brief Central request handler for UDS Service 0x11 (ECU Reset).
 *
 * Parses the incoming frame, validates the requested reset sub-function (hard vs soft),
 * transmits the positive confirmation, and schedules the non-blocking system reboot.
 *
 * @param req     Pointer to the raw received request buffer payload.
 * @param len     Length of the request payload in bytes.
 * @param tx_buf  Pointer to the shared scratchpad transmission buffer.
 * @param send_cb Pointer to the callback function for transmitting a positive response.
 * @param nrc_cb  Pointer to the callback function for transmitting a negative response code (NRC).
 */
void uds_reset_handle(uint8_t *req, size_t len, uint8_t *tx_buf, 
                      void (*send_cb)(const uint8_t *, size_t), 
                      void (*nrc_cb)(uint8_t, uint8_t));

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_SRC_UDS_RESET_H_ */
