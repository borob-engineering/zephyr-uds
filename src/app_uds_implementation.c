/**
 * @file app_uds_implementation.c
 * @brief Konkrete Applikationslogik (Überschreibt die __weak Definitionen)
 */

#include "uds_app_interface.h"
#include <string.h>
#include <errno.h>

#define DID_VIN        0xF190
#define DID_STATUS_LED 0x0123
#define RID_ERASE_FLASH 0xFF00

static uint8_t app_vin[17] = "ZEPHYRISANRTOS123";
static uint8_t app_led_state = 0;

/* Implementierung: DID Lesen */
int uds_app_read_did(uint16_t did, uint8_t *data_out, size_t *len_out, size_t max_len)
{
    if (did == DID_VIN) {
        if (max_len < 17) return -ENOMEM;
        memcpy(data_out, app_vin, 17);
        *len_out = 17;
        return 0;
    }
    return -ENOENT; /* Reicht der Core als NRC 0x31 (RequestOutOfRange) weiter */
}

/* Implementierung: DID Schreiben */
int uds_app_write_did(uint16_t did, const uint8_t *data_in, size_t len)
{
    if (did == DID_VIN) {
        if (len != 17) return -EINVAL; /* Reicht Core als NRC 0x13 (IncorrectLength) weiter */
        memcpy(app_vin, data_in, 17);
        return 0;
    }
    return -ENOENT;
}

/* Implementierung: Key-Berechnung für Security Access Level 1 */
uint32_t uds_app_calculate_key(const uint8_t *seed, size_t len)
{
    if (len < 4) return 0;
    uint32_t seed_val = ((uint32_t)seed[0] << 24) | ((uint32_t)seed[1] << 16) | 
                        ((uint32_t)seed[2] << 8)  | (uint32_t)seed[3];
    
    return seed_val ^ CONFIG_UDS_SEC_SECRET_MASK;
}

/* Implementierung: Stellgliedtest */
int uds_app_io_control(uint16_t did, uint8_t control_param, const uint8_t *control_state, size_t state_len, uint8_t *status_out)
{
    if (did == DID_STATUS_LED) {
        if (control_param == 0x00) { /* returnControlToECU */
            *status_out = app_led_state;
            return 0;
        } else if (control_param == 0x03) { /* shortTermAdjustment */
            if (state_len < 1) return -EINVAL;
            app_led_state = control_state[0];
            *status_out = app_led_state;
            return 0;
        }
        return -EOPNOTSUPP;
    }
    return -ENOENT;
}

/* Implementierung: Routine-Start */
int uds_app_routine_start(uint16_t routine_id, uint8_t *info_out)
{
    if (routine_id == RID_ERASE_FLASH) {
        *info_out = 0x01; /* Routine läuft an */
        return 0;
    }
    return -ENOENT;
}

/* Implementierung: Routine-Ergebnis abfragen */
int uds_app_routine_request_results(uint16_t routine_id, uint8_t *status_out, uint8_t *exit_info_out)
{
    if (routine_id == RID_ERASE_FLASH) {
        *status_out = 0x00; /* Fertig */
        *exit_info_out = 0x00; /* Erfolg */
        return 0;
    }
    return -ENOENT;
}
