/**
 * @file mac_table.h
 * @brief MAC address table implementation with expiration and event management.
 *
 * This header defines the data structures and function prototypes for managing
 * a fixed-size MAC address table, including support for entry expiration, role
 * tagging, and customizable insertion behavior. Expired entries are
 * automatically purged via a timer-based expiry manager. Events such as
 * insertion, deletion, and expiration trigger optional user-defined callbacks.
 *
 * This file was developed with the assistance of AI tools (OpenAI ChatGPT) and
 * adapted, tested, and maintained by Stanley Osagie Ramos
 * <sagiecyber@gmail.com>.
 *
 * @author
 * Stanley Osagie Ramos
 *
 * @copyright
 * Copyright (c) 2025 Stanley Osagie Ramos
 *
 * @license
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef MAC_TABLE_H
#define MAC_TABLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <freertos/FreeRTOS.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* Length of a MAC address in bytes */
#define MAC_ADDR_LEN 6

/**
 * @brief Enum representing the state of a slot in the MAC table.
 */
typedef enum {
  SLOT_EMPTY,     /**< Slot is empty and available */
  SLOT_TOMBSTONE, /**< Slot was occupied but is now deleted */
  SLOT_OCCUPIED   /**< Slot is currently occupied */
} slot_state_t;

/**
 * @brief Enum representing the result of MAC table operations.
 */
typedef enum {
  MAC_TABLE_OK,        /**< Operation successful */
  MAC_TABLE_TIMEOUT,   /**< Entry found but has expired */
  MAC_TABLE_NOT_FOUND, /**< Entry not found */
  MAC_TABLE_INSERTED,  /**< Entry inserted */
  MAC_TABLE_UPDATED,   /**< Existing entry updated */
  MAC_TABLE_DELETED,   /**< Entry deleted */
  MAC_TABLE_FULL       /**< MAC table is full */
} mac_entry_result_t;

/**
 * @brief Structure representing a single entry in the MAC table.
 */
typedef struct {
  uint8_t mac[MAC_ADDR_LEN]; /**< MAC address bytes */
  time_t timeout_duration;   /**< Absolute expiration time */
  slot_state_t state;        /**< Current state of this slot */
  uint8_t role; /**< Role associated with this MAC address entry (e.g., client,
                   gateway) */
} mac_entry_t;

/**
 * @brief Callback function type for MAC table events.
 *
 * @param slot_index Index of the affected slot.
 * @param mac        Pointer to the MAC address involved in the event.
 * @param status     Result or type of event.
 */
typedef void (*mac_table_event_callback_t)(int slot_index, const uint8_t *mac,
                                           mac_entry_result_t status);

struct mac_table_expiry_manager_t; /**< Forward declaration for expiry manager
                                    */

typedef struct mac_table_expiry_manager_t mac_table_expiry_manager_t;

/**
 * @brief Structure for tracking statistics related to the MAC address table.
 *
 * This structure stores statistics that give insights into the operations
 * performed on the MAC address table, such as the number of inserts, deletes,
 * expired entries, and the number of active entries currently present in the
 * table.
 */
typedef struct {
  size_t total_inserts;  /**< Total number of entries inserted into the table */
  size_t total_deletes;  /**< Total number of entries deleted from the table */
  size_t total_expired;  /**< Total number of entries that have expired */
  size_t active_entries; /**< Number of entries currently active in the table
                            (not expired or deleted) */
} mac_table_stats_t;

/**
 * @brief Structure representing the MAC address table.
 *
 * This structure defines the MAC address table, which holds entries for MAC
 * addresses along with related information like expiration time, state, and
 * role. It also manages various operations like insertion, deletion, and expiry
 * through the included fields and associated functions.
 */
typedef struct {
  mac_entry_t *entries;                /**< Array of table entries */
  size_t size;                         /**< Size of the entries array */
  uint32_t expiry_seconds;             /**< Time after which entries expire */
  mac_table_event_callback_t on_event; /**< Callback for events */
  mac_table_expiry_manager_t *expiry_manager; /**< Expiry manager pointer */
  mac_table_stats_t *stats; /**< Pointer to the statistics structure */
} mac_table_t;

/**
 * @brief Structure representing options for inserting a MAC address into the
 * table.
 *
 * This structure defines the optional parameters that can be used when
 * inserting or updating a MAC address in the MAC address table. It includes
 * options for setting a custom expiration duration and assigning a role to the
 * entry.
 */
typedef struct {
  bool has_custom_duration; /**< Flag indicating if a custom expiration duration
                               is provided */
  time_t custom_duration;   /**< Custom expiration duration, used if
                               `has_custom_duration` is true */

  bool has_role; /**< Flag indicating if a role is specified */
  uint8_t role;  /**< The role to be assigned to the MAC address entry */
} mac_insert_options_t;

/**
 * @brief Insert a MAC address into the table.
 *
 * This function inserts a MAC address into the MAC address table. If the
 * address already exists, it will update the existing entry's expiration time
 * and role.
 *
 * @param table Pointer to the MAC address table.
 * @param mac The MAC address to insert.
 * @return mac_entry_result_t The result of the insertion operation. Possible
 * values:
 *         - MAC_TABLE_INSERTED: The MAC address was successfully inserted.
 *         - MAC_TABLE_UPDATED: The MAC address already exists and was updated.
 *         - MAC_TABLE_FULL: The table is full and cannot accommodate the new
 * entry.
 *         - MAC_TABLE_NOT_FOUND: The MAC address or table is invalid.
 */
