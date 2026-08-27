/*
 * Copyright (c) 2026 borob-engineering
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public interface definitions for the UDS Security Access module (Service 0x27).
 */

#ifndef ZEPHYR_SRC_UDS_SECURITY_H_
#define ZEPHYR_SRC_UDS_SECURITY_H_

#include <stdint.h>
#include <stddef.h>
#include "uds_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the Security Access module components.
 *
 * Mounts the Non-Volatile Storage (NVS) file system, checks for persisted
 * brute-force lockout states from previous boot cycles, and sets up anti-tamper timers.
 */
void uds_security_init(void);

/**
 * @brief Retrieves the active global UDS security access state machine status.
 *
 * @return The current security status enum value (Locked, Seed Requested, Unlocked, Lockout).
 */
uds_security_status_t uds_security_get_status(void);

/**
 * @brief Resets the dynamic security state back to locked.
 *
 * Safe fallback hook that enforces a locked security level unless an active,
 * non-volatile anti-brute-force timeout block is currently running.
 */
void uds_security_reset_lock(void);

/**
 * @brief Central request handler for UDS Service 0x27 (Security Access).
 *
 * Evaluates seed requests (0x01/0x03) and key validations (0x02/0x04). Coordinates
 * cryptographic validation hooks and updates non-volatile fail-counters.
 *
 * @param req     Pointer to the raw received request buffer payload.
 * @param len     Length of the request payload in bytes.
 * @param tx_buf  Pointer to the shared scratchpad transmission buffer.
 * @param send_cb Pointer to the callback function for transmitting a positive response.
 * @param nrc_cb  Pointer to the callback function for transmitting a negative response code (NRC).
 */
void uds_security_handle_request(uint8_t *req, size_t len, uint8_t *tx_buf, 
                                 void (*send_cb)(const uint8_t *, size_t), 
                                 void (*nrc_cb)(uint8_t, uint8_t));

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_SRC_UDS_SECURITY_H_ */
