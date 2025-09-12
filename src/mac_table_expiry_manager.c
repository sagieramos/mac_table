#ifdef __cplusplus
extern "C" {
#endif
#include "mac_table.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
#include <time.h>
#include <string.h>
#include <esp_log.h>

static const char* TAG_EXPIRY = "MAC_EXPIRY";
#define EXPIRE_QUEUE_DEPTH 10
#define EXPIRE_TASK_STACK_SIZE 4096 // Increased stack size for safety

// Min-heap entry for tracking expirations
typedef struct {
    size_t slot_index;
    time_t expiry_time;
} HeapEntry;

// Min-heap structure
typedef struct {
    HeapEntry *entries;
    size_t size;
    size_t capacity;
} MinHeap;

// Message for the expiry task
typedef enum {
    MSG_EXPIRE_NOW
} ExpiryMsg;

// Expiry manager structure
struct mac_table_expiry_manager_t {
    mac_table_t *table;
    MinHeap *heap;
    TimerHandle_t expiry_timer;
    QueueHandle_t expiry_queue;
};

// **Improved Min-Heap Implementation**
MinHeap *min_heap_create(size_t capacity) {
    MinHeap *heap = pvPortMalloc(sizeof(MinHeap));
    if (!heap) return NULL;
    heap->entries = pvPortMalloc(sizeof(HeapEntry) * capacity);
    if (!heap->entries) {
        vPortFree(heap);
        return NULL;
    }
    heap->size = 0;
    heap->capacity = capacity;
    return heap;
}

void min_heap_free(MinHeap *heap) {
    if (heap) {
        vPortFree(heap->entries);
        vPortFree(heap);
    }
}

static inline size_t heap_parent(size_t index) { return (index - 1) / 2; }
static inline size_t heap_left_child(size_t index) { return 2 * index + 1; }
static inline size_t heap_right_child(size_t index) { return 2 * index + 2; }
static inline void heap_swap(HeapEntry *a, HeapEntry *b) { HeapEntry temp = *a; *a = *b; *b = temp; }

static void heapify_up(MinHeap *heap, size_t index) {
    while (index > 0 && heap->entries[heap_parent(index)].expiry_time > heap->entries[index].expiry_time) {
        heap_swap(&heap->entries[index], &heap->entries[heap_parent(index)]);
        index = heap_parent(index);
    }
}

static void heapify_down(MinHeap *heap, size_t index) {
    size_t min_index = index;
    size_t left = heap_left_child(index);
    size_t right = heap_right_child(index);

    if (left < heap->size && heap->entries[left].expiry_time < heap->entries[min_index].expiry_time) {
        min_index = left;
    }
    if (right < heap->size && heap->entries[right].expiry_time < heap->entries[min_index].expiry_time) {
        min_index = right;
    }

    if (min_index != index) {
        heap_swap(&heap->entries[index], &heap->entries[min_index]);
        heapify_down(heap, min_index);
    }
}

static int min_heap_insert(MinHeap *heap, size_t slot_index, time_t expiry_time) {
    if (heap->size >= heap->capacity) {
        return -1; // Heap is full
    }
    size_t index = heap->size++;
    heap->entries[index].slot_index = slot_index;
    heap->entries[index].expiry_time = expiry_time;
    heapify_up(heap, index);
    return 0;
}

static int min_heap_pop(MinHeap *heap, size_t *slot_index, time_t *expiry_time) {
    if (heap->size == 0) {
        return -1; // Heap is empty
    }
    *slot_index = heap->entries[0].slot_index;
    *expiry_time = heap->entries[0].expiry_time;
    heap->entries[0] = heap->entries[--heap->size];
    heapify_down(heap, 0);
    return 0;
}

static int min_heap_find_and_remove(MinHeap* heap, size_t slot_index) {
    for (size_t i = 0; i < heap->size; ++i) {
        if (heap->entries[i].slot_index == slot_index) {
            heap->entries[i] = heap->entries[--heap->size];
            heapify_down(heap, i);
            heapify_up(heap, i);
            return 0;
        }
    }
    return -1; // Not found
}

// Private function to check for protected roles
static bool is_protected_role(uint8_t role, const uint8_t *protected_roles) {
    if (!protected_roles) return false;
    for (const uint8_t *p = protected_roles; *p != 0; ++p) {
        if (*p == role) return true;
    }
    return false;
}

static void purge_expired_entries(mac_table_expiry_manager_t* manager) {
    if (!manager || !manager->table || !manager->heap) return;
    
    mac_table_t* table = manager->table;
    time_t now = time(NULL);
    size_t slot_index;
    time_t expiry_time;

    // Iterate through the heap and check for expired entries
    while (manager->heap->size > 0 && manager->heap->entries[0].expiry_time <= now) {
        // Pop the oldest entry
        if (min_heap_pop(manager->heap, &slot_index, &expiry_time) != 0) {
            continue;
        }

        // Check if the entry is still valid and has the correct slot index
        if (slot_index < table->size && table->entries[slot_index].state == SLOT_OCCUPIED &&
            table->entries[slot_index].expiration_time == expiry_time) {
            
            // Mark as expired and update stats
            table->entries[slot_index].state = SLOT_TOMBSTONE;
            if (table->stats) {
                table->stats->total_expired++;
                if (table->stats->active_entries > 0) {
                    table->stats->active_entries--;
                }
            }
            if (table->on_event) {
                table->on_event(slot_index, table->entries[slot_index].mac, MAC_TABLE_TIMEOUT);
            }
        }
    }
}

static void expiry_timer_callback(TimerHandle_t xTimer) {
    mac_table_expiry_manager_t* manager = (mac_table_expiry_manager_t*) pvTimerGetTimerID(xTimer);
    if (manager == NULL) return;

    // Send a message to the expiry task to trigger a purge
    ExpiryMsg msg = MSG_EXPIRE_NOW;
    if (xQueueSend(manager->expiry_queue, &msg, 0) != pdTRUE) {
        ESP_LOGE(TAG_EXPIRY, "Failed to send message to expiry queue.");
    }
}

static void expiry_task(void* arg) {
    mac_table_expiry_manager_t* manager = (mac_table_expiry_manager_t*) arg;
    ExpiryMsg msg;

    for (;;) {
        // Wait indefinitely for a message from the timer callback
        if (xQueueReceive(manager->expiry_queue, &msg, portMAX_DELAY) == pdTRUE) {
            if (msg == MSG_EXPIRE_NOW) {
                ESP_LOGD(TAG_EXPIRY, "Expiry task triggered. Purging entries...");
                purge_expired_entries(manager);

                // Check the heap for the next entry to expire and restart the timer
                if (manager->heap->size > 0) {
                    time_t now = time(NULL);
                    time_t next_expiry_time = manager->heap->entries[0].expiry_time;
                    int32_t wait_ms = (next_expiry_time > now) ? (next_expiry_time - now) * 1000 : 1;
                    xTimerChangePeriod(manager->expiry_timer, pdMS_TO_TICKS(wait_ms), 0);
                    xTimerStart(manager->expiry_timer, 0);
                } else {
                    xTimerStop(manager->expiry_timer, 0);
                }
            }
        }
    }
    vTaskDelete(NULL);
}


mac_table_expiry_manager_t *expiry_manager_create(mac_table_t *table) {
    mac_table_expiry_manager_t *manager = pvPortMalloc(sizeof(mac_table_expiry_manager_t));
    if (!manager) return NULL;

    manager->table = table;
    manager->heap = min_heap_create(table->size);
    if (!manager->heap) {
        vPortFree(manager);
        return NULL;
    }

    manager->expiry_queue = xQueueCreate(EXPIRE_QUEUE_DEPTH, sizeof(ExpiryMsg));
    if (manager->expiry_queue == NULL) {
        min_heap_free(manager->heap);
        vPortFree(manager);
        return NULL;
    }

    manager->expiry_timer = xTimerCreate(
        "mac_expiry_timer",
        pdMS_TO_TICKS(1000), // Initial period, will be updated
        pdFALSE,            // Auto-reload
        (void *)manager,      // Timer ID, passes a pointer to the manager
        expiry_timer_callback
    );
    if (manager->expiry_timer == NULL) {
        vQueueDelete(manager->expiry_queue);
        min_heap_free(manager->heap);
        vPortFree(manager);
        return NULL;
    }

    // Create a dedicated task for handling expiry events
    if (xTaskCreate(expiry_task, "expiry_task", EXPIRE_TASK_STACK_SIZE, manager, 5, NULL) != pdPASS) {
        xTimerDelete(manager->expiry_timer, 0);
        vQueueDelete(manager->expiry_queue);
        min_heap_free(manager->heap);
        vPortFree(manager);
        return NULL;
    }

    return manager;
}

void expiry_manager_free(mac_table_expiry_manager_t *manager) {
    if (manager) {
        if (manager->expiry_timer) {
            xTimerDelete(manager->expiry_timer, 0);
        }
        if (manager->expiry_queue) {
            vQueueDelete(manager->expiry_queue);
        }
        if (manager->heap) {
            min_heap_free(manager->heap);
        }
        vPortFree(manager);
    }
}

void expiry_manager_add_or_update(mac_table_expiry_manager_t *manager, size_t slot_index) {
    if (!manager || slot_index >= manager->table->size) return;

    time_t new_expiry_time = manager->table->entries[slot_index].expiration_time;
    
    // First, try to remove the old entry if it exists in the heap
    min_heap_find_and_remove(manager->heap, slot_index);
    
    // Then, insert the new one
    min_heap_insert(manager->heap, slot_index, new_expiry_time);

    // Restart the timer to reflect the soonest expiration
    if (manager->heap->size > 0) {
        time_t now = time(NULL);
        time_t next_expiry_time = manager->heap->entries[0].expiry_time;
        int32_t wait_ms = (next_expiry_time > now) ? (next_expiry_time - now) * 1000 : 1;
        xTimerChangePeriod(manager->expiry_timer, pdMS_TO_TICKS(wait_ms), 0);
        xTimerStart(manager->expiry_timer, 0);
    }
}

void expiry_manager_delete(mac_table_expiry_manager_t *manager, size_t slot_index) {
    if (!manager || slot_index >= manager->table->size) return;
    min_heap_find_and_remove(manager->heap, slot_index);
}

bool expiry_manager_remove_oldest(mac_table_expiry_manager_t *manager, const uint8_t *protected_roles) {
    if (!manager || !manager->heap || manager->heap->size == 0) return false;

    MinHeap *heap = manager->heap;
    size_t slot_index;
    time_t expiry_time;
    
    // Pop entries from the heap until a non-protected one is found or the heap is empty
    while (min_heap_pop(heap, &slot_index, &expiry_time) == 0) {
        if (slot_index >= manager->table->size) continue;

        mac_entry_t *entry = &manager->table->entries[slot_index];
        if (entry->state != SLOT_OCCUPIED) continue;
        
        if (is_protected_role(entry->role, protected_roles)) {
            // Re-insert this protected entry with its original timeout.
            min_heap_insert(heap, slot_index, entry->expiration_time);
            continue;
        }

        // Found a non-protected, oldest entry.
        entry->state = SLOT_TOMBSTONE;
        if (manager->table->stats) {
            manager->table->stats->total_deletes++;
            if (manager->table->stats->active_entries > 0) {
                manager->table->stats->active_entries--;
            }
        }
        if (manager->table->on_event) {
            manager->table->on_event(slot_index, entry->mac, MAC_TABLE_DELETED);
        }
        return true; // Successfully removed one entry
    }
    return false; // No non-protected entry to remove
}

#ifdef __cplusplus
}
#endif
