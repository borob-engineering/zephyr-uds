/**
 * @file uds_clear_dtc.c
 * @brief UDS Service 0x14 (Clear Diagnostic Information) mit asynchronem NRC 0x78 Response Pending Handling
 */

#include "uds_clear_dtc.h"
#include "uds_read_dtc.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(uds_server, LOG_LEVEL_INF);

static struct k_work clear_dtc_work;
static uint32_t target_dtc_group = 0;
static uint8_t local_tx_buf[8];

/* Zwischengespeicherte Callbacks für den asynchronen Worker */
static void (*stored_send_cb)(const uint8_t *, size_t) = NULL;
static void (*stored_nrc_cb)(uint8_t, uint8_t) = NULL;

static void clear_dtc_worker_handler(struct k_work *work)
{
    ARG_UNUSED(work);
    LOG_INF("Asynchroner Flash-Löschvorgang aktiv (DTC-Gruppe: 0x%06X)...", target_dtc_group);
    
    /* Simuliere eine lang andauernde Hardware-Operation (z.B. 1,5 Sekunden Sektor-Löschung) */
    k_msleep(1500); 

    /* Interne RAM-Datenbank bereinigen */
    uds_read_dtc_clear_all(target_dtc_group);

    LOG_INF("Löschen beendet. Sende finale UDS Bestätigung.");

    if (stored_send_cb != NULL) {
        /* FINALE POSITIVE ANTWORT ERST JETZT SENDEN: SID + 0x40 = 0x54 */
        local_tx_buf[0] = 0x54;
        stored_send_cb(local_tx_buf, 1);
    }
}

void uds_clear_dtc_init(void)
{
    k_work_init(&clear_dtc_work, clear_dtc_worker_handler);
}

void uds_clear_dtc_handle(uint8_t *req, size_t len, 
                          void (*send_cb)(const uint8_t *, size_t), 
                          void (*nrc_cb)(uint8_t, uint8_t))
{
    uint8_t sid = req[0];
    
    if (len != 4) {
        nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
        return;
    }

    if (k_work_is_pending(&clear_dtc_work)) {
        nrc_cb(sid, UDS_NRC_CONDITIONS_NOT_CORRECT);
        return;
    }

    target_dtc_group = ((uint32_t)req[1] << 16) | 
                       ((uint32_t)req[2] << 8)  | 
                       ((uint32_t)req[3]);

    /* Callbacks für den Hintergrund-Task sichern */
    stored_send_cb = send_cb;
    stored_nrc_cb = nrc_cb;

    /* 1. SOFORT TRENNEN & RESPONSE PENDING (NRC 0x78) SENDEN */
    LOG_INF("Clear DTC erfordert Zeit. Sende NRC 0x78 (Response Pending).");
    nrc_cb(sid, UDS_NRC_RESPONSE_PENDING);

    /* 2. Den eigentlichen Lösch-Task in den Hintergrund schieben */
    k_work_submit(&clear_dtc_work);
}