mac_entry_result_t mac_table_insert(mac_table_t *table, const uint8_t *mac);

/**
 * @brief Insert or update a MAC address with custom options.
 *
 * This function inserts a MAC address into the MAC address table with
 * additional options, such as custom expiration duration or role. If the
 * address already exists, it will update the existing entry's expiration time,
 * role, and other properties based on the provided options.
 *
 * @param table Pointer to the MAC address table.
 * @param mac The MAC address to insert or update.
 * @param opts Optional parameters for insertion. Can include:
 *             - custom expiration duration (`custom_duration`).
 *             - custom role (`role`).
 * @return mac_entry_result_t The result of the insertion or update operation.
 * Possible values:
 *         - MAC_TABLE_INSERTED: The MAC address was successfully inserted.
 *         - MAC_TABLE_UPDATED: The MAC address already exists and was updated.
 *         - MAC_TABLE_FULL: The table is full and cannot accommodate the new
 * entry.
 *         - MAC_TABLE_NOT_FOUND: The MAC address or table is invalid.
 */
mac_entry_result_t mac_table_insert_ex(mac_table_t *table, const uint8_t *mac,
                                       const mac_insert_options_t *opts);

/**
 * @brief Create and initialize the expiry manager.
 *
 * @param table Pointer to the MAC table.
 * @return Pointer to the created expiry manager, or NULL on failure.
 */
mac_table_expiry_manager_t *expiry_manager_create(mac_table_t *table);

/**
 * @brief Free resources associated with the expiry manager.
 *
 * @param manager Pointer to the expiry manager.
 */
void expiry_manager_free(mac_table_expiry_manager_t *manager);

/**
 * @brief Add or update a slot in the expiry manager.
 *
 * @param manager Pointer to the expiry manager.
 * @param slot_index Index of the slot to add or update.
 */
void expiry_manager_add_or_update(mac_table_expiry_manager_t *manager,
                                  size_t slot_index);

/**
 * @brief Delete a slot from the expiry manager.
 *
 * @param manager Pointer to the expiry manager.
 * @param slot_index Index of the slot to delete.
 */
void expiry_manager_delete(mac_table_expiry_manager_t *manager,
                           size_t slot_index);

/**
 * @brief Initialize a MAC address table.
 *
 * This function initializes a MAC address table by allocating memory for the
 * entries, setting up the table's size, defining the expiration timeout, and
 * assigning a callback function for table events.
 *
 * @param table Pointer to the MAC address table to initialize.
 * @param entries Pointer to an array of MAC entries to store in the table.
 * @param size The size of the `entries` array.
 * @param expiry_seconds The expiration timeout for each entry in seconds.
 * @param on_event A callback function that will be triggered on table events
 * (e.g., insertion, deletion).
 * @return true if the table was successfully initialized, false otherwise.
 */
bool mac_table_init(mac_table_t *table, mac_entry_t *entries, size_t size,
                    size_t expiry_seconds, mac_table_event_callback_t on_event);

/**
 * @brief Insert or update a MAC address in the table.
 *
 * @param table Pointer to the MAC table.
 * @param mac MAC address to insert or update.
 * @return Result of the insertion.
 */
mac_entry_result_t mac_table_insert(mac_table_t *table, const uint8_t *mac);

/**
 * @brief Check if a MAC address exists in the table.
 *
 * @param table Pointer to the MAC table.
 * @param mac MAC address to check.
 * @return Result of the existence check.
 */
mac_entry_result_t mac_table_exists(const mac_table_t *table,
                                    const uint8_t *mac);

/**
 * @brief Delete a MAC address from the table.
 *
 * @param table Pointer to the MAC table.
 * @param mac MAC address to delete.
 * @return Result of the deletion.
 */
mac_entry_result_t mac_table_delete(mac_table_t *table, const uint8_t *mac);

/**
 * @brief Get a copy of a MAC table entry.
 *
 * @param table Pointer to the MAC table.
 * @param index Index of the entry to retrieve.
 * @param info Pointer to store the entry copy.
 * @return Result of the retrieval.
 */
mac_entry_result_t mac_table_get_by_index(const mac_table_t *table,
                                          size_t index, mac_entry_t *info);

bool mac_table_get_stats(const mac_table_t *table, mac_table_stats_t *stats);

/**
 * @brief Convert a MAC address to a string format.
 *
 * @param mac Pointer to MAC address bytes.
 * @param str Output buffer to store the string.
 */
void mac_to_str(const uint8_t *mac, char *str);

/**
 * @brief Convert a string representation of a MAC address to bytes.
 *
 * @param str Input string.
 * @param mac Output buffer for MAC address bytes.
 * @return true if conversion succeeded, false otherwise.
 */
bool str_to_mac(const char *str, uint8_t *mac);

#ifdef __cplusplus
}
#endif

#endif // MAC_TABLE_H
