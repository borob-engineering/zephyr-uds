/*
 * Copyright (c) 2026 borob-engineering
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Generic Security Access driver with crypto levels and NVS brute-force protection.
 *
 * This file implements UDS Service 0x27 (Security Access). It interacts with the Zephyr
 * Entropy API to generate secure random seeds and leverages the Non-Volatile Storage (NVS)
 * subsystem to persist counter states against brute-force attacks across ECU reboots.
 */

#include "uds_security.h"
#include "uds_app_interface.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/entropy.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/kvss/nvs.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_DECLARE(uds_server, LOG_LEVEL_INF);

#define SECURITY_SEED_SIZE   CONFIG_UDS_SEC_SEED_SIZE
#define MAX_FAILED_ATTEMPTS  CONFIG_UDS_SEC_MAX_FAILED_ATTEMPTS
#define LOCKOUT_TIME_MS      CONFIG_UDS_SEC_LOCKOUT_TIME_MS

#define NVS_DTC_COUNTER_ID   1
#define NVS_LOCKOUT_STATE_ID 2

/* Resolve storage partition node properties from Devicetree */
#define STORAGE_PARTITION_NODE    DT_NODELABEL(storage_partition)
#define STORAGE_PARTITION_DEVICE  DEVICE_DT_GET(DT_MTD_FROM_FIXED_PARTITION(STORAGE_PARTITION_NODE))
#define STORAGE_PARTITION_OFFSET  DT_REG_ADDR(STORAGE_PARTITION_NODE)
#define STORAGE_PARTITION_SIZE    DT_REG_SIZE(STORAGE_PARTITION_NODE)

static uds_security_status_t security_status = UDS_SEC_LOCKED;
static uint8_t generated_seed[SECURITY_SEED_SIZE];
static uint8_t failed_attempts_counter;
static struct k_timer lockout_timer;

static const struct device *entropy_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_entropy));
static struct nvs_fs fs;

/**
 * @brief Lockout supervisor timer callback. Resets brute-force block states.
 *
 * @param timer Pointer to the expiring kernel timer instance.
 */
static void lockout_timer_expiry_cb(struct k_timer *timer)
{
	uint8_t zero = 0;

	ARG_UNUSED(timer);
	security_status = UDS_SEC_LOCKED;
	failed_attempts_counter = 0;
	
	(void)nvs_write(&fs, NVS_DTC_COUNTER_ID, &zero, sizeof(zero));
	(void)nvs_write(&fs, NVS_LOCKOUT_STATE_ID, &zero, sizeof(zero));
	LOG_INF("Anti-Brute-Force security lockout period expired.");
}

void uds_security_init(void)
{
	struct flash_pages_info info;
	uint8_t stored_lockout = 0;
	int ret;

	k_timer_init(&lockout_timer, lockout_timer_expiry_cb, NULL);

	/* 1. Verify storage partition physical flash device readiness */
	if (!device_is_ready(STORAGE_PARTITION_DEVICE)) {
		LOG_ERR("Flash device for storage_partition is not ready!");
		return;
	}

	fs.flash_device = STORAGE_PARTITION_DEVICE;
	fs.offset = STORAGE_PARTITION_OFFSET;

	/* 2. Retrieve sector size at structural partition boundary start offset */
	ret = flash_get_page_info_by_offs(fs.flash_device, fs.offset, &info);
	if (ret == 0) {
		fs.sector_size = info.size;
		/* 3. Calculate virtual NVS sector density over total allocated layout scope */
		fs.sector_count = STORAGE_PARTITION_SIZE / info.size;
	} else {
		LOG_ERR("Could not read flash page details for partition offset!");
		return;
	}

	if (fs.sector_count == 0) {
		LOG_ERR("storage_partition size is smaller than a single physical flash page!");
		return;
	}

	if (nvs_mount(&fs) != 0) {
		LOG_ERR("NVS file system mount failed!");
		return;
	}

	ret = nvs_read(&fs, NVS_LOCKOUT_STATE_ID, &stored_lockout, sizeof(stored_lockout));
	if (ret > 0 && stored_lockout == 1) {
		security_status = UDS_SEC_BRUTE_FORCE_LOCKOUT;
		failed_attempts_counter = MAX_FAILED_ATTEMPTS;
		k_timer_start(&lockout_timer, K_MSEC(LOCKOUT_TIME_MS), K_NO_WAIT);
		LOG_WRN("Security lockout state re-activated from NVS flash storage!");
		return;
	}

	(void)nvs_read(&fs, NVS_DTC_COUNTER_ID, &failed_attempts_counter, sizeof(failed_attempts_counter));
}

