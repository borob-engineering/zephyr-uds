/*
 * Copyright (c) 2026 borob-engineering
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public interface definitions for the UDS Periodic Data module (Service 0x2A).
 */

#ifndef ZEPHYR_SRC_UDS_PERIODIC_H_
#define ZEPHYR_SRC_UDS_PERIODIC_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Central request handler for UDS Service 0x2A (Read Data By Periodic Identifier).
 *
 * Parses incoming tester requests to manage periodic schedules. Supports starting 
 * data transmission at configured update rates or completely resetting the fast scheduler loop.
 *
 * @param req     Pointer to the raw received request buffer payload.
 * @param len     Length of the request payload in bytes.
 * @param send_cb Pointer to the callback function for transmitting a positive response.
 * @param nrc_cb  Pointer to the callback function for transmitting a negative response code (NRC).
 */
void uds_handle_periodic_request(uint8_t *req, size_t len,
				 void (*send_cb)(const uint8_t *, size_t),
				 void (*nrc_cb)(uint8_t, uint8_t));

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_SRC_UDS_PERIODIC_H_ */
