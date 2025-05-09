# MAC Address Table with Expiry Management

## Overview

`mac_table.h` provides a **MAC address table implementation** with **expiry management** suitable for embedded systems, such as **ESP32** using **FreeRTOS**. It enables dynamic management of MAC addresses, with time-based expiration, event handling, and state management for each entry.

The library is ideal for applications like **peer-to-peer networking**, **device discovery**, and **access control**, where devices need to be tracked and cleaned up automatically after a defined period.

---

## Features

- **Dynamic MAC Address Table**: Handles a list of MAC addresses with time-based expiry.
- **Expiry Management**: Automatically expires entries after a defined time interval.
- **Event Callbacks**: Triggers user-defined callbacks when events occur (e.g., insertion, update, deletion, expiry).
- **Slot Management**: Each MAC address is managed by its own slot with state tracking (e.g., occupied, empty, or tombstone).
- **Efficient and Lightweight**: Optimized for embedded systems with constrained resources.

---

## Usage

### 1. Initialization

Initialize the MAC address table with `mac_table_init()`.

```c
mac_entry_t mac_table_entries[MAC_TABLE_SIZE];
mac_table_t mac_table;

mac_table_init(&mac_table, mac_table_entries, MAC_TABLE_SIZE, 300, mac_table_event_callback);
```
### 2. Inserting/Updating MAC Addresses
Use mac_table_insert() to insert or update a MAC address.
```c
uint8_t new_mac[MAC_ADDR_LEN] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
mac_table_result_t result = mac_table_insert(&mac_table, new_mac);
```
### 3. Deleting a MAC Address
To delete a MAC address, use `mac_table_delete()`.
```c
mac_table_result_t delete_result = mac_table_delete(&mac_table, mac_to_delete);
```
### 4. Checking if a MAC Address Exists
Check for the existence of a MAC address using `mac_table_exists()`.
```c
mac_table_result_t check_result = mac_table_exists(&mac_table, mac_to_check);
```
## Advanced Features
### Expiry Management
The library supports automatic expiry of MAC address entries, with the expiration duration set during initialization.
```c
mac_table_init(&mac_table, mac_table_entries, MAC_TABLE_SIZE, 600, mac_table_event_callback);
```
Entries will expire after the specified time (e.g., 600 seconds).
### Event Callbacks
User-defined callbacks are triggered on events such as insertion, update, or expiry. Define the callback to handle the events.
```c
void mac_table_event_callback(int slot_index, const uint8_t *mac, mac_entry_result_t status) {
    // Handle events such as insertion, update, deletion, expiry
}
```
## Example Use Case: ESP-NOW Integration
### The table can be integrated with ESP-NOW to dynamically manage peers for a mesh-style network.
```c
#include "mac_table.h"
#include "esp_wifi.h"
#include "esp_now.h"

// Initialize ESP-NOW
void espnow_init() {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_now_init());
}

// Add or remove peers based on MAC table events
void mac_table_event_callback(int slot_index, const uint8_t *mac, mac_entry_result_t status) {
    if (status == MAC_TABLE_INSERTED) {
        add_peer(mac);
    } else if (status == MAC_TABLE_DELETED || status == MAC_TABLE_TIMEOUT) {
        remove_peer(mac);
    }
}
```
## Configuration
### MAC Table Size
Define the size of the MAC address table with `MAC_TABLE_SIZE`.
```c
#define MAC_TABLE_SIZE 10
```
### Expiry Timeout
Set the expiration duration for MAC addresses in the mac_table_init() call.
```c
mac_table_init(&mac_table, mac_table_entries, MAC_TABLE_SIZE, 300, mac_table_event_callback);
```
## License
This project is licensed under the MIT License. See the [LICENSE](https://github.com/sagieramos/mac_table/blob/main/README.md) file for more details.
## Acknowledgments
Developed with the help of AI tools (OpenAI ChatGPT, Grok AI, Claude).

Adapted and maintained by Stanley Osagie Ramos.