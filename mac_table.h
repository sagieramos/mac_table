/**
 * @file mac_table.h
 * @brief MAC address table implementation with expiry management.
 *
 * This file was developed with the assistance of AI tools (OpenAI ChatGPT),
 * and adapted, tested, and maintained by Stanley Osagie Ramos
 * <sagiecyber@gmail.com>.
 *
 * © 2025 Stanley Osagie Stanley Ramos. Licensed under the MIT License.
 */

#ifndef MAC_TABLE_H
#define MAC_TABLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <freertos/FreeRTOS.h>
#include <stdbool.h>
#include <stdint.h>
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
} mac_entry_t;

/**
 * @brief Callback function type for MAC table events.
 *
 * @param slot_index Index of the affected slot.
 * @param status     Result or type of event.
 */
typedef void (*mac_table_event_callback_t)(size_t slot_index,
                                           mac_entry_result_t status);

struct mac_table_expiry_manager_t; /**< Forward declaration for expiry manager
                                    */

typedef struct mac_table_expiry_manager_t mac_table_expiry_manager_t;

/**
 * @brief Structure representing the MAC address table.
 */
typedef struct {
  mac_entry_t *entries;                /**< Array of table entries */
  size_t size;                         /**< Size of the entries array */
  uint32_t expiry_seconds;             /**< Time after which entries expire */
  mac_table_event_callback_t on_event; /**< Callback for events */
  mac_table_expiry_manager_t *expiry_manager; /**< Expiry manager pointer */
} mac_table_t;

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
 * @param table Pointer to the MAC table to initialize.
 * @param entries Array of entries to use.
 * @param size Size of the entries array.
 * @param expiry_seconds Expiration timeout in seconds.
 * @param on_event Callback to trigger on table events.
 */
void mac_table_init(mac_table_t *table, mac_entry_t *entries, size_t size,
                    uint32_t expiry_seconds,
                    mac_table_event_callback_t on_event);

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
mac_entry_result_t mac_table_get_entry(const mac_table_t *table, size_t index,
                                       mac_entry_t *info);

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
