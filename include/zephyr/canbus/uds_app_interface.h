/*
 * Copyright (c) 2026 borob-engineering
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Extended application interface for UDS OEM functions.
 *
 * This header defines the weak interface hooks that the application layer
 * must implement to handle specific UDS services like DID read/write,
 * IO control, routine execution, flashing, and diagnostic data retrieval.
 */

#ifndef ZEPHYR_INCLUDE_CANBUS_UDS_APP_INTERFACE_H_
#define ZEPHYR_INCLUDE_CANBUS_UDS_APP_INTERFACE_H_

#include <stdint.h>
#include <stddef.h>
#include "uds_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read Data By Identifier (Service 0x22).
 *
 * @param did The 16-bit Data Identifier to read.
 * @param data_out Pointer to the buffer where the DID data should be copied.
 * @param len_out Pointer to store the actual number of bytes copied.
 * @param max_len Maximum capacity of the data_out buffer.
 *
 * @retval 0 On success.
 * @retval -EIO General input/output error.
 * @retval -ENOENT DID not found (maps to NRC 0x31 RequestOutOfRange).
 * @retval -EACCES Invalid security level or session (maps to NRC 0x33 or 0x7F).
 * @retval -EINVAL Buffer too small for requested data (maps to NRC 0x14).
 */
int uds_app_read_did(uint16_t did, uint8_t *data_out, size_t *len_out, size_t max_len);

/**
 * @brief Write Data By Identifier (Service 0x2E).
 *
 * @param did The 16-bit Data Identifier to write.
 * @param data_in Pointer to the received data block to be persisted.
 * @param len Length of the data in bytes.
 *
 * @retval 0 On success.
 * @retval -ENOENT DID not found (maps to NRC 0x31 RequestOutOfRange).
 * @retval -EACCES Write blocked in current session/security level (maps to NRC 0x7E/0x33).
 * @retval -EINVAL Invalid data length or content (maps to NRC 0x13 or 0x22).
 */
int uds_app_write_did(uint16_t did, const uint8_t *data_in, size_t len);

/**
 * @brief Input Output Control By Identifier (Service 0x2F).
 *
 * @param did The 16-bit Data Identifier associated with the actuator.
 * @param control_param The UDS control parameter (e.g., returnControlToECU, shortTermAdjustment).
 * @param control_state Pointer to the optional control state options.
 * @param state_len Length of the control state option buffer.
 * @param status_out Pointer to copy the current actuator status back to the tester.
 *
 * @retval 0 On success.
 * @retval -ENOENT DID/Actuator not found (maps to NRC 0x31).
 * @retval -EINVAL Invalid control parameter or state values (maps to NRC 0x22).
 */
int uds_app_io_control(uint16_t did, uint8_t control_param, const uint8_t *control_state,
		       size_t state_len, uint8_t *status_out);

/**
 * @brief Start Routine (Service 0x31, Sub-function 0x01).
 *
 * @param routine_id The 16-bit Routine Identifier to execute.
 * @param info_out Pointer to buffer for optional routine entry execution info.
 *
 * @retval 0 On success.
 * @retval -ENOENT Routine ID not found (maps to NRC 0x31).
 * @retval -EINPROGRESS Routine started asynchronously (maps to NRC 0x78 ResponsePending).
 * @retval -EACCES Preconditions not met (maps to NRC 0x22).
 */
int uds_app_routine_start(uint16_t routine_id, uint8_t *info_out);

/**
 * @brief Request Routine Results (Service 0x31, Sub-function 0x03).
 *
 * @param routine_id The 16-bit Routine Identifier.
 * @param status_out Pointer to output buffer for the current execution status.
 * @param exit_info_out Pointer to output buffer for exit information codes.
 *
 * @retval 0 On success.
 * @retval -ENOENT Routine ID not found (maps to NRC 0x31).
 * @retval -EBUSY Routine is still running (maps to NRC 0x24).
 */
int uds_app_routine_request_results(uint16_t routine_id, uint8_t *status_out,
				    uint8_t *exit_info_out);

/**
 * @brief Erase Flash Memory Target (Service 0x31 / Flashing Pipeline).
 *
 * @param address The absolute starting flash address to erase.
 * @param size The size of the memory area to be erased in bytes.
 *
 * @retval 0 On success.
 * @retval -EFAULT Erase operation failed in hardware (maps to NRC 0x72).
 * @retval -EINVAL Invalid address range (maps to NRC 0x31).
 */
int uds_app_flash_erase_target(uint32_t address, size_t size);

/**
 * @brief Write Flash Memory Block (Service 0x36 / Flashing Pipeline).
 *
 * @param address_offset The current write offset/address in the flash target.
 * @param data Pointer to the binary payload data block.
 * @param len Length of the data block in bytes.
 *
 * @retval 0 On success.
 * @retval -EFAULT Write operation failed in hardware (maps to NRC 0x72).
 * @retval -EIO Flash alignment or verify error (maps to NRC 0x71).
 */
int uds_app_flash_write_block(uint32_t address_offset, const uint8_t *data, size_t len);

/**
 * @brief Read DTC Freeze Frame Data (Service 0x19, Sub-function 0x04).
 *
 * @param dtc The 3-byte Diagnostic Trouble Code.
 * @param record_num The specific freeze frame record number requested.
 * @param data_out Pointer to the buffer where freeze frame data will be written.
 * @param len_out Pointer to store the actual copied data length.
 * @param max_len Maximum capacity of the data_out buffer.
 *
 * @retval 0 On success.
 * @retval -ENOENT DTC or Record number not found (maps to NRC 0x31).
 * @retval -EINVAL Buffer size too small (maps to NRC 0x14).
 */
int uds_app_get_freeze_frame(uint32_t dtc, uint8_t record_num, uint8_t *data_out,
			     size_t *len_out, size_t max_len);

/**
 * @brief Advanced Cryptographic Key Verification (Service 0x27 Security Access).
 *
 * Implements advanced validation via asymmetrical or symmetrical mechanisms (e.g., AES/SHA).
 *
 * @param security_level The requested security level step.
 * @param seed Pointer to the generated seed bytes sent to the tester.
 * @param seed_len Length of the generated seed.
 * @param received_key Pointer to the key bytes received from the tester.
 * @param key_len Length of the received key.
 *
 * @retval 0 On success (Key valid, access granted).
 * @retval -EACCES Key invalid (maps to NRC 0x35 InvalidKey).
 * @retval -EINVAL Length mismatch or wrong state (maps to NRC 0x13 or 0x22).
 */
int uds_app_verify_key_krypto(uint8_t security_level, const uint8_t *seed, size_t seed_len,
			      const uint8_t *received_key, size_t key_len);

/**
 * @brief Read Periodic Data By Identifier (Service 0x2A).
 *
 * @param periodic_did The 1-byte periodic data identifier.
 * @param data_out Pointer to the buffer where the fast periodic sample data is stored.
 * @param len_out Pointer to store the sampled payload length.
 *
 * @retval 0 On success.
 * @retval -ENOENT Periodic DID not supported (maps to NRC 0x31).
 */
int uds_app_get_periodic_did(uint8_t periodic_did, uint8_t *data_out, size_t *len_out);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_CANBUS_UDS_APP_INTERFACE_H_ */
