#ifndef MAC_TABLE_LOG_H
#define MAC_TABLE_LOG_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stdio.h>

/* Default logging macros (can be overridden by user before including
 * mac_table.h) */

#ifndef MAC_LOGE
#define MAC_LOGE(tag, fmt, ...) printf("[E] %s: " fmt "\n", tag, ##__VA_ARGS__)
#endif

#ifndef MAC_LOGW
#define MAC_LOGW(tag, fmt, ...) printf("[W] %s: " fmt "\n", tag, ##__VA_ARGS__)
#endif

#ifndef MAC_LOGI
#define MAC_LOGI(tag, fmt, ...) printf("[I] %s: " fmt "\n", tag, ##__VA_ARGS__)
#endif

#ifndef MAC_LOGD
#define MAC_LOGD(tag, fmt, ...) printf("[D] %s: " fmt "\n", tag, ##__VA_ARGS__)
#endif

#endif // MAC_TABLE_LOG_H
#ifdef __cplusplus
}
#endif
