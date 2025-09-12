#ifdef __cplusplus
extern "C" {
#endif

#ifdef ESP_PLATFORM
#include <esp_crc.h>
#else
#include <stdlib.h>
#endif
#include "mac_table.h"
#include <esp_log.h>

static const char* TAG_TABLE = "MAC_TABLE";

// Unified internal function for deleting an entry
static void mac_table_delete_internal(mac_table_t *table, size_t index) {
    mac_entry_t *entry = &table->entries[index];
    if (entry->state == SLOT_OCCUPIED) {
        entry->state = SLOT_TOMBSTONE;

        // Update statistics and call expiry manager
        if (table->stats) {
            table->stats->total_deletes++;
            if (table->stats->active_entries > 0) {
                table->stats->active_entries--;
            }
        }
        if (table->expiry_manager) {
            expiry_manager_delete(table->expiry_manager, index);
        }

        // Call event handler
        if (table->on_event) {
            table->on_event(index, entry->mac, MAC_TABLE_DELETED);
        }
    }
}

// Hashing function, unchanged
static inline uint32_t mac_hash(const uint8_t *mac, size_t size) {
#ifdef ESP_PLATFORM
    uint32_t crc = esp_crc32_le(0, mac, MAC_ADDR_LEN);
    return crc % size;
#else
    uint32_t hash = 0x811C9DC5;
    for (size_t i = 0; i < MAC_ADDR_LEN; i++) {
        hash ^= mac[i];
        hash *= 0x01000193;
    }
    return hash % size;
#endif
}

/**
 * @brief Internal function to find a MAC address in the table.
 *
 * @param table Pointer to the MAC table.
 * @param mac MAC address to search for.
 * @param p_index Pointer to store the index if found.
 * @return Result of the search.
 */
static mac_entry_result_t mac_table_find_internal(const mac_table_t *table, const uint8_t *mac, size_t *p_index) {
    if (!table || !mac) return MAC_TABLE_NOT_FOUND;

    size_t start_index = mac_hash(mac, table->size);
    size_t index = start_index;

    // Linear probing with a maximum of 'size' iterations to prevent infinite loops
    for (size_t i = 0; i < table->size; i++) {
        index = (start_index + i) % table->size;
        mac_entry_t *entry = &table->entries[index];

        if (entry->state == SLOT_EMPTY) {
            if (p_index) *p_index = index;
            return MAC_TABLE_NOT_FOUND;
        }

        if (entry->state == SLOT_OCCUPIED && memcmp(entry->mac, mac, MAC_ADDR_LEN) == 0) {
            if (p_index) *p_index = index;
            time_t now = time(NULL);
            if (entry->expiration_time < now) {
                return MAC_TABLE_TIMEOUT;
            }
            return MAC_TABLE_OK;
        }
    }

    // If loop completes without finding a match or empty slot
    if (p_index) {
        // Find the next available tombstone or an empty slot for insertion
        for (size_t i = 0; i < table->size; i++) {
            index = (start_index + i) % table->size;
            if (table->entries[index].state != SLOT_OCCUPIED) {
                *p_index = index;
                return MAC_TABLE_NOT_FOUND;
            }
        }
    }

    return MAC_TABLE_NOT_FOUND;
}

static mac_entry_result_t mac_table_insert_or_update(mac_table_t *table, const uint8_t *mac, const mac_insert_options_t *opts) {
    if (!table || !mac) {
        return MAC_TABLE_NOT_FOUND;
    }

    size_t index = 0;
    mac_entry_result_t find_result = mac_table_find_internal(table, mac, &index);

    if (find_result == MAC_TABLE_OK) {
        // Entry found and is still active. Update its expiration time.
        mac_entry_t *entry = &table->entries[index];
        time_t new_expiration_time;

        if (opts && opts->has_custom_duration) {
            new_expiration_time = time(NULL) + opts->custom_duration;
        } else {
            new_expiration_time = time(NULL) + table->expiry_seconds;
        }
        entry->expiration_time = new_expiration_time;

        if (opts && opts->has_role) {
            entry->role = opts->role;
        }

        if (table->expiry_manager) {
            expiry_manager_add_or_update(table->expiry_manager, index);
        }

        if (table->on_event) {
            table->on_event(index, mac, MAC_TABLE_UPDATED);
        }

        return MAC_TABLE_UPDATED;
    } else {
        // Entry not found or expired. Find an empty slot and insert.
        size_t empty_slot_index = 0;
        bool found_empty_slot = false;
        
        // Linear probe to find an empty slot or a tombstone.
        size_t start_index = mac_hash(mac, table->size);
        for (size_t i = 0; i < table->size; i++) {
            size_t current_index = (start_index + i) % table->size;
            if (table->entries[current_index].state != SLOT_OCCUPIED) {
                empty_slot_index = current_index;
                found_empty_slot = true;
                break;
            }
        }

        if (!found_empty_slot) {
            ESP_LOGE(TAG_TABLE, "MAC table is full!");
            return MAC_TABLE_FULL;
        }

        // Insert new entry
        mac_entry_t *new_entry = &table->entries[empty_slot_index];
        memcpy(new_entry->mac, mac, MAC_ADDR_LEN);
        new_entry->state = SLOT_OCCUPIED;
        
        if (opts && opts->has_custom_duration) {
            new_entry->expiration_time = time(NULL) + opts->custom_duration;
        } else {
            new_entry->expiration_time = time(NULL) + table->expiry_seconds;
        }
        
        if (opts && opts->has_role) {
            new_entry->role = opts->role;
        } else {
            new_entry->role = 0; // Default role
        }

        if (table->stats) {
            table->stats->total_inserts++;
            table->stats->active_entries++;
        }

        if (table->expiry_manager) {
            expiry_manager_add_or_update(table->expiry_manager, empty_slot_index);
        }

        if (table->on_event) {
            table->on_event(empty_slot_index, mac, MAC_TABLE_INSERTED);
        }
        return MAC_TABLE_INSERTED;
    }
}

bool mac_table_init(mac_table_t *table, mac_entry_t *entries, size_t size,
                    size_t expiry_seconds, mac_table_event_callback_t on_event) {
    if (!table || !entries || size == 0) {
        return false;
    }

    table->entries = entries;
    table->size = size;
    table->expiry_seconds = expiry_seconds;
    table->on_event = on_event;

    // Reset all entries to an empty state
    for (size_t i = 0; i < size; i++) {
        table->entries[i].state = SLOT_EMPTY;
    }

    // Allocate and initialize statistics structure
    table->stats = (mac_table_stats_t *)malloc(sizeof(mac_table_stats_t));
    if (!table->stats) {
        return false;
    }
    memset(table->stats, 0, sizeof(mac_table_stats_t));

    // Create and initialize expiry manager
    table->expiry_manager = expiry_manager_create(table);
    if (!table->expiry_manager) {
        free(table->stats);
        return false;
    }

    return true;
}

mac_entry_result_t mac_table_insert(mac_table_t *table, const uint8_t *mac) {
    return mac_table_insert_or_update(table, mac, NULL);
}

mac_entry_result_t mac_table_insert_ex(mac_table_t *table, const uint8_t *mac,
                                       const mac_insert_options_t *opts) {
    return mac_table_insert_or_update(table, mac, opts);
}

mac_entry_result_t mac_table_exists(const mac_table_t *table,
                                    const uint8_t *mac) {
    return mac_table_find_internal(table, mac, NULL);
}

mac_entry_result_t mac_table_find(const mac_table_t *table, const uint8_t *mac, mac_entry_t *info) {
    if (!table || !mac || !info) {
        return MAC_TABLE_NOT_FOUND;
    }

    size_t index = 0;
    mac_entry_result_t result = mac_table_find_internal(table, mac, &index);

    if (result == MAC_TABLE_OK) {
        // Copy the data to the info struct
        memcpy(info, &table->entries[index], sizeof(mac_entry_t));
    }
    return result;
}

mac_entry_result_t mac_table_delete(mac_table_t *table, const uint8_t *mac) {
    size_t index = 0;
    mac_entry_result_t find_result = mac_table_find_internal(table, mac, &index);

    if (find_result == MAC_TABLE_OK || find_result == MAC_TABLE_TIMEOUT) {
        mac_table_delete_internal(table, index);
        return MAC_TABLE_DELETED;
    }
    return MAC_TABLE_NOT_FOUND;
}

mac_entry_result_t mac_table_get_by_index(const mac_table_t *table,
                                          size_t index, mac_entry_t *info) {
    if (!table || !info || index >= table->size) {
        return MAC_TABLE_NOT_FOUND;
    }

    if (table->entries[index].state == SLOT_OCCUPIED) {
        time_t now = time(NULL);
        if (table->entries[index].expiration_time < now) {
            return MAC_TABLE_TIMEOUT;
        }
        memcpy(info, &table->entries[index], sizeof(mac_entry_t));
        return MAC_TABLE_OK;
    }

    return MAC_TABLE_NOT_FOUND;
}

bool mac_table_remove_oldest(mac_table_t *table,
                             const uint8_t *protected_roles) {
    if (!table || !table->expiry_manager) {
        return false;
    }

    // This function is handled by the expiry manager's dedicated logic.
    return expiry_manager_remove_oldest(table->expiry_manager, protected_roles);
}

int mac_table_evict_by_role(mac_table_t *table, uint8_t role) {
    if (!table) return 0;
    int evicted_count = 0;
    for (size_t i = 0; i < table->size; i++) {
        if (table->entries[i].state == SLOT_OCCUPIED && table->entries[i].role == role) {
            mac_table_delete_internal(table, i);
            evicted_count++;
        }
    }
    return evicted_count;
}

int mac_table_clear(mac_table_t *table) {
    if (!table) return 0;
    int cleared_count = 0;
    for (size_t i = 0; i < table->size; i++) {
        if (table->entries[i].state == SLOT_OCCUPIED) {
            mac_table_delete_internal(table, i);
            cleared_count++;
        }
    }
    return cleared_count;
}

bool mac_table_reset_stats(mac_table_t *table) {
    if (!table || !table->stats) {
        return false;
    }
    memset(table->stats, 0, sizeof(mac_table_stats_t));
    return true;
}

bool mac_table_get_stats(const mac_table_t *table, mac_table_stats_t *stats) {
    if (!table || !stats || !table->stats) {
        return false;
    }
    memcpy(stats, table->stats, sizeof(mac_table_stats_t));
    return true;
}

void mac_to_str(const uint8_t *mac, char *str) {
    if (!mac || !str) return;
    sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

bool str_to_mac(const char *str, uint8_t *mac) {
    if (!str || !mac) return false;
    int num_scanned = sscanf(str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                             &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
    return num_scanned == MAC_ADDR_LEN;
}

#ifdef __cplusplus
}
#endif
