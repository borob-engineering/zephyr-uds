/**
 * @file uds_app_interface.h
 * @brief Erweitertes Applikations-Interface für OEM-Serienfunktionen (Zephyr 4.4.0)
 */

#ifndef UDS_APP_INTERFACE_H_
#define UDS_APP_INTERFACE_H_

#include "uds_types.h"
#include <stddef.h>

/* --- STANDARD-DIENSTE (0x22, 0x2E, 0x2F, 0x31, Flash) --- */
int uds_app_read_did(uint16_t did, uint8_t *data_out, size_t *len_out, size_t max_len);
int uds_app_write_did(uint16_t did, const uint8_t *data_in, size_t len);
int uds_app_io_control(uint16_t did, uint8_t control_param, const uint8_t *control_state, size_t state_len, uint8_t *status_out);
int uds_app_routine_start(uint16_t routine_id, uint8_t *info_out);
int uds_app_routine_request_results(uint16_t routine_id, uint8_t *status_out, uint8_t *exit_info_out);
int uds_app_flash_erase_target(uint32_t address, size_t size);
int uds_app_flash_write_block(uint32_t address_offset, const uint8_t *data, size_t len);

/* --- ERWEITERUNG 1: FREEZE FRAMES (0x19 0x04) --- */
int uds_app_get_freeze_frame(uint32_t dtc, uint8_t record_num, uint8_t *data_out, size_t *len_out, size_t max_len);

/* --- ERWEITERUNG 2: ERWEITERTE KRYPTOGRAFIE (0x27 AES-128 / SHA-256) --- */
int uds_app_verify_key_krypto(uint8_t security_level, const uint8_t *seed, size_t seed_len, const uint8_t *received_key, size_t key_len);

/* --- ERWEITERUNG 3: PERIODISCHE DATEN (0x2A) --- */
int uds_app_get_periodic_did(uint8_t periodic_did, uint8_t *data_out, size_t *len_out);

#endif /* UDS_APP_INTERFACE_H_ */
