/**
 * @file uds_weak_defaults.c
 * @brief Sichere Standard-Fallbacks (__weak) des generischen UDS-Stacks
 */

#include "uds_app_interface.h"
#include <zephyr/toolchain.h>
#include <errno.h>

/* Fallback für das Lesen von DIDs: Melde standardmäßig "Nicht gefunden" */
__weak int uds_app_read_did(uint16_t did, uint8_t *data_out, size_t *len_out, size_t max_len)
{
    return -ENOENT; 
}

/* Fallback für das Schreiben von DIDs: Melde standardmäßig "Nicht erlaubt" */
__weak int uds_app_write_did(uint16_t did, const uint8_t *data_in, size_t len)
{
    return -EACCES;
}

/* Fallback für Security-Key-Berechnung: Blockiert das Entsperren im Auslieferungszustand */
__weak uint32_t uds_app_calculate_key(const uint8_t *seed, size_t len)
{
    return 0x00000000;
}

/* Fallback für Stellgliedtests (IO-Control) */
__weak int uds_app_io_control(uint16_t did, uint8_t control_param, const uint8_t *control_state, size_t state_len, uint8_t *status_out)
{
    return -ENOENT;
}

/* Fallback für Routinen-Start */
__weak int uds_app_routine_start(uint16_t routine_id, uint8_t *info_out)
{
    return -ENOENT;
}

/* Fallback für Routinen-Abfrage */
__weak int uds_app_routine_request_results(uint16_t routine_id, uint8_t *status_out, uint8_t *exit_info_out)
{
    return -ENOENT;
}
