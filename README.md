# MAC Address Table with Expiry Management

## Overview

`mac_table.h` provides a **MAC address table implementation** with **expiry management** suitable for embedded systems, such as **ESP32** using **FreeRTOS**. It is designed for managing MAC addresses dynamically with time-based expiration, event handling, and state management for each entry.

This library is useful in applications like **peer-to-peer networking**, **device discovery**, and **access control**, where devices must be tracked and cleaned up automatically after a certain time period.

---

## Features

- **Dynamic MAC Address Table**: Handles a list of MAC addresses with time-based expiry.
- **Expiry Management**: Automatically expires entries after a defined time interval.
- **Event Callbacks**: Triggers user-defined callbacks when events occur (e.g., insertion, update, deletion, expiry).
- **Slot Management**: Each MAC address is managed by its own slot with state tracking (e.g., occupied, empty, or tombstone).
- **Lightweight and Efficient**: Optimized for embedded systems with constrained resources.

---

## Usage

### 1. Initialization

To initialize the MAC table, call `mac_table_init()` with the desired parameters.

```c
mac_entry_t mac_table_entries[MAC_TABLE_SIZE];
mac_table_t mac_table;

mac_table_init(&mac_table, mac_table_entries, MAC_TABLE_SIZE, 300, mac_table_event_callback);
```
### 2. Inserting/Updating MAC Addresses
To insert or update a MAC address, use the `mac_table_insert()` function:
```c
uint8_t new_mac[MAC_ADDR_LEN] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
mac_table_result_t result = mac_table_insert(&mac_table, new_mac);

if (result == MAC_TABLE_INSERTED) {
    ESP_LOGI("MAC_TABLE", "MAC address successfully inserted.");
} else if (result == MAC_TABLE_UPDATED) {
    ESP_LOGI("MAC_TABLE", "MAC address already exists and was updated.");
}
```
### 3. Deleting a MAC Address
To delete a MAC address, use the `mac_table_delete()` function:
```c
uint8_t mac_to_delete[MAC_ADDR_LEN] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
mac_table_result_t delete_result = mac_table_delete(&mac_table, mac_to_delete);

if (delete_result == MAC_TABLE_DELETED) {
    ESP_LOGI("MAC_TABLE", "MAC address successfully deleted.");
} else {
    ESP_LOGW("MAC_TABLE", "MAC address not found for deletion.");
}
```
### 4. Checking if a MAC Address Exists
To check if a MAC address exists in the table, use the `mac_table_exists()` function:
```c
uint8_t mac_to_check[MAC_ADDR_LEN] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
mac_table_result_t check_result = mac_table_exists(&mac_table, mac_to_check);

if (check_result == MAC_TABLE_OK) {
    ESP_LOGI("MAC_TABLE", "MAC address found in the table.");
} else {
    ESP_LOGW("MAC_TABLE", "MAC address not found in the table.");
}
```
### 5. Retrieving a MAC Address Entry
To retrieve a specific MAC entry from the table by its index, use the `mac_table_get_entry()` function:
```c
mac_entry_t retrieved_entry;
mac_table_result_t get_result = mac_table_get_entry(&mac_table, 0, &retrieved_entry);

if (get_result == MAC_TABLE_OK) {
    char mac_str[18];
    mac_to_str(retrieved_entry.mac, mac_str);
    ESP_LOGI("MAC_TABLE", "MAC address at index 0: %s", mac_str);
} else {
    ESP_LOGW("MAC_TABLE", "Failed to retrieve MAC address.");
}
```
## Advanced Features
### Expiry Management
The library supports automatic expiry management using a timer-based system, integrated with the FreeRTOS scheduler. Expired entries are automatically removed or marked as expired, and user-defined callbacks are triggered.

You can adjust the expiry duration for entries in the `mac_table_init()` call.
```c
mac_table_init(&mac_table, mac_table_entries, MAC_TABLE_SIZE, 600, mac_table_event_callback);
```
In this example, MAC addresses will expire after 600 seconds (10 minutes).

### Event Callbacks
User-defined callbacks are invoked when certain events occur in the MAC address table, such as insertion, deletion, or expiry of entries. You can define your custom behavior in the callback function by processing the event details passed as arguments

```c
void mac_table_event_callback(size_t slot_index, mac_entry_result_t status) {
    switch (status) {
        case MAC_TABLE_INSERTED:
            ESP_LOGI("MAC_TABLE", "MAC address inserted at slot %d", slot_index);
            break;
        case MAC_TABLE_UPDATED:
            ESP_LOGI("MAC_TABLE", "MAC address updated at slot %d", slot_index);
            break;
        case MAC_TABLE_DELETED:
            ESP_LOGI("MAC_TABLE", "MAC address deleted at slot %d", slot_index);
            break;
        case MAC_TABLE_TIMEOUT:
            ESP_LOGW("MAC_TABLE", "MAC address at slot %d expired", slot_index);
            break;
        default:
            ESP_LOGI("MAC_TABLE", "Unknown event at slot %d", slot_index);
    }
}
```
## Configuration
### MAC Table Size
The size of the MAC address table is defined by `MAC_TABLE_SIZE`. Set this value based on your application's needs.
```c
#define MAC_TABLE_SIZE 10
```
### Expiry Timeout
The expiration timeout for MAC address entries can be set during initialization. The time is given in seconds.
```c
mac_table_init(&mac_table, mac_table_entries, MAC_TABLE_SIZE, 300, mac_table_event_callback);
```
In this example, entries will expire after 300 seconds (5 minutes).

License
This project is licensed under the MIT License. See the LICENSE file for more details.

Contributing
Feel free to fork this repository, submit issues, or pull requests for any enhancements or bug fixes. All contributions are welcome!
## Acknowledgments
- Developed with the help of AI tools (OpenAI ChatGPT, Grok AI and, Claude).
- Adapted and maintained by Stanley Osagie Ramos.