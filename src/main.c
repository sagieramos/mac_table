#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mac_table.h"
#include <stdio.h>

#define MAC_TABLE_SIZE 10
#define MAC_EXPIRY_SECONDS 60

static const char *TAG = "MAC_TABLE_EXAMPLE";

/* Global MAC table instance */
mac_table_t g_mac_table;
mac_entry_t g_mac_entries[MAC_TABLE_SIZE];

/**
 * @brief Simple event handler to log MAC table events and get a stats snapshot.
 *
 * @param slot_index The index of the slot where the event occurred.
 * @param mac The MAC address involved.
 * @param status The type of event.
 */
void mac_event_handler(int slot_index, const uint8_t *mac,
                       const mac_entry_result_t status) {
  char mac_str[20];
  mac_to_str(mac, mac_str);

  switch (status) {
  case MAC_TABLE_INSERTED:
    ESP_LOGI(TAG, "EVENT: MAC address %s inserted at slot %d", mac_str,
             slot_index);
    break;
  case MAC_TABLE_UPDATED:
    ESP_LOGI(TAG, "EVENT: MAC address %s updated at slot %d", mac_str,
             slot_index);
    break;
  case MAC_TABLE_DELETED:
    ESP_LOGW(TAG, "EVENT: MAC address %s deleted at slot %d", mac_str,
             slot_index);
    break;
  case MAC_TABLE_TIMEOUT:
    ESP_LOGW(TAG, "EVENT: MAC address %s expired at slot %d", mac_str,
             slot_index);
    break;
  default:
    break;
  }

  // Get a fresh snapshot of the stats after every event
  mac_table_stats_t current_stats;
  if (mac_table_get_stats(&g_mac_table, &current_stats)) {
    ESP_LOGI(TAG, "Current Active Entries: %zu", current_stats.active_entries);
  }
}

/**
 * @brief Prints the current state of the MAC table, including all entries.
 */
static void print_mac_table(void) {
  ESP_LOGI(TAG, "Current MAC Table State:");
  ESP_LOGI(TAG, "--------------------------------------------------");
  for (size_t i = 0; i < MAC_TABLE_SIZE; i++) {
    mac_entry_t entry;
    mac_entry_result_t result = mac_table_get_by_index(&g_mac_table, i, &entry);
    if (result == MAC_TABLE_OK) {
      char mac_str[20];
      mac_to_str(entry.mac, mac_str);
      time_t now = time(NULL);
      ESP_LOGI(TAG, "Slot %zu: MAC: %s | Role: %u | TTL: %lld s", i, mac_str,
               entry.role, (long long)entry.expiration_time - now);
    } else {
      ESP_LOGI(TAG, "Slot %zu: EMPTY or EXPIRED", i);
    }
  }
  ESP_LOGI(TAG, "--------------------------------------------------");
}

/**
 * @brief Prints the current MAC table statistics.
 */
static void print_mac_stats(void) {
  mac_table_stats_t stats;
  if (mac_table_get_stats(&g_mac_table, &stats)) {
    ESP_LOGI(TAG, "MAC Table Statistics:");
    ESP_LOGI(TAG, "   Total Inserts: %zu", stats.total_inserts);
    ESP_LOGI(TAG, "   Total Deletes: %zu", stats.total_deletes);
    ESP_LOGI(TAG, "   Total Expired: %zu", stats.total_expired);
    ESP_LOGI(TAG, "   Active Entries: %zu", stats.active_entries);
  }
}

/**
 * @brief Demonstrates the use of the new mac_table_find function.
 * @param mac The MAC address to look up.
 */
static void find_and_print_entry(const uint8_t *mac) {
  char mac_str[20];
  mac_to_str(mac, mac_str);
  mac_entry_t found_entry;

  ESP_LOGI(TAG, "Looking up MAC address: %s", mac_str);

  mac_entry_result_t result = mac_table_find(&g_mac_table, mac, &found_entry);

  if (result == MAC_TABLE_OK) {
    time_t now = time(NULL);
    ESP_LOGI(TAG, "Found entry for %s: Role=%u, TTL=%llds", mac_str,
             found_entry.role, (long long)found_entry.expiration_time - now);
  } else {
    ESP_LOGW(TAG, "Entry for %s not found or expired. Status: %d", mac_str,
             result);
  }
}

void app_main(void) {
  // Initialize the MAC table
  if (!mac_table_init(&g_mac_table, g_mac_entries, MAC_TABLE_SIZE,
                      MAC_EXPIRY_SECONDS, mac_event_handler)) {
    ESP_LOGE(TAG, "MAC table initialization failed!");
    return;
  }
  ESP_LOGI(TAG, "MAC table initialized with size %d and expiry %d seconds.",
           MAC_TABLE_SIZE, MAC_EXPIRY_SECONDS);

  // Insert some entries
  uint8_t mac1[] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
  uint8_t mac2[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
  uint8_t mac3[] = {0xde, 0xad, 0xbe, 0xef, 0xca, 0xfe};

  mac_table_insert(&g_mac_table, mac1);
  mac_table_insert(&g_mac_table, mac2);

  // Insert a third MAC with a long expiration
  mac_insert_options_t opts = {.has_custom_duration = true,
                               .custom_duration = 100,
                               .has_role = true,
                               .role = 2};
  mac_table_insert_ex(&g_mac_table, mac3, &opts);

  // Check state and stats
  print_mac_table();
  print_mac_stats();

  // Demonstrate the  mac_table_find function
  find_and_print_entry(mac1);
  find_and_print_entry(mac3);

  // Wait for entries to expire
  vTaskDelay(pdMS_TO_TICKS(15000));
  print_mac_table();
  print_mac_stats();

  // Wait for the final entry to expire
  vTaskDelay(pdMS_TO_TICKS(60000));
  print_mac_table();
  print_mac_stats();

  ESP_LOGI(TAG, "Returned from app_main()");
}
