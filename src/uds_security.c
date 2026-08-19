/**
 * @file uds_security.c
 * @brief Security Access (0x27) mit echtem Hardware-RNG (Entropy Driver) für Zephyr v4.4.0
 */

#include "uds_security.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/entropy.h> /* Neuer Header für Hardware-Entropie */
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_DECLARE(uds_server, LOG_LEVEL_INF);

#define SECURITY_SEED_SIZE   CONFIG_UDS_SEC_SEED_SIZE
#define SECURITY_SECRET_MASK CONFIG_UDS_SEC_SECRET_MASK
#define MAX_FAILED_ATTEMPTS  CONFIG_UDS_SEC_MAX_FAILED_ATTEMPTS
#define LOCKOUT_TIME_MS      CONFIG_UDS_SEC_LOCKOUT_TIME_MS

static uds_security_status_t security_status = UDS_SEC_LOCKED;
static uint8_t generated_seed[SECURITY_SEED_SIZE];
static uint8_t failed_attempts_counter = 0;
static struct k_timer lockout_timer;

/* Device Pointer für den Hardware-RNG */
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

    /* Validierung des Hardware-Zufallsgenerators beim Systemstart */
    if (!device_is_ready(entropy_dev)) {
        LOG_ERR("Hardware Entropy/RNG Device ist nicht bereit! Seeds sind unsicher.");
    } else {
        LOG_INF("Hardware Entropy/RNG erfolgreich initialisiert.");
    }
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

static uint32_t calculate_key_from_seed(const uint8_t *seed)
{
    /* Zuverlässiges und compilerunabhängiges Shifting über Byte-Array */
    uint32_t seed_val = ((uint32_t)seed[0] << 24) | 
                        ((uint32_t)seed[1] << 16) | 
                        ((uint32_t)seed[2] << 8)  | 
                        (uint32_t)seed[3];
    return seed_val ^ SECURITY_SECRET_MASK;
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

    if (sub_function == 0x01) { /* Request Seed */
        if (len != 2) {
            nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
            return;
        }

        if (security_status == UDS_SEC_UNLOCKED) {
            memset(generated_seed, 0, SECURITY_SEED_SIZE);
        } else if (security_status != UDS_SEC_SEED_REQUESTED) {
            
            /* ECHTEN HARDWARE-ZUFALL ABRUFEN */
            if (device_is_ready(entropy_dev)) {
                int ret = entropy_get_entropy(entropy_dev, generated_seed, SECURITY_SEED_SIZE);
                if (ret < 0) {
                    LOG_ERR("Fehler beim Lesen des Hardware-RNG (%d). Nutze Fallback.", ret);
                    /* Sicheres Fallback auf System-Uptime, falls der HW-RNG im Betrieb versagt */
                    uint32_t uptime = k_uptime_get_32();
                    for (int i = 0; i < SECURITY_SEED_SIZE; i++) {
                        generated_seed[i] = (uint8_t)(uptime >> (i * 8));
                    }
                }
            } else {
                /* Totales Fallback falls Device zur Laufzeit offline ist */
                uint32_t uptime = k_uptime_get_32();
                for (int i = 0; i < SECURITY_SEED_SIZE; i++) {
                    generated_seed[i] = (uint8_t)(uptime >> (i * 8));
                }
            }


            /* ISO 14229 Konformität: Ein gültiger Seed im gesperrten Zustand darf niemals rein 0x00 sein */
            bool is_all_zero = true;
            for (int i = 0; i < SECURITY_SEED_SIZE; i++) {
                if (generated_seed[i] != 0x00) {
                    is_all_zero = false;
                    break;
                }
            }
            if (is_all_zero) {
                generated_seed[0] = 0x42; /* Setze ein fixes Byte falls RNG exakt 0 liefert */
            }

            security_status = UDS_SEC_SEED_REQUESTED;
        }

        tx_buf[0] = sid + 0x40;
        tx_buf[1] = sub_function;
        memcpy(&tx_buf[2], generated_seed, SECURITY_SEED_SIZE);
        send_cb(tx_buf, 2 + SECURITY_SEED_SIZE);

    } else if (sub_function == 0x02) { /* Send Key */
        if (len != (2 + SECURITY_SEED_SIZE)) {
            nrc_cb(sid, UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT);
            return;
        }

        if (security_status != UDS_SEC_SEED_REQUESTED) {
            nrc_cb(sid, UDS_NRC_REQUEST_SEQUENCE_ERROR);
            return;
        }

        uint32_t received_key = ((uint32_t)req[2] << 24) | 
                                ((uint32_t)req[3] << 16) | 
                                ((uint32_t)req[4] << 8)  | 
                                (uint32_t)req[5];
                                
        uint32_t expected_key = calculate_key_from_seed(generated_seed);

        if (received_key == expected_key) {
            security_status = UDS_SEC_UNLOCKED;
            failed_attempts_counter = 0;
            LOG_INF("Security Access erfolgreich entsperrt.");
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
