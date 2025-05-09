#include "mac_table.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MAC_TABLE_SIZE 5
#define MAC_ADDR_LEN 6

static const char *TAG = "MAC_TABLE_TEST";

mac_table_t mac_table;

// Dummy callback function to log events
void mac_table_event_callback(int slot_index, const uint8_t *mac, mac_entry_result_t status) {
    char mac_str[18];
    mac_to_str(mac, mac_str);

    switch (status) {
        case MAC_TABLE_INSERTED:
            ESP_LOGI(TAG, "Inserted MAC address %s at slot %d", mac_str, slot_index);
            break;
        case MAC_TABLE_UPDATED:
            ESP_LOGI(TAG, "Updated MAC address %s at slot %d", mac_str, slot_index);
            break;
        case MAC_TABLE_DELETED:
            ESP_LOGI(TAG, "Deleted MAC address %s from slot %d", mac_str, slot_index);
            break;
        case MAC_TABLE_TIMEOUT:
            ESP_LOGI(TAG, "MAC address %s at slot %d expired", mac_str, slot_index);
            break;
        case MAC_TABLE_FULL:
            ESP_LOGE(TAG, "MAC table full, could not insert MAC %s", mac_str);
            break;
        default:
            ESP_LOGI(TAG, "Unknown event for MAC %s at slot %d", mac_str, slot_index);
            break;
    }

    // Log the MAC table stats
    ESP_LOGI(TAG, "MAC Table Stats: Total Inserts: %d, Total Deletes: %d, Total Expired: %d, Active Entries: %d", 
        mac_table.stats->total_inserts, 
        mac_table.stats->total_deletes, 
        mac_table.stats->total_expired, 
        mac_table.stats->active_entries);
}

void app_main(void) {
    // Initialize the MAC address table
    mac_entry_t mac_table_entries[MAC_TABLE_SIZE];
    mac_table_init(&mac_table, mac_table_entries, MAC_TABLE_SIZE, 30, mac_table_event_callback);

    // Test data
    uint8_t test_mac_1[MAC_ADDR_LEN] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
    uint8_t test_mac_2[MAC_ADDR_LEN] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5F};
    uint8_t test_mac_3[MAC_ADDR_LEN] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x60};
    uint8_t test_mac_4[MAC_ADDR_LEN] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x61};
    uint8_t test_mac_5[MAC_ADDR_LEN] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x62};
    uint8_t test_mac_6[MAC_ADDR_LEN] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x63}; // Will trigger table full

    // 1. Insert MAC addresses
    ESP_LOGI(TAG, "Inserting test MAC addresses...");
    mac_table_insert(&mac_table, test_mac_1);
    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second

    mac_table_insert(&mac_table, test_mac_2);
    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second

    mac_table_insert(&mac_table, test_mac_3);
    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second

    mac_table_insert(&mac_table, test_mac_4);
    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second

    mac_table_insert(&mac_table, test_mac_5);  // This should succeed
    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second

    mac_table_insert(&mac_table, test_mac_6);  // This should fail (table full)
    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second

    // 2. Check if MAC addresses exist
    ESP_LOGI(TAG, "Checking MAC addresses...");
    mac_table_exists(&mac_table, test_mac_1); // Should exist
    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second

    mac_table_exists(&mac_table, test_mac_6); // Should not exist
    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second

    // 3. Retrieve MAC address entries
    ESP_LOGI(TAG, "Retrieving MAC addresses...");
    mac_entry_t retrieved_entry;
    if (mac_table_get_by_index(&mac_table, 0, &retrieved_entry) == MAC_TABLE_OK) {
        char mac_str[18];
        mac_to_str(retrieved_entry.mac, mac_str);
        ESP_LOGI(TAG, "Entry at index 0: %s", mac_str);
    }
    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second

    // 4. Update a MAC address
    ESP_LOGI(TAG, "Updating MAC address...");
    uint8_t updated_mac[MAC_ADDR_LEN] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};  // Re-insert same MAC
    mac_table_insert(&mac_table, updated_mac);  // Should trigger an update event
    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second

    // 5. Delete a MAC address
    ESP_LOGI(TAG, "Deleting MAC address...");
    mac_table_delete(&mac_table, test_mac_3); // Should delete successfully
    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second

    // 6. Check table status after deletion
    ESP_LOGI(TAG, "Checking MAC addresses again...");
    if(mac_table_exists(&mac_table, test_mac_3) == MAC_TABLE_NOT_FOUND) {
        ESP_LOGI(TAG, "Pass: MAC NOT FOUND");
    } else {
        ESP_LOGW(TAG, "FAILED");
    }
        // Should not exist now
    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second

    // 7. Expiry Handling - Simulate expiry by manually invoking callback
    ESP_LOGI(TAG, "Simulating expiry...");
    mac_entry_t *expired_entry = &mac_table.entries[0];  // Use first entry for simulation
    mac_table_event_callback(0, expired_entry->mac, MAC_TABLE_TIMEOUT);
    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second

    // 8. Re-insert to ensure full cycle works
    ESP_LOGI(TAG, "Re-inserting MAC after expiry...");
    mac_table_insert(&mac_table, test_mac_3); // Should succeed as space is now freed
    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second

    while(1) {
        if (mac_table.stats->active_entries == 0) {
        uint8_t test_mac_2[MAC_ADDR_LEN] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5F};

        // Custom options
        mac_insert_options_t opts;
        opts.has_custom_duration = true;
        opts.custom_duration = 120;  // Set custom expiry to 120 seconds
        opts.has_role = true;
        opts.role = 1;  // Assign role 1
    
        // 1. Insert MAC address with custom options
        ESP_LOGI(TAG, "Inserting MAC address with custom options...");
        mac_entry_result_t result = mac_table_insert_ex(&mac_table, test_mac_1, &opts);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
