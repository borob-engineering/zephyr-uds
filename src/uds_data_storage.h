/**
 * @file uds_data_storage.h
 * @brief Zentraler RAM-Speicher für Fahrzeugdaten (z.B. VIN)
 */

#ifndef UDS_DATA_STORAGE_H_
#define UDS_DATA_STORAGE_H_

#include <zephyr/types.h>
#include <stddef.h>

#define VIN_SIZE 17

void uds_data_storage_init(void);
const uint8_t *uds_data_storage_get_vin(void);
int uds_data_storage_set_vin(const uint8_t *new_vin, size_t len);

#endif /* UDS_DATA_STORAGE_H_ */
