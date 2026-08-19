/**
 * @file uds_types.h
 * @brief Gemeinsame Definitionen, NRCs und Statustypen für das UDS-Subsystem
 */

#ifndef UDS_TYPES_H_
#define UDS_TYPES_H_

#include <zephyr/types.h>
#include <stddef.h>

#define UDS_BUFF_SIZE CONFIG_UDS_BUFF_SIZE
#define VIN_SIZE 17

/* ISO 14229-1 Standard Negative Response Codes (NRC) */
#define UDS_NRC_SERVICE_NOT_SUPPORTED                    0x11
#define UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED               0x12
#define UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT       0x13
#define UDS_NRC_CONDITIONS_NOT_CORRECT                   0x22
#define UDS_NRC_REQUEST_SEQUENCE_ERROR                   0x24
#define UDS_NRC_REQUEST_OUT_OF_RANGE                     0x31
#define UDS_NRC_SECURITY_ACCESS_DENIED               0x33
#define UDS_NRC_INVALID_KEY                              0x35
#define UDS_NRC_EXCEEDED_NUMBER_OF_ATTEMPTS          0x36
#define UDS_NRC_UPLOAD_DOWNLOAD_NOT_ACCEPTED            0x70
#define UDS_NRC_TRANSFER_DATA_SUSPENDED                  0x71
#define UDS_NRC_WRONG_BLOCK_SEQUENCE_COUNTER             0x73
#define UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED_IN_ACTIVE_SESS 0x7E
#define UDS_NRC_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION  0x7F

typedef enum {
    UDS_ADDR_PHYSICAL,
    UDS_ADDR_FUNCTIONAL
} uds_addressing_t;

typedef enum {
    UDS_SESSION_DEFAULT = 0x01,
    UDS_SESSION_PROGRAMMING = 0x02,
    UDS_SESSION_EXTENDED = 0x03
} uds_session_type_t;

typedef enum {
    UDS_SEC_LOCKED,
    UDS_SEC_SEED_REQUESTED,
    UDS_SEC_UNLOCKED,
    UDS_SEC_BRUTE_FORCE_LOCKOUT
} uds_security_status_t;

typedef enum {
    ROUTINE_IDLE,
    ROUTINE_RUNNING,
    ROUTINE_COMPLETED,
    ROUTINE_FAILED
} routine_status_t;

typedef struct {
    uint8_t code[3];
    uint8_t status;
} uds_dtc_t;

#endif /* UDS_TYPES_H_ */
