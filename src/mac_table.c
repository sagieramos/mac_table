#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <esp_crc.h>
#include "mac_table.h"
#include "esp_log.h"

static inline uint32_t mac_hash(const uint8_t *mac, size_t size)
{
    uint32_t crc = esp_crc32_le(0, mac, MAC_ADDR_LEN);
    return crc % size;
}

void mac_table_init(mac_table_t *table, mac_entry_t *entries, size_t size,
                    uint32_t expiry_seconds, mac_table_event_callback_t on_event)
{
    if (!table || !entries || size == 0) {
        return;
    }

    table->entries = entries;
    table->size = size;
    table->expiry_seconds = expiry_seconds;
    table->on_event = on_event;
    table->expiry_manager = NULL;

    for (size_t i = 0; i < size; i++) {
        table->entries[i].state = SLOT_EMPTY;
        table->entries[i].timeout_duration = 0;
        memset(table->entries[i].mac, 0, MAC_ADDR_LEN);
    }

    table->expiry_manager = expiry_manager_create(table);
    if (!table->expiry_manager) {
        ESP_LOGW("xpiry_manager", "Failed to create expiry manager. MAC table timeouts will not be handled.");
        esp_restart();
    }
}

mac_entry_result_t mac_table_insert(mac_table_t *table, const uint8_t *mac)
{
    if (!table || !mac) {
        return MAC_TABLE_NOT_FOUND;
    }

    uint32_t index = mac_hash(mac, table->size);
    int first_tombstone = -1;
    int first_expired = -1;
    time_t current_time = time(NULL);

    for (size_t i = 0; i < table->size; i++) {
        size_t probe = (index + i) % table->size;
        mac_entry_t *entry = &table->entries[probe];

        if (entry->state == SLOT_OCCUPIED) {
            if (memcmp(entry->mac, mac, MAC_ADDR_LEN) == 0) {
                entry->timeout_duration = current_time + table->expiry_seconds;
                if (table->expiry_manager) {
                    expiry_manager_add_or_update(table->expiry_manager, probe);
                }
                if (table->on_event) {
                    table->on_event(probe, MAC_TABLE_UPDATED);
                }
                return MAC_TABLE_UPDATED;
            }
            
            if (current_time >= entry->timeout_duration && first_expired == -1) {
                first_expired = probe;
            }
        } else if (entry->state == SLOT_TOMBSTONE && first_tombstone == -1) {
            first_tombstone = probe;
        } else if (entry->state == SLOT_EMPTY) {
            memcpy(entry->mac, mac, MAC_ADDR_LEN);
            entry->state = SLOT_OCCUPIED;
            entry->timeout_duration = current_time + table->expiry_seconds;
            if (table->expiry_manager) {
                expiry_manager_add_or_update(table->expiry_manager, probe);
            }
            if (table->on_event) {
                table->on_event(probe, MAC_TABLE_INSERTED);
            }
            return MAC_TABLE_INSERTED;
        }
    }

     if (first_expired != -1) {
        mac_entry_t *entry = &table->entries[first_expired];
        if (table->expiry_manager) {
            expiry_manager_delete(table->expiry_manager, first_expired);
        }
        memcpy(entry->mac, mac, MAC_ADDR_LEN);
        entry->timeout_duration = current_time + table->expiry_seconds;
        if (table->expiry_manager) {
            expiry_manager_add_or_update(table->expiry_manager, first_expired);
        }
        if (table->on_event) {
            table->on_event(first_expired, MAC_TABLE_INSERTED);
        }
        return MAC_TABLE_INSERTED;
    } 
    

    if (first_tombstone != -1) {
        mac_entry_t *entry = &table->entries[first_tombstone];
        memcpy(entry->mac, mac, MAC_ADDR_LEN);
        entry->timeout_duration = current_time + table->expiry_seconds;
        entry->state = SLOT_OCCUPIED;
        if (table->expiry_manager) {
            expiry_manager_add_or_update(table->expiry_manager, first_tombstone);
        }
        if (table->on_event) {
            table->on_event(first_tombstone, MAC_TABLE_INSERTED);
        }
        return MAC_TABLE_INSERTED;
    }

    if (table->on_event) {
        table->on_event(-1, MAC_TABLE_FULL);
    }

    return MAC_TABLE_FULL;
}

