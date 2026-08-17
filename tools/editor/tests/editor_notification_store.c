#include "panels/editor_notification_store.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    EditorNotificationStore store = {0};
    char summary[32];
    for(size_t i = 1; i <= 105; i += 1) {
        snprintf(summary, sizeof(summary), "notification %zu", i);
        assert(editor_notification_store_push(&store, summary, "detail", NULL,
            NULL));
    }
    assert(store.entry_count == 100);
    assert(store.entry_start == 5);
    assert(editor_notification_store_id_get(&store, 5) == NULL);
    assert(strcmp(editor_notification_store_id_get(&store, 6)->summary,
        "notification 6") == 0);
    assert(strcmp(editor_notification_store_newest_get(&store, 0)->summary,
        "notification 105") == 0);
    assert(strcmp(editor_notification_store_newest_get(&store, 99)->summary,
        "notification 6") == 0);
    assert(editor_notification_store_newest_get(&store, 100) == NULL);
    assert(store.toast_count == 3);
    assert(store.toast_ids[0] == 103);
    assert(store.toast_ids[1] == 104);
    assert(store.toast_ids[2] == 105);
    editor_notification_store_toast_remove(&store, 104);
    assert(store.toast_count == 2);
    assert(store.toast_ids[0] == 103);
    assert(store.toast_ids[1] == 105);
    return 0;
}
