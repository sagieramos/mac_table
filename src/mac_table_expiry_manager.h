#ifndef MAC_TABLE_EXPIRY_MANAGER_H_
#define MAC_TABLE_EXPIRY_MANAGER_H_
#ifdef __cplusplus
extern "C" {
#endif
#include "mac_table.h"
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
#ifdef __cplusplus
}
#endif

#endif // MAC_TABLE_EXPIRY_MANAGER_H_
