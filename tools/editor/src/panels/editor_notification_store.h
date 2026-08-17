#ifndef ROHR_EDITOR_NOTIFICATION_STORE_H
#define ROHR_EDITOR_NOTIFICATION_STORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EDITOR_NOTIFICATION_SUMMARY_MAX 160
#define EDITOR_NOTIFICATION_DETAIL_MAX 2048
#define EDITOR_NOTIFICATION_LOG_MAX 100
#define EDITOR_NOTIFICATION_TOAST_MAX 3

typedef struct EditorNotificationRecord {
    uint64_t id;
    char summary[EDITOR_NOTIFICATION_SUMMARY_MAX];
    char detail[EDITOR_NOTIFICATION_DETAIL_MAX];
} EditorNotificationRecord;

typedef struct EditorNotificationStore {
    EditorNotificationRecord entries[EDITOR_NOTIFICATION_LOG_MAX];
    size_t entry_start;
    size_t entry_count;
    uint64_t next_id;
    uint64_t toast_ids[EDITOR_NOTIFICATION_TOAST_MAX];
    uint64_t toast_created_ms[EDITOR_NOTIFICATION_TOAST_MAX];
    size_t toast_count;
} EditorNotificationStore;

bool editor_notification_store_push(EditorNotificationStore *store,
    const char *summary, const char *detail, uint64_t created_ms,
    size_t *physical_index, bool *replaced);
EditorNotificationRecord *editor_notification_store_id_get(
    EditorNotificationStore *store, uint64_t id);
EditorNotificationRecord *editor_notification_store_newest_get(
    EditorNotificationStore *store, size_t offset);
void editor_notification_store_toast_remove(EditorNotificationStore *store,
    uint64_t id);
void editor_notification_store_toasts_expire(EditorNotificationStore *store,
    uint64_t now_ms, uint64_t lifetime_ms);

#endif