uds_security_status_t uds_security_get_status(void)
{
	return security_status;
}

void uds_security_reset_lock(void)
{
	if (security_status != UDS_SEC_BRUTE_FORCE_LOCKOUT) {
		security_status = UDS_SEC_LOCKED;
	}
}
void uds_security_handle_request(uint8_t *req, size_t len, uint8_t *tx_buf, 
                                 void (*send_cb)(const uint8_t *, size_t), 
                                 void (*nrc_cb)(uint8_t, uint8_t))
{
	uint8_t sid = req[0];
	uint8_t sub_function;
	uint8_t check_level;
	uint8_t local_key_buf[16];
	uint32_t uptime;
	uint8_t zero = 0;
	uint8_t lockout_active = 1;
	int access_granted;
	int i;

	if (len < 2) {
		nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
		return;
	}

	if (security_status == UDS_SEC_BRUTE_FORCE_LOCKOUT) {
		nrc_cb(sid, UDS_NRC_EXCEEDED_NUMBER_OF_ATTEMPTS);
		return;
	}

	sub_function = req[1];

	if (sub_function == 0x01 || sub_function == 0x03) { /* Request Seed (Level 1 or 3) */
		if (len != 2) {
			nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
			return;
		}

		if (security_status == UDS_SEC_UNLOCKED) {
			memset(generated_seed, 0, SECURITY_SEED_SIZE);
		} else {
			if (device_is_ready(entropy_dev)) {
				(void)entropy_get_entropy(entropy_dev, generated_seed, SECURITY_SEED_SIZE);
			} else {
				uptime = k_uptime_get_32();
				for (i = 0; i < SECURITY_SEED_SIZE; i++) {
					generated_seed[i] = (uint8_t)(uptime >> (i * 8));
				}
			}
			security_status = UDS_SEC_SEED_REQUESTED;
		}

		tx_buf[0] = sid + 0x40;
		tx_buf[1] = sub_function;
		memcpy(&tx_buf[2], generated_seed, SECURITY_SEED_SIZE);
		send_cb(tx_buf, 2 + SECURITY_SEED_SIZE);

	} else if (sub_function == 0x02 || sub_function == 0x04) { /* Send Key (Level 1 or 3) */
		if (len != (2 + SECURITY_SEED_SIZE)) {
			nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
			return;
		}

		if (security_status != UDS_SEC_SEED_REQUESTED) {
			nrc_cb(sid, UDS_NRC_REQUEST_SEQUENCE_ERROR);
			return;
		}

		check_level = sub_function - 1; /* Evaluates to 1 or 3 */

		if (SECURITY_SEED_SIZE <= sizeof(local_key_buf)) {
			memcpy(local_key_buf, &req[2], SECURITY_SEED_SIZE);
		}

		/* Pass the 32-bit aligned stack reference to prevent alignment traps */
		access_granted = uds_app_verify_key_krypto(check_level, generated_seed,
							    SECURITY_SEED_SIZE, local_key_buf,
							    SECURITY_SEED_SIZE);

		if (access_granted == 0) {
			security_status = UDS_SEC_UNLOCKED;
			failed_attempts_counter = 0;
			(void)nvs_write(&fs, NVS_DTC_COUNTER_ID, &zero, sizeof(zero));
			
			tx_buf[0] = sid + 0x40;
			tx_buf[1] = sub_function;
			send_cb(tx_buf, 2);
		} else {
			failed_attempts_counter++;
			(void)nvs_write(&fs, NVS_DTC_COUNTER_ID, &failed_attempts_counter,
					sizeof(failed_attempts_counter));

			if (failed_attempts_counter >= MAX_FAILED_ATTEMPTS) {
				security_status = UDS_SEC_BRUTE_FORCE_LOCKOUT;
				(void)nvs_write(&fs, NVS_LOCKOUT_STATE_ID, &lockout_active,
						sizeof(lockout_active));
				k_timer_start(&lockout_timer, K_MSEC(LOCKOUT_TIME_MS), K_NO_WAIT);
				nrc_cb(sid, UDS_NRC_EXCEEDED_NUMBER_OF_ATTEMPTS);
			} else {
				security_status = UDS_SEC_LOCKED;
				nrc_cb(sid, UDS_NRC_INVALID_KEY);
			}
		}
	} else {
		nrc_cb(sid, UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED);
	}
}
