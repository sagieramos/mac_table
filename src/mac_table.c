#ifdef __cplusplus
extern "C" {
#endif

#ifdef ESP_PLATFORM
#include <esp_crc.h>
#endif
#include "mac_table.h"

static inline uint32_t mac_hash(const uint8_t *mac, size_t size)
{
#ifdef ESP_PLATFORM
    uint32_t crc = esp_crc32_le(0, mac, MAC_ADDR_LEN);
    return crc % size;
#else
    uint32_t hash = 0x811C9DC5;
    for (size_t i = 0; i < MAC_ADDR_LEN; ++i) {
        hash ^= mac[i];
        hash *= 0x01000193;
    }
    return hash % size;
#endif
}                   


#define DEFAULT_ROLE 0

bool mac_table_init(mac_table_t *table, mac_entry_t *entries, size_t size,
                    size_t expiry_seconds, mac_table_event_callback_t on_event)
{
    if (!table || !entries || size == 0) {
        return false;
    }

    table->entries = entries;
    table->size = size;
    table->expiry_seconds = expiry_seconds;
    table->on_event = on_event;
    table->expiry_manager = NULL;

      // Initialize stats
      table->stats = (mac_table_stats_t *)calloc(1, sizeof(mac_table_stats_t));  // Allocate memory for stats
      if (!table->stats) {
          return false;
      }

    for (size_t i = 0; i < size; i++) {
        table->entries[i].state = SLOT_EMPTY;
        table->entries[i].timeout_duration = 0;
        memset(table->entries[i].mac, 0, MAC_ADDR_LEN);
    }

    table->expiry_manager = expiry_manager_create(table);
    if (!table->expiry_manager) {
        return false;
    }

    return true;
}

mac_entry_result_t mac_table_insert(mac_table_t *table, const uint8_t *mac) {
    return mac_table_insert_ex(table, mac, NULL);
}
mac_entry_result_t mac_table_insert_ex(mac_table_t *table, const uint8_t *mac, const mac_insert_options_t *opts)
{
    if (!table || !mac) {
        return MAC_TABLE_NOT_FOUND;
    }

    uint32_t index = mac_hash(mac, table->size);
    int first_tombstone = -1;
    time_t current_time = time(NULL);

    time_t timeout = (opts && opts->has_custom_duration)
        ? current_time + opts->custom_duration
        : current_time + table->expiry_seconds;

    uint8_t role = (opts && opts->has_role) ? opts->role : DEFAULT_ROLE;

    for (size_t i = 0; i < table->size; i++) {
        size_t probe = (index + i) % table->size;
        mac_entry_t *entry = &table->entries[probe];

        if (entry->state == SLOT_OCCUPIED) {
            if (memcmp(entry->mac, mac, MAC_ADDR_LEN) == 0) {
                entry->timeout_duration = timeout;
                entry->role = role;
                if (table->expiry_manager) {
                    expiry_manager_add_or_update(table->expiry_manager, probe);
                }
                if (table->on_event) {
                    table->on_event(probe, mac, MAC_TABLE_UPDATED);
                }
                return MAC_TABLE_UPDATED;
            }
        } else if (entry->state == SLOT_TOMBSTONE && first_tombstone == -1) {
            first_tombstone = probe;
        } else if (entry->state == SLOT_EMPTY) {
            memcpy(entry->mac, mac, MAC_ADDR_LEN);
            entry->state = SLOT_OCCUPIED;
            entry->timeout_duration = timeout;
            entry->role = role;

            // Update statistics
            table->stats->total_inserts++;
            table->stats->active_entries++;

            if (table->expiry_manager) {
                expiry_manager_add_or_update(table->expiry_manager, probe);
            }
            if (table->on_event) {
                table->on_event(probe, mac, MAC_TABLE_INSERTED);
            }


            return MAC_TABLE_INSERTED;
        }
    }

    if (first_tombstone != -1) {
        mac_entry_t *entry = &table->entries[first_tombstone];
        memcpy(entry->mac, mac, MAC_ADDR_LEN);
        entry->timeout_duration = timeout;
        entry->role = role;
        entry->state = SLOT_OCCUPIED;

                // Update statistics
                table->stats->total_inserts++;
                table->stats->active_entries++;
        
        if (table->expiry_manager) {
            expiry_manager_add_or_update(table->expiry_manager, first_tombstone);
        }
        if (table->on_event) {
            table->on_event(first_tombstone, mac, MAC_TABLE_INSERTED);
        }

        return MAC_TABLE_INSERTED;
    }

    if (table->on_event) {
        table->on_event(-1, mac, MAC_TABLE_FULL);
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

            // Update statistics
            table->stats->total_deletes++;
            table->stats->active_entries--;

            if (table->expiry_manager) {
                expiry_manager_delete(table->expiry_manager, probe);
            }
            if (table->on_event) {
                table->on_event(probe, mac, MAC_TABLE_DELETED);
            }

            return MAC_TABLE_DELETED;
        }
    }

    return MAC_TABLE_NOT_FOUND;
}


