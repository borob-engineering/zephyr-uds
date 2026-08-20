/**
 * @file uds_flash_pipeline.c
 * @brief Implementierung der Services 0x34, 0x36 und 0x37 für die Flash-Pipeline (Software-Flashing)
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

typedef enum {
	FLASH_STATE_IDLE,
	FLASH_STATE_DOWNLOAD_APPROVED,
	FLASH_STATE_TRANSFERRING
} flash_state_t;

static flash_state_t pipeline_state = FLASH_STATE_IDLE;
static uint32_t memory_address = 0;
static uint32_t memory_size = 0;
static uint32_t bytes_received = 0;
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

	uint8_t alf_id = req[2];
	uint8_t memory_address_len = alf_id & 0x0F;
	uint8_t memory_size_len = (alf_id >> 4) & 0x0F;

	if (len != (3 + memory_address_len + memory_size_len)) {
		nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
		return;
	}

	memory_address = 0;
	size_t idx = 3;
	for (int i = 0; i < memory_address_len; i++) {
		memory_address = (memory_address << 8) | req[idx++];
	}

	memory_size = 0;
	for (int i = 0; i < memory_size_len; i++) {
		memory_size = (memory_size << 8) | req[idx++];
	}

	LOG_INF("UDS Download Request erhalten. Addr: 0x%08X, Size: %u Bytes", memory_address, memory_size);

	/* ZENTRALE ÄNDERUNG: Vor dem Download wird der Zielbereich im Flash hardwareseitig gelöscht */
	int ret = uds_app_flash_erase_target(memory_address, memory_size);
	if (ret != 0) {
		LOG_ERR("Hardware-Flash loeschen fehlgeschlagen: %d", ret);
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

	if (pipeline_state != FLASH_STATE_DOWNLOAD_APPROVED && pipeline_state != FLASH_STATE_TRANSFERRING) {
		nrc_cb(sid, UDS_NRC_REQUEST_SEQUENCE_ERROR);
		return;
	}

	if (len < 3) {
		nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
		return;
	}

	uint8_t block_counter = req[1];
	if (block_counter != expected_block_counter) {
		nrc_cb(sid, UDS_NRC_WRONG_BLOCK_SEQUENCE_COUNTER);
		return;
	}

	size_t payload_len = len - 2;

	/* ZENTRALE ÄNDERUNG: Eingehenden Block direkt per Offset physisch in den Flash schreiben */
	uint32_t current_offset = memory_address + bytes_received;
	int ret = uds_app_flash_write_block(current_offset, &req[2], payload_len);
	if (ret != 0) {
		LOG_ERR("Schreiben im Flash bei Offset 0x%08X fehlgeschlagen: %d", current_offset, ret);
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

	if (pipeline_state != FLASH_STATE_TRANSFERRING) {
		nrc_cb(sid, UDS_NRC_REQUEST_SEQUENCE_ERROR);
		return;
	}

	if (bytes_received < memory_size) {
		LOG_WRN("Warnung: Weniger Bytes erhalten (%u) als via 0x34 angefordert (%u)", bytes_received, memory_size);
	}

	LOG_INF("Transfer erfolgreich beendet. Insgesamt %u Bytes geflasht.", bytes_received);
	pipeline_state = FLASH_STATE_IDLE;

	tx_buf[0] = sid + 0x40;
	send_cb(tx_buf, 1);
}
