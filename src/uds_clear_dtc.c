/**
 * @file uds_clear_dtc.c
 * @brief UDS Service 0x14 mit gekoppelter RAM-Clear-Funktion
 */

#include "uds_clear_dtc.h"
#include "uds_read_dtc.h" /* NEU: Zugriff auf das Gegenstück */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(uds_server, LOG_LEVEL_INF);

static struct k_work clear_dtc_work;
static uint32_t target_dtc_group = 0;

static void clear_dtc_worker_handler(struct k_work *work)
{
    ARG_UNUSED(work);
    LOG_INF("Asynchroner Löschvorgang gestartet...");
    
    /* Simuliere Verarbeitungszeit */
    k_msleep(50); 

    /* ECHTE INTERNE LÖSCHUNG DER RAM-DATEN AUFRUFEN */
    uds_read_dtc_clear_all(target_dtc_group);

    LOG_INF("Asynchroner Löschvorgang erfolgreich beendet.");
}

void uds_clear_dtc_init(void)
{
    k_work_init(&clear_dtc_work, clear_dtc_worker_handler);
}

void uds_clear_dtc_handle(uint8_t *req, size_t len, uint8_t *tx_buf, 
                          void (*send_cb)(const uint8_t *, size_t), 
                          void (*nrc_cb)(uint8_t, uint8_t))
{
    uint8_t sid = req[0];
    
    if (len != 4) {
        nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
        return;
    }

    target_dtc_group = ((uint32_t)req[1] << 16) | 
                       ((uint32_t)req[2] << 8)  | 
                       ((uint32_t)req[3]);

    if (k_work_is_pending(&clear_dtc_work)) {
        nrc_cb(sid, UDS_NRC_CONDITIONS_NOT_CORRECT);
        return;
    }

    k_work_submit(&clear_dtc_work);

    tx_buf[0] = sid + 0x40;
    send_cb(tx_buf, 1);
}
