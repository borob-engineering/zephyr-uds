/*
 * Copyright (c) 2026 borob-engineering
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Implementation of the vehicle data storage component with NVS non-volatile persistence.
 */

#include "uds_data_storage.h"
#include "uds_app_interface.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/kvss/nvs.h>
#include <string.h>
#include <errno.h>

LOG_MODULE_DECLARE(uds_server, LOG_LEVEL_INF);

/* Reservierte Applikations-NVS-ID für die persistente Speicherung der VIN */
#define NVS_VIN_PARAM_ID 0x0200

/** @brief Active volatile RAM cache mirror holding the current VIN for rapid retrieval. */
static uint8_t vehicle_vin[VIN_SIZE];

void uds_data_storage_init(void)
{
	struct nvs_fs *fs;
	int ret;

	/* Set up standard factory fallback configuration */
	memcpy(vehicle_vin, "ZEPHYRISANRTOS123", VIN_SIZE);

	fs = uds_app_get_nvs_context();
	if (fs != NULL) {
		/* Attempt to retrieve previously persisted VIN string block from NVS flash */
		ret = nvs_read(fs, NVS_VIN_PARAM_ID, vehicle_vin, VIN_SIZE);
		if (ret == VIN_SIZE) {
			LOG_INF("Successfully recovered persistent VIN from flash storage.");
		} else {
			LOG_WRN("No persistent VIN found. Loading factory default fallback.");
		}
	}
}

const uint8_t *uds_data_storage_get_vin(void)
{
	return vehicle_vin;
}

int uds_data_storage_set_vin(const uint8_t *new_vin, size_t len)
{
	struct nvs_fs *fs;
	int ret;

	if (len != VIN_SIZE || new_vin == NULL) {
		return -EINVAL;
	}

	fs = uds_app_get_nvs_context();
	if (fs == NULL) {
		return -ENXIO;
	}

	/* 1. Synchronize incoming stream payload directly to physical flash sectors via NVS hook */
	ret = uds_app_write_persistent_data(fs, NVS_VIN_PARAM_ID, new_vin, VIN_SIZE);
	if (ret != 0) {
		LOG_ERR("Failed to persist new VIN identifier block to non-volatile flash: %d", ret);
		return -EIO;
	}

	/* 2. Update local runtime mirror cache upon successful flash operation */
	memcpy(vehicle_vin, new_vin, VIN_SIZE);
	LOG_INF("VIN successfully updated in flash memory and active RAM cache.");

	return 0;
}
