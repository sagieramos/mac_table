#include <Arduino.h>
#include "mac_table.h"

// Callback for MAC table events
void mac_table_event_callback(size_t slot_index, mac_entry_result_t status) {
  Serial.print("Event at slot: ");
  Serial.println(slot_index);
  Serial.println(status);
}

void setup() {
  Serial.begin(115200);

  // Initialize the MAC table
  mac_entry_t mac_entries[10];
  mac_table_t table;
  mac_table_init(&table, mac_entries, 10, 60, mac_table_event_callback);

  // Example MAC address
  uint8_t mac[MAC_ADDR_LEN] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};

  mac_table_insert(&table, mac);  // Insert MAC address
}

void loop() {
  // Loop code
}