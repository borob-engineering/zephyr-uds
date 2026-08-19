/**
 * @file uds_routine.c
 * @brief Asynchrone Routine Control Engine (0x31) mit integriertem NRC 0x78 Response Pending Handling
 */

#include "uds_routine.h"
#include "uds_session.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(uds_server, LOG_LEVEL_INF);

#define ROUTINE_ID_ERASE_MEMORY 0xFF00

static volatile routine_status_t erase_routine_status = ROUTINE_IDLE;
static uint8_t routine_exit_info = 0x00;
static struct k_work routine_work;

/* Lokaler Puffer für die finale positive Antwort des Workers */
static uint8_t local_routine_tx_buf[5];

/* Gespeicherte Funktionszeiger für die asynchrone Kommunikation */
static void (*stored_routine_send_cb)(const uint8_t *, size_t) = NULL;
static void (*stored_routine_nrc_cb)(uint8_t, uint8_t) = NULL;

static void routine_worker_handler(struct k_work *work)
{
    ARG_UNUSED(work);
    LOG_INF("Asynchroner Routine-Worker: Bereite Hardware-Operation vor...");
    
    /* Simuliere eine lang andauernde Vorbereitungsphase (z.B. 2 Sekunden Hardware-Sperrung) */
    k_msleep(2000); 

    erase_routine_status = ROUTINE_COMPLETED;
    routine_exit_info = 0x00;
    
    LOG_INF("Routine-Initialisierung abgeschlossen. Sende finale positive Antwort.");

    if (stored_routine_send_cb != NULL) {
        /* FINALE POSITIVE ANTWORT ERST JETZT ABSETZEN:
         * SID + 0x40 (0x71) + Sub-Function (0x01) + RID High (0xFF) + RID Low (0x00) + Status (0x00 = Fertig) */
        local_routine_tx_buf[0] = 0x71;
        local_routine_tx_buf[1] = 0x01;
        local_routine_tx_buf[2] = 0xFF;
        local_routine_tx_buf[3] = 0x00;
        local_routine_tx_buf[4] = 0x00; /* routineStatus: completed/finished */
        
        stored_routine_send_cb(local_routine_tx_buf, 5);
    }
}

void uds_routine_init(void)
{
    k_work_init(&routine_work, routine_worker_handler);
}

void uds_routine_handle_control(uint8_t *req, size_t len, 
                                void (*send_cb)(const uint8_t *, size_t), 
                                void (*nrc_cb)(uint8_t, uint8_t))
{
    if (len == 0 || req == NULL) {
        return;
    }

    uint8_t sid = req[0];

    if (uds_session_get() == UDS_SESSION_DEFAULT) {
        nrc_cb(sid, UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED_IN_ACTIVE_SESS);
        return;
    }

    if (len < 4) {
        nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
        return;
    }

    uint8_t sub_function = req[1];
    uint16_t routine_id = ((uint16_t)req[2] << 8) | req[3];

    if (routine_id != ROUTINE_ID_ERASE_MEMORY) {
        nrc_cb(sid, UDS_NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    switch (sub_function) {
    case 0x01: /* startRoutine */
        if (erase_routine_status == ROUTINE_RUNNING) {
            nrc_cb(sid, UDS_NRC_CONDITIONS_NOT_CORRECT);
            return;
        }
        
        erase_routine_status = ROUTINE_RUNNING;
        
        /* Callbacks für den asynchronen Worker-Thread sichern */
        stored_routine_send_cb = send_cb;
        stored_routine_nrc_cb = nrc_cb;

        /* 1. TIMEOUT VERHINDERN: Sofort ein NRC 0x78 (Response Pending) senden */
        LOG_INF("Routine 0xFF00 benötigt Verarbeitungszeit. Sende NRC 0x78.");
        nrc_cb(sid, UDS_NRC_RESPONSE_PENDING);

        /* 2. Worker in den Hintergrund schieben */
        k_work_submit(&routine_work);
        break;

    case 0x03: /* requestRoutineResults */
        /* Das Abfragen des Status läuft synchron, da es sofort den aktuellen Zustand zurückgeben kann */
        local_routine_tx_buf[0] = sid + 0x40;
        local_routine_tx_buf[1] = sub_function;
        local_routine_tx_buf[2] = req[2];
        local_routine_tx_buf[3] = req[3];
        local_routine_tx_buf[4] = (erase_routine_status == ROUTINE_RUNNING) ? 0x01 : 
                                  (erase_routine_status == ROUTINE_COMPLETED) ? 0x00 : 0x02;
        
        /* Optionales sechstes Byte für Zusatzinformationen anhängen */
        uint8_t temp_buf[6];
        memcpy(temp_buf, local_routine_tx_buf, 5);
        temp_buf[5] = routine_exit_info;
        
        send_cb(temp_buf, 6);
        break;

    default:
        nrc_cb(sid, UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED);
        break;
    }
}
