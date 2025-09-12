#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mac_table.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#define MAC_TABLE_SIZE 10
#define MAC_ENTRY_LIFETIME_SECONDS 15

// Define custom roles for the example
#define ROLE_CLIENT 1
#define ROLE_GATEWAY 2
#define ROLE_STAFF 3

// Local storage for the MAC table entries
mac_entry_t my_mac_entries[MAC_TABLE_SIZE];
mac_table_t my_mac_table;
mac_table_stats_t my_stats;

static const char *TAG = "MAC_TABLE_EXAMPLE";

/**
 * @brief Global event handler for MAC table events.
 *
 * This function is called by the MAC table implementation whenever an
 * entry is inserted, updated, deleted, or times out.
 */
static void mac_table_event_handler(int slot_index, const uint8_t *mac,
                                    const mac_entry_result_t status) {
  char mac_str[18];
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
    ESP_LOGI(TAG, "EVENT: MAC address %s deleted from slot %d", mac_str,
             slot_index);
    break;
  case MAC_TABLE_TIMEOUT:
    ESP_LOGW(TAG, "EVENT: MAC address %s expired at slot %d", mac_str,
             slot_index);
    break;
  default:
    break;
  }
}

/**
 * @brief Prints the current state of the MAC table.
 */
static void print_mac_table_state() {
  ESP_LOGI(TAG, "Current MAC Table State:");
  ESP_LOGI(TAG, "--------------------------------------------------");
  for (int i = 0; i < MAC_TABLE_SIZE; i++) {
    mac_entry_t entry;
    if (mac_table_get_by_index(&my_mac_table, i, &entry) == MAC_TABLE_OK) {
      char mac_str[18];
      mac_to_str(entry.mac, mac_str);
      time_t now = time(NULL);
      ESP_LOGI(TAG, "Slot %d: MAC: %s | Role: %d | TTL: %ld s", i, mac_str,
               entry.role, entry.expiration_time - now);
    } else {
      ESP_LOGI(TAG, "Slot %d: EMPTY or EXPIRED", i);
    }
  }
  ESP_LOGI(TAG, "--------------------------------------------------");
}

/**
 * @brief Prints the MAC table statistics.
 */
static void print_mac_table_stats() {
  mac_table_get_stats(&my_mac_table, &my_stats);
  ESP_LOGI(TAG, "MAC Table Statistics:");
  ESP_LOGI(TAG, "  Total Inserts: %zu", my_stats.total_inserts);
  ESP_LOGI(TAG, "  Total Deletes: %zu", my_stats.total_deletes);
  ESP_LOGI(TAG, "  Total Expired: %zu", my_stats.total_expired);
  ESP_LOGI(TAG, "  Active Entries: %zu", my_stats.active_entries);
}

void app_main(void) {
  // Synchronize time for expiration to work correctly
  setenv("TZ", "UTC", 1);
  tzset();

  ESP_LOGI(TAG, "Initializing MAC address table...");
  bool init_ok =
      mac_table_init(&my_mac_table, my_mac_entries, MAC_TABLE_SIZE,
                     MAC_ENTRY_LIFETIME_SECONDS, mac_table_event_handler);

  if (!init_ok) {
    ESP_LOGE(TAG, "Failed to initialize MAC table!");
    return;
  }
  ESP_LOGI(TAG,
           "MAC table initialized successfully with size %d and expiry of %d "
           "seconds.",
           MAC_TABLE_SIZE, MAC_ENTRY_LIFETIME_SECONDS);

  // --- Example Scenario 1: Basic insertions ---
  ESP_LOGI(TAG, "--- Scenario 1: Basic insertions ---");
  uint8_t mac1[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
  uint8_t mac2[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
  uint8_t mac3[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB};
  uint8_t mac4[] = {0x99, 0x88, 0x77, 0x66, 0x55, 0x44};

  mac_table_insert(&my_mac_table, mac1);
  mac_table_insert(&my_mac_table, mac2);
  vTaskDelay(pdMS_TO_TICKS(1000)); // Delay to show TTL difference
  mac_table_insert(&my_mac_table, mac3);
  print_mac_table_state();
  print_mac_table_stats();

  // --- Scenario 2: Insertion with custom options ---
  ESP_LOGI(TAG, "--- Scenario 2: Insertion with custom options ---");
  uint8_t mac5[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
  mac_insert_options_t opts = {.has_custom_duration = true,
                               .custom_duration = 60, // A long-lived entry
                               .has_role = true,
                               .role = ROLE_GATEWAY};
  mac_table_insert_ex(&my_mac_table, mac5, &opts);
  print_mac_table_state();
  print_mac_table_stats();

  // --- Scenario 3: Update an existing entry ---
  ESP_LOGI(TAG, "--- Scenario 3: Update an existing entry ---");
  mac_table_insert(&my_mac_table, mac1); // Updates expiration time and role
  print_mac_table_state();
  print_mac_table_stats();

  // --- Scenario 4: Deletion and existence check ---
  ESP_LOGI(TAG, "--- Scenario 4: Deletion and existence check ---");
  mac_table_delete(&my_mac_table, mac3);
  if (mac_table_exists(&my_mac_table, mac3) == MAC_TABLE_NOT_FOUND) {
    ESP_LOGI(TAG,
             "MAC address %02x:%02x:%02x:%02x:%02x:%02x successfully deleted.",
             mac3[0], mac3[1], mac3[2], mac3[3], mac3[4], mac3[5]);
  }
  print_mac_table_state();
  print_mac_table_stats();

  // --- Scenario 5: Eviction by role ---
  ESP_LOGI(TAG, "--- Scenario 5: Eviction by role ---");
  uint8_t mac6[] = {0x44, 0x55, 0x66, 0x77, 0x88, 0x99};
  uint8_t mac7[] = {0xAA, 0xBB, 0xCC, 0xEE, 0xDD, 0xFF};
  mac_insert_options_t client_opts = {.has_role = true, .role = ROLE_CLIENT};
  mac_table_insert_ex(&my_mac_table, mac6, &client_opts);
  mac_table_insert_ex(&my_mac_table, mac7, &client_opts);

  int evicted_count = mac_table_evict_by_role(&my_mac_table, ROLE_CLIENT);
  ESP_LOGI(TAG, "Evicted %d entries with role CLIENT.", evicted_count);
  print_mac_table_state();
  print_mac_table_stats();

  // --- Scenario 6: Wait for entries to expire ---
  ESP_LOGI(TAG, "--- Scenario 6: Waiting for entries to expire ---");
  ESP_LOGI(TAG, "Waiting %d seconds for entries to expire...",
           MAC_ENTRY_LIFETIME_SECONDS);
  vTaskDelay(
      pdMS_TO_TICKS(MAC_ENTRY_LIFETIME_SECONDS * 1000 + 2000)); // Add a buffer
  print_mac_table_state();
  print_mac_table_stats();
}