mac_entry_result_t mac_table_get_by_index(const mac_table_t *table, size_t index, mac_entry_t *out_entry)
{
    if (!table || index >= table->size) {
        return MAC_TABLE_NOT_FOUND;
    }

    const mac_entry_t *entry = &table->entries[index];

    if (entry->state != SLOT_OCCUPIED) {
        return MAC_TABLE_NOT_FOUND;
    }

    if (out_entry != NULL) {
        memcpy(out_entry->mac, entry->mac, MAC_ADDR_LEN);
        out_entry->timeout_duration = entry->timeout_duration;
        out_entry->state = entry->state;
        out_entry->role = entry->role;
    }

    return MAC_TABLE_OK;
}

void mac_table_delete_by_index(mac_table_t *table, size_t index) {
    if (index >= table->size) return;

    mac_entry_t *entry = &table->entries[index];
    if (entry->state == SLOT_OCCUPIED) {
        entry->state = SLOT_TOMBSTONE;

        table->stats->total_deletes++;
        table->stats->active_entries--;

        if (table->on_event) {
            table->on_event(index, entry->mac, MAC_TABLE_DELETED);
        }

        expiry_manager_delete(table->expiry_manager, index);
    }
}



bool mac_table_get_stats(const mac_table_t *table, mac_table_stats_t *stats){
    if (table && stats) {
        memcpy(stats, table->stats, sizeof(mac_table_stats_t));
        return true;
    }

    return false;
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

int mac_table_evict_by_role(mac_table_t *table, uint8_t role) {
    int evicted_count = 0;
    for (size_t i = 0; i < table->size; i++) {
        mac_entry_t *entry = &table->entries[i];
        if (entry->state == SLOT_OCCUPIED && entry->role == role) {
            
            entry->state = SLOT_TOMBSTONE;
            evicted_count++;
            table->stats->total_deletes++;
            table->stats->active_entries--;

            if (table->on_event) {
                table->on_event(index, entry->mac, MAC_TABLE_DELETED);
            }
    
            expiry_manager_delete(table->expiry_manager, index);
        }
    }
    return evicted_count;
}

int mac_table_clear(mac_table_t *table) {
    int cleared_count = 0;
    for (size_t i = 0; i < table->size; i++) {
        mac_entry_t *entry = &table->entries[i];
        if (entry->state == SLOT_OCCUPIED) {
            entry->state = SLOT_TOMBSTONE;
            cleared_count++;
            table->stats->total_deletes++;
            table->stats->active_entries--;

            if (table->on_event) {
                table->on_event(index, entry->mac, MAC_TABLE_DELETED);
            }
    
            expiry_manager_delete(table->expiry_manager, index);
        }
    }
    
    return cleared_count;
}

bool mac_table_reset_stats(mac_table_t *table) {
    if (table && table->stats) {
        table->stats->total_inserts = 0;
        table->stats->total_deletes = 0;
        table->stats->total_expired = 0;
        return true;
    }
    return false;
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