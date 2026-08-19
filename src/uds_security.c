/**
 * @file uds_security.c
 * @brief Generischer Security-Access Treiber
 */

#include "uds_security.h"
#include "uds_app_interface.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/entropy.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_DECLARE(uds_server, LOG_LEVEL_INF);

#define SECURITY_SEED_SIZE   CONFIG_UDS_SEC_SEED_SIZE
#define MAX_FAILED_ATTEMPTS  CONFIG_UDS_SEC_MAX_FAILED_ATTEMPTS
#define LOCKOUT_TIME_MS      CONFIG_UDS_SEC_LOCKOUT_TIME_MS

static uds_security_status_t security_status = UDS_SEC_LOCKED;
static uint8_t generated_seed[SECURITY_SEED_SIZE];
static uint8_t failed_attempts_counter = 0;
static struct k_timer lockout_timer;

static const struct device *entropy_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_entropy));

static void lockout_timer_expiry_cb(struct k_timer *timer)
{
    ARG_UNUSED(timer);
    security_status = UDS_SEC_LOCKED;
    failed_attempts_counter = 0;
    LOG_INF("Anti-Brute-Force Sperre abgelaufen.");
}

void uds_security_init(void)
{
    k_timer_init(&lockout_timer, lockout_timer_expiry_cb, NULL);
}

uds_security_status_t uds_security_get_status(void)
{
    return security_status;
}

void uds_security_reset_lock(void)
{
    if (security_status != UDS_SEC_BRUTE_FORCE_LOCKOUT) {
        security_status = UDS_SEC_LOCKED;
    }
}

void uds_security_handle_request(uint8_t *req, size_t len, uint8_t *tx_buf, 
                                 void (*send_cb)(const uint8_t *, size_t), 
                                 void (*nrc_cb)(uint8_t, uint8_t))
{
    uint8_t sid = req[0];
    if (len < 2) {
        nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
        return;
    }

    if (security_status == UDS_SEC_BRUTE_FORCE_LOCKOUT) {
        nrc_cb(sid, UDS_NRC_EXCEEDED_NUMBER_OF_ATTEMPTS);
        return;
    }

    uint8_t sub_function = req[1];

    if (sub_function == 0x01) {
        if (len != 2) {
            nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
            return;
        }

        if (security_status == UDS_SEC_UNLOCKED) {
            memset(generated_seed, 0, SECURITY_SEED_SIZE);
        } else if (security_status != UDS_SEC_SEED_REQUESTED) {
            if (device_is_ready(entropy_dev)) {
                if (entropy_get_entropy(entropy_dev, generated_seed, SECURITY_SEED_SIZE) < 0) {
                    uint32_t uptime = k_uptime_get_32();
                    for (int i = 0; i < SECURITY_SEED_SIZE; i++) generated_seed[i] = (uint8_t)(uptime >> (i * 8));
                }
            } else {
                uint32_t uptime = k_uptime_get_32();
                for (int i = 0; i < SECURITY_SEED_SIZE; i++) generated_seed[i] = (uint8_t)(uptime >> (i * 8));
            }
            security_status = UDS_SEC_SEED_REQUESTED;
        }

        tx_buf[0] = sid + 0x40;
        tx_buf[1] = sub_function;
        memcpy(&tx_buf[2], generated_seed, SECURITY_SEED_SIZE);
        send_cb(tx_buf, 2 + SECURITY_SEED_SIZE);

    } else if (sub_function == 0x02) {
        if (len != (2 + SECURITY_SEED_SIZE)) {
            nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
            return;
        }

        if (security_status != UDS_SEC_SEED_REQUESTED) {
            nrc_cb(sid, UDS_NRC_REQUEST_SEQUENCE_ERROR);
            return;
        }

        uint32_t received_key = ((uint32_t)req[2] << 24) | ((uint32_t)req[3] << 16) | 
                                ((uint32_t)req[4] << 8)  | (uint32_t)req[5];
                                
        /* ABSTRAKTION: Aufruf des Applikations-Keys */
        uint32_t expected_key = uds_app_calculate_key(generated_seed, SECURITY_SEED_SIZE);

        if (received_key == expected_key) {
            security_status = UDS_SEC_UNLOCKED;
            failed_attempts_counter = 0;
            tx_buf[0] = sid + 0x40;
            tx_buf[1] = sub_function;
            send_cb(tx_buf, 2);
        } else {
            failed_attempts_counter++;
            if (failed_attempts_counter >= MAX_FAILED_ATTEMPTS) {
                security_status = UDS_SEC_BRUTE_FORCE_LOCKOUT;
                k_timer_start(&lockout_timer, K_MSEC(LOCKOUT_TIME_MS), K_NO_WAIT);
                nrc_cb(sid, UDS_NRC_EXCEEDED_NUMBER_OF_ATTEMPTS);
            } else {
                security_status = UDS_SEC_LOCKED;
                nrc_cb(sid, UDS_NRC_INVALID_KEY);
            }
        }
    } else {
        nrc_cb(sid, UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED);
    }
}
