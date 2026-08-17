#include "editor_notification_store.h"

#include <stdio.h>

bool editor_notification_store_push(EditorNotificationStore *store,
        const char *summary, const char *detail, size_t *physical_index,
        bool *replaced) {
    size_t index;
    bool did_replace;
    if(store == NULL || summary == NULL || detail == NULL) return false;
    did_replace = store->entry_count == EDITOR_NOTIFICATION_LOG_MAX;
    if(!did_replace) {
        index = (store->entry_start + store->entry_count) %
            EDITOR_NOTIFICATION_LOG_MAX;
        store->entry_count += 1;
    } else {
        index = store->entry_start;
        store->entry_start = (store->entry_start + 1) %
            EDITOR_NOTIFICATION_LOG_MAX;
    }
    store->next_id += 1;
    if(store->next_id == 0) store->next_id = 1;
    store->entries[index] = (EditorNotificationRecord){.id = store->next_id};
    snprintf(store->entries[index].summary,
        sizeof(store->entries[index].summary), "%s", summary);
    snprintf(store->entries[index].detail,
        sizeof(store->entries[index].detail), "%s", detail);
    if(store->toast_count < EDITOR_NOTIFICATION_TOAST_MAX) {
        store->toast_ids[store->toast_count++] = store->next_id;
    } else {
        for(size_t i = 1; i < EDITOR_NOTIFICATION_TOAST_MAX; i += 1)
            store->toast_ids[i - 1] = store->toast_ids[i];
        store->toast_ids[EDITOR_NOTIFICATION_TOAST_MAX - 1] = store->next_id;
    }
    if(physical_index != NULL) *physical_index = index;
    if(replaced != NULL) *replaced = did_replace;
    return true;
}

EditorNotificationRecord *editor_notification_store_id_get(
        EditorNotificationStore *store, uint64_t id) {
    if(store == NULL || id == 0) return NULL;
    for(size_t i = 0; i < store->entry_count; i += 1) {
        size_t index = (store->entry_start + i) % EDITOR_NOTIFICATION_LOG_MAX;
        if(store->entries[index].id == id) return &store->entries[index];
    }
    return NULL;
}

EditorNotificationRecord *editor_notification_store_newest_get(
        EditorNotificationStore *store, size_t offset) {
    size_t logical;
    if(store == NULL || offset >= store->entry_count) return NULL;
    logical = store->entry_count - offset - 1;
    return &store->entries[(store->entry_start + logical) %
        EDITOR_NOTIFICATION_LOG_MAX];
}

void editor_notification_store_toast_remove(EditorNotificationStore *store,
        uint64_t id) {
    size_t write = 0;
    if(store == NULL) return;
    for(size_t i = 0; i < store->toast_count; i += 1) {
        if(store->toast_ids[i] != id)
            store->toast_ids[write++] = store->toast_ids[i];
    }
    store->toast_count = write;
}
