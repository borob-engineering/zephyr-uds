/*
 * Copyright (c) 2026 borob-engineering
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Common definitions, Negative Response Codes (NRCs), and status types for the UDS subsystem.
 */

#ifndef ZEPHYR_INCLUDE_CANBUS_UDS_TYPES_H_
#define ZEPHYR_INCLUDE_CANBUS_UDS_TYPES_H_

#include <stdint.h>
#include <stddef.h>
#include <zephyr/types.h>

/** @brief Global UDS buffer size derived from Kconfig. */
#define UDS_BUFF_SIZE CONFIG_UDS_BUFF_SIZE

/** @brief Standard length of a Vehicle Identification Number (VIN). */
#define VIN_SIZE 17

/*
 * ISO 14229-1 Standard Negative Response Codes (NRC)
 */
#define UDS_NRC_SERVICE_NOT_SUPPORTED                     0x11
#define UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED                0x12
#define UDS_NRC_INCORRECT_LENGTH_OR_INVALID_FORMAT        0x13
#define UDS_NRC_CONDITIONS_NOT_CORRECT                    0x22
#define UDS_NRC_REQUEST_SEQUENCE_ERROR                    0x24
#define UDS_NRC_REQUEST_OUT_OF_RANGE                      0x31
#define UDS_NRC_SECURITY_ACCESS_DENIED                    0x33
#define UDS_NRC_INVALID_KEY                               0x35
#define UDS_NRC_EXCEEDED_NUMBER_OF_ATTEMPTS               0x36
#define UDS_NRC_UPLOAD_DOWNLOAD_NOT_ACCEPTED             0x70
#define UDS_NRC_TRANSFER_DATA_SUSPENDED                   0x71
#define UDS_NRC_WRONG_BLOCK_SEQUENCE_COUNTER              0x73
#define UDS_NRC_RESPONSE_PENDING                          0x78
#define UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED_IN_ACTIVE_SESS 0x7E
#define UDS_NRC_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION   0x7F

/**
 * @brief UDS Addressing Type.
 */
typedef enum {
	/** 1-to-1 communication with a specific ECU */
	UDS_ADDR_PHYSICAL,
	/** 1-to-many communication across the entire network */
	UDS_ADDR_FUNCTIONAL
} uds_addressing_t;

/**
 * @brief UDS Diagnostic Session Types (Service 0x10).
 */
typedef enum {
	/** Default session started on ECU boot */
	UDS_SESSION_DEFAULT = 0x01,
	/** Session dedicated to bootloader/firmware flashing operations */
	UDS_SESSION_PROGRAMMING = 0x02,
	/** Extended session for physical adjustments and advanced diagnostics */
	UDS_SESSION_EXTENDED = 0x03
} uds_session_type_t;

/**
 * @brief Security Access State Machine (Service 0x27).
 */
typedef enum {
	/** Security is locked (default state) */
	UDS_SEC_LOCKED,
	/** Seed was sent to tester, awaiting key validation */
	UDS_SEC_SEED_REQUESTED,
	/** Security is unlocked for protected operations */
	UDS_SEC_UNLOCKED,
	/** Locked out due to consecutive failed key attempts */
	UDS_SEC_BRUTE_FORCE_LOCKOUT
} uds_security_status_t;

/**
 * @brief Routine Control Execution States (Service 0x31).
 */
typedef enum {
	/** Routine is inactive or ready to start */
	ROUTINE_IDLE,
	/** Routine background work queue task is currently executing */
	ROUTINE_RUNNING,
	/** Routine finished successfully */
	ROUTINE_COMPLETED,
	/** Routine finished with errors */
	ROUTINE_FAILED
} routine_status_t;

/**
 * @brief Diagnostic Trouble Code (DTC) Representation (Service 0x19/0x14).
 */
typedef struct {
	/** 3-byte array containing the unique DTC code */
	uint8_t code[3];
	/** ISO 14229-1 compliant status byte bitmask */
	uint8_t status;
} uds_dtc_t;

#endif /* ZEPHYR_INCLUDE_CANBUS_UDS_TYPES_H_ */
