/**
 * @file uds_routine.c
 * @brief Generische Routine Control Engine (0x31)
 */

#include "uds_routine.h"
#include "uds_session.h"
#include "uds_app_interface.h" /* KORREKTUR: Fehlendes Include für die App-Schnittstellen hinzugefügt */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_DECLARE(uds_server, LOG_LEVEL_INF);

#define ROUTINE_ID_ERASE_MEMORY 0xFF00

static volatile routine_status_t erase_routine_status = ROUTINE_IDLE;
static uint8_t routine_exit_info = 0x00;
static uint16_t active_routine_id = 0;
static struct k_work routine_work;

/* Puffer für die finale positive Antwort des Workers */
static uint8_t local_routine_tx_buf[5];
static void (*stored_routine_send_cb)(const uint8_t *, size_t) = NULL;

static void routine_worker_handler(struct k_work *work)
{
    ARG_UNUSED(work);
    uint8_t app_info = 0;
    
    /* Aufruf der zeitkritischen Applikationsroutine */
    int ret = uds_app_routine_start(active_routine_id, &app_info);

    if (ret == 0) {
        erase_routine_status = ROUTINE_COMPLETED;
        routine_exit_info = app_info;
    } else {
        erase_routine_status = ROUTINE_FAILED;
        routine_exit_info = 0x0F;
    }

    if (stored_routine_send_cb != NULL) {
        local_routine_tx_buf[0] = 0x71;
        local_routine_tx_buf[1] = 0x01;
        local_routine_tx_buf[2] = (uint8_t)(active_routine_id >> 8);
        local_routine_tx_buf[3] = (uint8_t)(active_routine_id & 0xFF);
        local_routine_tx_buf[4] = (erase_routine_status == ROUTINE_COMPLETED) ? 0x00 : 0x02;
        
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
    if (len == 0 || req == NULL) return;
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

    switch (sub_function) {
    case 0x01: /* startRoutine */
        if (erase_routine_status == ROUTINE_RUNNING) {
            nrc_cb(sid, UDS_NRC_CONDITIONS_NOT_CORRECT);
            return;
        }
        
        active_routine_id = routine_id;
        erase_routine_status = ROUTINE_RUNNING;
        stored_routine_send_cb = send_cb;

        nrc_cb(sid, UDS_NRC_RESPONSE_PENDING);
        k_work_submit(&routine_work);
        break;

    case 0x03: /* requestRoutineResults */
        {
            uint8_t app_status = 0, app_exit = 0;
            int ret = uds_app_routine_request_results(routine_id, &app_status, &app_exit);
            if (ret != 0) {
                nrc_cb(sid, UDS_NRC_REQUEST_OUT_OF_RANGE);
                return;
            }
            
            /* Temporärer lokaler Stack-Puffer für die synchrone Statusantwort (6 Bytes) */
            uint8_t sync_tx_buf[6];
            sync_tx_buf[0] = sid + 0x40;
            sync_tx_buf[1] = sub_function;
            sync_tx_buf[2] = req[2];
            sync_tx_buf[3] = req[3];
            sync_tx_buf[4] = app_status;
            sync_tx_buf[5] = app_exit;
            
            send_cb(sync_tx_buf, 6);
        }
        break;

    default:
        nrc_cb(sid, UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED);
        break;
    }
}
