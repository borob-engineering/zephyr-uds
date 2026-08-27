/*
 * Copyright (c) 2026 borob-engineering
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Implementation of the central volatile RAM storage for vehicle identifiers.
 *
 * This file contains internal storage definitions and wrappers for data
 * persistence layers, specifically handling global identifiers like the VIN.
 */
 
#include "uds_data_storage.h"
#include <string.h>
#include <errno.h>

/** @brief Internal buffer holding the active 17-byte Vehicle Identification Number. */
static uint8_t vehicle_vin[VIN_SIZE];

void uds_data_storage_init(void)
{
	/* Initialize with a default placeholder VIN on bootup sequence */
	memcpy(vehicle_vin, "ZEPHYRISANRTOS123", VIN_SIZE);
}

const uint8_t *uds_data_storage_get_vin(void)
{
	return vehicle_vin;
}

int uds_data_storage_set_vin(const uint8_t *new_vin, size_t len)
{
	if (len != VIN_SIZE || new_vin == NULL) {
		return -EINVAL;
	}

	memcpy(vehicle_vin, new_vin, VIN_SIZE);
	return 0;
}
