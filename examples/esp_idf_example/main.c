#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "mac_table.h"

static const char *TAG = "example";

// Callback function for MAC table events
void mac_table_event_callback(size_t slot_index, mac_entry_result_t status) {
    switch (status) {
        case MAC_TABLE_INSERTED:
            ESP_LOGE(TAG, "MAC address inserted at slot %d\n", (int)slot_index);
            break;
        case MAC_TABLE_UPDATED:
            ESP_LOGE(TAG, "MAC address updated at slot %d\n", (int)slot_index);
            break;
        case MAC_TABLE_DELETED:
            ESP_LOGE(TAG, "MAC address deleted at slot %d\n", (int)slot_index);
            break;
	    case MAC_TABLE_FULL;
            ESP_LOGE(TAG, "MAC address is full");
            break;
        default:
            break;
    }
}

void app_main() {
    // Create the MAC table
    mac_entry_t mac_entries[10];  // Array of MAC entries (size 10)
    mac_table_t table;
    mac_table_init(&table, mac_entries, 10, 60, mac_table_event_callback);  // 60 seconds expiry

    // Example MAC address (replace with actual MAC)
    uint8_t mac[MAC_ADDR_LEN] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};

    // Insert the MAC address
    mac_entry_result_t result = mac_table_insert(&table, mac);
    if (result == MAC_TABLE_INSERTED) {
        printf("MAC address inserted successfully.\n");
    }

    // Check if the MAC address exists
    result = mac_table_exists(&table, mac);
    if (result == MAC_TABLE_OK) {
        printf("MAC address exists in the table.\n");
    } else {
        printf("MAC address not found.\n");
    }
}

