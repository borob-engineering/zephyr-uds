/*
 * Copyright (c) 2026 borob-engineering
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public interface definitions for the UDS Routine Control module (Service 0x31).
 */

#ifndef ZEPHYR_SRC_UDS_ROUTINE_H_
#define ZEPHYR_SRC_UDS_ROUTINE_H_

#include <stdint.h>
#include <stddef.h>
#include "uds_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the Routine Control module components.
 *
 * This function sets up the asynchronous kernel work objects and supervisor timers 
 * required to execute long-running test routines in the background while handling 
 * Response Pending (NRC 0x78) frames.
 */
void uds_routine_init(void);

/**
 * @brief Central request handler for UDS Service 0x31 (Routine Control).
 *
 * Parses the incoming frame, validates formatting and session state, and handles
 * sub-functions like startRoutine (0x01) or requestRoutineResults (0x03) by 
 * interacting with the application layer and background worker queues.
 *
 * @param req     Pointer to the raw received request buffer payload.
 * @param len     Length of the request payload in bytes.
 * @param send_cb Pointer to the callback function for transmitting a positive response.
 * @param nrc_cb  Pointer to the callback function for transmitting a negative response code (NRC).
 */
void uds_routine_handle_control(uint8_t *req, size_t len, 
                                void (*send_cb)(const uint8_t *, size_t), 
                                void (*nrc_cb)(uint8_t, uint8_t));

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_SRC_UDS_ROUTINE_H_ */
