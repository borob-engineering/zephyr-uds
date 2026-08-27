/*
 * Copyright (c) 2026 borob-engineering
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public interface for the central vehicle data RAM storage component.
 */

#ifndef ZEPHYR_SRC_UDS_DATA_STORAGE_H_
#define ZEPHYR_SRC_UDS_DATA_STORAGE_H_

#include <stdint.h>
#include <stddef.h>
#include "uds_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the volatile internal data storage.
 *
 * Populates global non-volatile fields with factory default fallback strings
 * during the early firmware boot up routine.
 */
void uds_data_storage_init(void);

/**
 * @brief Retrieves a pointer to the active Vehicle Identification Number (VIN).
 *
 * @return Const pointer to the 17-byte raw VIN memory area.
 */
const uint8_t *uds_data_storage_get_vin(void);

/**
 * @brief Updates the active Vehicle Identification Number (VIN) inside internal RAM.
 *
 * @param new_vin Pointer to the buffer containing the new 17-byte identification string.
 * @param len     Length of the new VIN block buffer.
 *
 * @retval 0       On success.
 * @retval -EINVAL Invalid buffer length or null pointer assignment.
 */
int uds_data_storage_set_vin(const uint8_t *new_vin, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_SRC_UDS_DATA_STORAGE_H_ */
