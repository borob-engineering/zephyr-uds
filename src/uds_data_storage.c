/**
 * @file uds_data_storage.c
 * @brief Implementierung des zentralen RAM-Speichers für Fahrzeugdaten
 */

#include "uds_data_storage.h"
#include <string.h>

static uint8_t vehicle_vin[VIN_SIZE];

void uds_data_storage_init(void)
{
    /* Standard-VIN beim Booten initialisieren */
    memcpy(vehicle_vin, "ZEPHYRISANRTOS123", VIN_SIZE);
}

const uint8_t *uds_data_storage_get_vin(void)
{
    return vehicle_vin;
}

int uds_data_storage_set_vin(const uint8_t *new_vin, size_t len)
{
    if (len != VIN_SIZE || new_vin == NULL) {
        return -1;
    }
    memcpy(vehicle_vin, new_vin, VIN_SIZE);
    return 0;
}
