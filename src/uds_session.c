/**
 * @file uds_session.c
 * @brief Implementierung des Session-Managements und der S3-Timeout Überwachung
 */

#include "uds_session.h"
#include "uds_security.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(uds_server, LOG_LEVEL_INF);

static uds_session_type_t current_session = UDS_SESSION_DEFAULT;
static struct k_timer s3_timer;

static void s3_timer_expiry_cb(struct k_timer *timer)
{
    ARG_UNUSED(timer);
    
    if (current_session != UDS_SESSION_DEFAULT) {
        LOG_WRN("S3-Timeout abgelaufen! Keine Tester-Aktivität erkannt.");
        current_session = UDS_SESSION_DEFAULT;
        uds_security_reset_lock();
        LOG_INF("Automatisch in DEFAULT_SESSION gewechselt und Security gesperrt.");
    }
}

void uds_session_init(void)
{
    k_timer_init(&s3_timer, s3_timer_expiry_cb, NULL);
    current_session = UDS_SESSION_DEFAULT;
}

void uds_session_set(uds_session_type_t new_session)
{
    current_session = new_session;
    uds_session_refresh_timer();
}

uds_session_type_t uds_session_get(void)
{
    return current_session;
}

void uds_session_refresh_timer(void)
{
    if (current_session != UDS_SESSION_DEFAULT) {
        k_timer_start(&s3_timer, K_MSEC(CONFIG_UDS_S3_TIMEOUT_MS), K_NO_WAIT);
    } else {
        k_timer_stop(&s3_timer);
    }
}
