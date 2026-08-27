/*
 * Copyright (c) 2026 borob-engineering
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public interface definitions for the UDS IO Control module (Service 0x2F).
 */

#ifndef ZEPHYR_SRC_UDS_IOCONTROL_H_
#define ZEPHYR_SRC_UDS_IOCONTROL_H_

#include <stdint.h>
#include <stddef.h>
#include "uds_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Central request handler for UDS Service 0x2F (Input Output Control By Identifier).
 *
 * Validates formatting, checks session and security preconditions, and delegates
 * the actuator control operation to the weak application layer interface hook.
 *
 * @param req     Pointer to the raw received request buffer payload.
 * @param len     Length of the request payload in bytes.
 * @param tx_buf  Pointer to the shared scratchpad transmission buffer.
 * @param send_cb Pointer to the callback function for transmitting a positive response.
 * @param nrc_cb  Pointer to the callback function for transmitting a negative response code (NRC).
 */
void uds_handle_io_control(uint8_t *req, size_t len, uint8_t *tx_buf, 
                           void (*send_cb)(const uint8_t *, size_t), 
                           void (*nrc_cb)(uint8_t, uint8_t));

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_SRC_UDS_IOCONTROL_H_ */