mac_entry_result_t mac_table_exists(const mac_table_t *table, const uint8_t *mac)
{
    if (!table || !mac) {
        return MAC_TABLE_NOT_FOUND;
    }

    uint32_t index = mac_hash(mac, table->size);

    for (size_t i = 0; i < table->size; i++) {
        size_t probe = (index + i) % table->size;
        const mac_entry_t *entry = &table->entries[probe];

        if (entry->state == SLOT_EMPTY) {
            return MAC_TABLE_NOT_FOUND;
        }

        if (entry->state == SLOT_OCCUPIED && memcmp(entry->mac, mac, MAC_ADDR_LEN) == 0) {
            return MAC_TABLE_OK;
        }
    }

    return MAC_TABLE_NOT_FOUND;
}

mac_entry_result_t mac_table_delete(mac_table_t *table, const uint8_t *mac)
{
    if (!table || !mac) {
        return MAC_TABLE_NOT_FOUND;
    }

    uint32_t index = mac_hash(mac, table->size);

    for (size_t i = 0; i < table->size; i++) {
        size_t probe = (index + i) % table->size;
        mac_entry_t *entry = &table->entries[probe];

        if (entry->state == SLOT_EMPTY) {
            return MAC_TABLE_NOT_FOUND;
        }

        if (entry->state == SLOT_OCCUPIED && memcmp(entry->mac, mac, MAC_ADDR_LEN) == 0) {
            entry->state = SLOT_TOMBSTONE;
            if (table->expiry_manager) {
                expiry_manager_delete(table->expiry_manager, probe);
            }
            if (table->on_event) {
                table->on_event(probe, MAC_TABLE_DELETED);
            }
            return MAC_TABLE_DELETED;
        }
    }
    
    return MAC_TABLE_NOT_FOUND;
}

mac_entry_result_t mac_table_get_entry(const mac_table_t *table, size_t index, mac_entry_t *info)
{
    if (!table || index >= table->size) {
        return MAC_TABLE_NOT_FOUND;
    }
    
    const mac_entry_t *entry = &table->entries[index];
    
    if (entry->state != SLOT_OCCUPIED) {
        return MAC_TABLE_NOT_FOUND;
    }
    
    time_t current_time = time(NULL);
    if (current_time >= entry->timeout_duration) {
        if (info != NULL) {
            memcpy(info->mac, entry->mac, MAC_ADDR_LEN);
            info->timeout_duration = entry->timeout_duration;
            info->state = entry->state;
        }
        return MAC_TABLE_TIMEOUT;
    }
    
    if (info != NULL) {
        memcpy(info->mac, entry->mac, MAC_ADDR_LEN);
        info->timeout_duration = entry->timeout_duration;
        info->state = entry->state;
    }
    
    return MAC_TABLE_OK;
}

void mac_to_str(const uint8_t *mac, char *str)
{
    if (!mac || !str) {
        return;
    }
    
    static const char hex_chars[] = "0123456789abcdef";
    
    int j = 0;
    for (int i = 0; i < MAC_ADDR_LEN; i++) {
        str[j++] = hex_chars[(mac[i] >> 4) & 0xF];
        str[j++] = hex_chars[mac[i] & 0xF];
        
        if (i < MAC_ADDR_LEN - 1) {
            str[j++] = ':';
        }
    }
    str[j] = '\0';
}

bool str_to_mac(const char *str, uint8_t *mac)
{
    if (!str || !mac) {
        return false;
    }
    
    uint8_t byte;
    int matched = 0;
    const char *ptr = str;
    
    for (int i = 0; i < MAC_ADDR_LEN; i++) {
        byte = 0;
        
        if (*ptr >= '0' && *ptr <= '9')
            byte = (*ptr - '0') << 4;
        else if (*ptr >= 'a' && *ptr <= 'f')
            byte = (*ptr - 'a' + 10) << 4;
        else if (*ptr >= 'A' && *ptr <= 'F')
            byte = (*ptr - 'A' + 10) << 4;
        else
            return false;
        ptr++;
        
        if (*ptr >= '0' && *ptr <= '9')
            byte |= (*ptr - '0');
        else if (*ptr >= 'a' && *ptr <= 'f')
            byte |= (*ptr - 'a' + 10);
        else if (*ptr >= 'A' && *ptr <= 'F')
            byte |= (*ptr - 'A' + 10);
        else
            return false;
        ptr++;
        
        mac[i] = byte;
        matched++;
        
        if (i < MAC_ADDR_LEN - 1) {
            if (*ptr != ':')
                return false;
            ptr++;
        }
    }
    
    return (matched == MAC_ADDR_LEN && *ptr == '\0');
}

#ifdef __cplusplus
}
#endif