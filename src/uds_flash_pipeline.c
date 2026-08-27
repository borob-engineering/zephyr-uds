/*
 * Copyright (c) 2026 borob-engineering
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Implementation of UDS Services 0x34, 0x36, and 0x37 for the flashing pipeline.
 *
 * This file contains the complete sequential pipeline logic required for secure and
 * standard-compliant software flashing (firmware updates) on top of physical flash drivers.
 */

#include "uds_flash_pipeline.h"
#include "uds_session.h"
#include "uds_security.h"
#include "uds_app_interface.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <errno.h>

LOG_MODULE_DECLARE(uds_server, LOG_LEVEL_INF);

/**
 * @brief Internal tracking states of the software flashing sequence.
 */
typedef enum {
	/** Default state, pipeline is locked and inactive */
	FLASH_STATE_IDLE,
	/** Download session negotiated, target memory erased, awaiting binary streaming */
	FLASH_STATE_DOWNLOAD_APPROVED,
	/** Consecutive block data transfer active */
	FLASH_STATE_TRANSFERRING
} flash_state_t;

static flash_state_t pipeline_state = FLASH_STATE_IDLE;
static uint32_t memory_address;
static uint32_t memory_size;
static uint32_t bytes_received;
static uint8_t expected_block_counter = 1;

void uds_flash_pipeline_init(void)
{
	pipeline_state = FLASH_STATE_IDLE;
	memory_address = 0;
	memory_size = 0;
	bytes_received = 0;
	expected_block_counter = 1;
}

void uds_handle_request_download(uint8_t *req, size_t len, uint8_t *tx_buf, 
                                 void (*send_cb)(const uint8_t *, size_t), 
                                 void (*nrc_cb)(uint8_t, uint8_t))
{
	uint8_t sid = req[0];
	uint8_t alf_id;
	uint8_t memory_address_len;
	uint8_t memory_size_len;
	size_t idx;
	int i;
	int ret;

	if (uds_session_get() == UDS_SESSION_DEFAULT) {
		nrc_cb(sid, UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED_IN_ACTIVE_SESS);
		return;
	}

	if (uds_security_get_status() != UDS_SEC_UNLOCKED) {
		nrc_cb(sid, UDS_NRC_SECURITY_ACCESS_DENIED);
		return;
	}

	if (len < 5) {
		nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
		return;
	}

	alf_id = req[2];
	memory_address_len = alf_id & 0x0F;
	memory_size_len = (alf_id >> 4) & 0x0F;

	if (len != (3 + memory_address_len + memory_size_len)) {
		nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
		return;
	}

	memory_address = 0;
	idx = 3;
	for (i = 0; i < memory_address_len; i++) {
		memory_address = (memory_address << 8) | req[idx++];
	}

	memory_size = 0;
	for (i = 0; i < memory_size_len; i++) {
		memory_size = (memory_size << 8) | req[idx++];
	}

	LOG_INF("UDS Download Request received. Addr: 0x%08X, Size: %u Bytes",
		memory_address, memory_size);

	/* Clear target area inside physical memory flash before data ingestion */
	ret = uds_app_flash_erase_target(memory_address, memory_size);
	if (ret != 0) {
		LOG_ERR("Hardware flash erasure target failed: %d", ret);
		nrc_cb(sid, UDS_NRC_UPLOAD_DOWNLOAD_NOT_ACCEPTED);
		return;
	}

	bytes_received = 0;
	expected_block_counter = 1;
	pipeline_state = FLASH_STATE_DOWNLOAD_APPROVED;

	tx_buf[0] = sid + 0x40;
	tx_buf[1] = 0x20; 
	tx_buf[2] = (uint8_t)(UDS_BUFF_SIZE >> 8); 
	tx_buf[3] = (uint8_t)(UDS_BUFF_SIZE & 0xFF); 

	send_cb(tx_buf, 4);
}

void uds_handle_transfer_data(uint8_t *req, size_t len, uint8_t *tx_buf, 
                              void (*send_cb)(const uint8_t *, size_t), 
                              void (*nrc_cb)(uint8_t, uint8_t))
{
	uint8_t sid = req[0];
	uint8_t block_counter;
	size_t payload_len;
	uint32_t current_offset;
	int ret;

	if (pipeline_state != FLASH_STATE_DOWNLOAD_APPROVED &&
	    pipeline_state != FLASH_STATE_TRANSFERRING) {
		nrc_cb(sid, UDS_NRC_REQUEST_SEQUENCE_ERROR);
		return;
	}

	if (len < 3) {
		nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
		return;
	}

	block_counter = req[1];
	if (block_counter != expected_block_counter) {
		nrc_cb(sid, UDS_NRC_WRONG_BLOCK_SEQUENCE_COUNTER);
		return;
	}

	payload_len = len - 2;
	current_offset = memory_address + bytes_received;

	/* Stream the parsed block chunk payload directly into target flash layer memory */
	ret = uds_app_flash_write_block(current_offset, &req[2], payload_len);
	if (ret != 0) {
		LOG_ERR("Writing to flash target failed at offset 0x%08X: %d", current_offset, ret);
		nrc_cb(sid, UDS_NRC_TRANSFER_DATA_SUSPENDED);
		return;
	}

	bytes_received += payload_len;
	expected_block_counter++;
	pipeline_state = FLASH_STATE_TRANSFERRING;

	tx_buf[0] = sid + 0x40;
	tx_buf[1] = block_counter;

	send_cb(tx_buf, 2);
}

void uds_handle_request_transfer_exit(uint8_t *req, size_t len, uint8_t *tx_buf, 
                                      void (*send_cb)(const uint8_t *, size_t), 
                                      void (*nrc_cb)(uint8_t, uint8_t))
{
	uint8_t sid = req[0];

	ARG_UNUSED(len);

	if (pipeline_state != FLASH_STATE_TRANSFERRING) {
		nrc_cb(sid, UDS_NRC_REQUEST_SEQUENCE_ERROR);
		return;
	}

	if (bytes_received < memory_size) {
		LOG_WRN("Warning: Fewer bytes received (%u) than requested via 0x34 (%u)",
			bytes_received, memory_size);
	}

	LOG_INF("Transfer completed successfully. Total %u bytes written to flash.", bytes_received);
	pipeline_state = FLASH_STATE_IDLE;

	tx_buf[0] = sid + 0x40;
	send_cb(tx_buf, 1);
}
