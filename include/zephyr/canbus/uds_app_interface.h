/**
 * @file uds_app_interface.h
 * @brief Applikations-Schnittstellen (Weak Functions) für den UDS-Core
 */

#ifndef UDS_APP_INTERFACE_H_
#define UDS_APP_INTERFACE_H_

#include "uds_types.h"
#include <stddef.h>

/* --- SERVICE 0x22 / 0x2E: DATA BY IDENTIFIER (DID) --- */
int uds_app_read_did(uint16_t did, uint8_t *data_out, size_t *len_out, size_t max_len);
int uds_app_write_did(uint16_t did, const uint8_t *data_in, size_t len);

/* --- SERVICE 0x27: SECURITY ACCESS --- */
uint32_t uds_app_calculate_key(const uint8_t *seed, size_t len);

/* --- SERVICE 0x2F: INPUT OUTPUT CONTROL --- */
int uds_app_io_control(uint16_t did, uint8_t control_param, const uint8_t *control_state, size_t state_len, uint8_t *status_out);

/* --- SERVICE 0x31: ROUTINE CONTROL --- */
int uds_app_routine_start(uint16_t routine_id, uint8_t *info_out);
int uds_app_routine_request_results(uint16_t routine_id, uint8_t *status_out, uint8_t *exit_info_out);

/* --- SERVICES 0x34 / 0x36 / 0x37: REAL HARDWARE FLASH PIPELINE --- */
int uds_app_flash_erase_target(uint32_t address, size_t size);
int uds_app_flash_write_block(uint32_t address_offset, const uint8_t *data, size_t len);

#endif /* UDS_APP_INTERFACE_H_ */
