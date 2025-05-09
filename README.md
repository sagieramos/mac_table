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

