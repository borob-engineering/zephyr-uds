/*
 * Copyright (c) 2026 borob-engineering
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public interface definitions for the UDS Session Management module (Service 0x10).
 */

#ifndef ZEPHYR_SRC_UDS_SESSION_H_
#define ZEPHYR_SRC_UDS_SESSION_H_

#include <stdint.h>
#include <stddef.h>
#include "uds_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the Session Management module components.
 *
 * Sets up the internal kernel timers required to monitor S3 client timeout overhead
 * and forces the stack state machine into the default diagnostic session.
 */
void uds_session_init(void);

/**
 * @brief Transitions the active diagnostic layer into a new session type.
 *
 * @param new_session The target session enum value (Default, Programming, Extended).
 */
void uds_session_set(uds_session_type_t new_session);

/**
 * @brief Retrieves the active global UDS diagnostic session state.
 *
 * @return The current active session type enum value.
 */
uds_session_type_t uds_session_get(void);

/**
 * @brief Refreshes or stops the active S3 server keep-alive connection timer.
 *
 * Automatically schedules a re-trigger window if the active session is non-default,
 * or safely terminates supervisor timer tracking when inside default execution boundaries.
 */
void uds_session_refresh_timer(void);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_SRC_UDS_SESSION_H_ */
