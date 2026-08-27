/*
 * Copyright (c) 2026 borob-engineering
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public interface definitions for the UDS Flashing Pipeline services.
 *
 * Contains handlers for Service 0x34 (Request Download), Service 0x36 (Transfer Data),
 * and Service 0x37 (Request Transfer Exit) used during ECU firmware updates.
 */

#ifndef ZEPHYR_SRC_UDS_FLASH_PIPELINE_H_
#define ZEPHYR_SRC_UDS_FLASH_PIPELINE_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the internal states and pointers of the flashing pipeline.
 */
void uds_flash_pipeline_init(void);

/**
 * @brief Central request handler for UDS Service 0x34 (Request Download).
 *
 * @param req     Pointer to the raw received request buffer payload.
 * @param len     Length of the request payload in bytes.
 * @param tx_buf  Pointer to the shared scratchpad transmission buffer.
 * @param send_cb Pointer to the callback function for transmitting responses.
 * @param nrc_cb  Pointer to the callback function for transmitting NRCs.
 */
void uds_handle_request_download(uint8_t *req, size_t len, uint8_t *tx_buf,
				 void (*send_cb)(const uint8_t *, size_t),
				 void (*nrc_cb)(uint8_t, uint8_t));

/**
 * @brief Central request handler for UDS Service 0x36 (Transfer Data).
 *
 * @param req     Pointer to the raw received request buffer payload.
 * @param len     Length of the request payload in bytes.
 * @param tx_buf  Pointer to the shared scratchpad transmission buffer.
 * @param send_cb Pointer to the callback function for transmitting responses.
 * @param nrc_cb  Pointer to the callback function for transmitting NRCs.
 */
void uds_handle_transfer_data(uint8_t *req, size_t len, uint8_t *tx_buf,
			      void (*send_cb)(const uint8_t *, size_t),
			      void (*nrc_cb)(uint8_t, uint8_t));

/**
 * @brief Central request handler for UDS Service 0x37 (Request Transfer Exit).
 *
 * @param req     Pointer to the raw received request buffer payload.
 * @param len     Length of the request payload in bytes.
 * @param tx_buf  Pointer to the shared scratchpad transmission buffer.
 * @param send_cb Pointer to the callback function for transmitting responses.
 * @param nrc_cb  Pointer to the callback function for transmitting NRCs.
 */
void uds_handle_request_transfer_exit(uint8_t *req, size_t len, uint8_t *tx_buf,
				      void (*send_cb)(const uint8_t *, size_t),
				      void (*nrc_cb)(uint8_t, uint8_t));

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_SRC_UDS_FLASH_PIPELINE_H_ */
