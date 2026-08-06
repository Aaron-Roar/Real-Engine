#include "memory.h"

typedef struct TestObject {
    int value;
} TestObject;

MEMORY_DECLARE_OBJECT_POOL(TestPool, TestObject);
MEMORY_DEFINE_OBJECT_POOL(TestPool, TestObject)

void run_pool_test(void) {
    TestPool pool = {0};
    TestPoolResult result = {0};
    TestPoolObjectResult object_result = {0};
    TestObject first_object = {0};
    TestObject second_object = {0};
    TestObject third_object = {0};
    TestObject *first = NULL;
    TestObject *second = NULL;
    TestObject *third = NULL;

    /*
     * Initialize the pool with two object slots.
     * This is useful when a system knows the usual number of objects it needs,
     * such as particles, bullets, entities, or temporary frame objects. The
     * allocation happens once here instead of every time an object is requested.
     */
    result = TestPool_init(&pool, 2);
    if(result.kind == ERROR_RESULT_ERROR) {
        return;
    }

    /*
     * Store copies completed object values into available pool slots.
     * The returned pointers are owned by the pool and stay valid until released,
     * cleared, destroyed, or until the pool is expanded or shrunk. This matters
     * because users should store indexes or reacquire pointers after resizing if
     * they need long-lived references.
     */
    first_object.value = 10;
    object_result = TestPool_store(&pool, first_object);
    if(object_result.kind == ERROR_RESULT_VALUE) {
        first = object_result.result.value;
    }

    second_object.value = 20;
    object_result = TestPool_store(&pool, second_object);
    if(object_result.kind == ERROR_RESULT_VALUE) {
        second = object_result.result.value;
    }

    /*
     * Release objects before resizing when callers still hold raw pointers.
     * This avoids using stale pointers if the resize moves the backing memory.
     */
    result = TestPool_release(&pool, first);
    if(result.kind == ERROR_RESULT_ERROR) {
        (void)TestPool_destroy(&pool);
        return;
    }

    result = TestPool_release(&pool, second);
    if(result.kind == ERROR_RESULT_ERROR) {
        (void)TestPool_destroy(&pool);
        return;
    }

    /*
     * Expand the pool when more capacity is needed.
     * This is useful for load spikes, like spawning more effects than expected.
     * Expansion can move the backing memory, so any old object pointers should
     * be treated as invalid after a successful expand.
     */
    result = TestPool_expand(&pool, 1);
    if(result.kind == ERROR_RESULT_ERROR) {
        (void)TestPool_destroy(&pool);
        return;
    }

    /*
     * Store copies a completed object value into the first available slot.
     * It returns the same borrowed pointer shape as acquire, so callers can
     * still mutate the stored object while the slot is active.
     */
    third_object.value = 30;
    object_result = TestPool_store(&pool, third_object);
    if(object_result.kind == ERROR_RESULT_VALUE) {
        third = object_result.result.value;
    }

    /*
     * Release individual objects when they are no longer active.
     * This makes their slots reusable without freeing the whole pool, which is
     * useful for repeatedly created and destroyed objects during gameplay.
     */
    result = TestPool_release(&pool, third);
    if(result.kind == ERROR_RESULT_ERROR) {
        (void)TestPool_destroy(&pool);
        return;
    }

    /*
     * Shrink the pool after unused capacity is no longer needed.
     * Shrink only succeeds when the removed range contains no active objects.
     * This matters because it prevents silently invalidating live objects.
     */
    result = TestPool_shrink(&pool, 2);
    if(result.kind == ERROR_RESULT_ERROR) {
        (void)TestPool_destroy(&pool);
        return;
    }

    /*
     * Clear resets every slot to unused while keeping the allocated capacity.
     * This is useful at level reloads, scene resets, or frame-bound scratch
     * pools where the memory should be reused immediately afterward.
     */
    result = TestPool_clear(&pool);
    if(result.kind == ERROR_RESULT_ERROR) {
        (void)TestPool_destroy(&pool);
        return;
    }

    /*
     * Destroy frees the heap memory owned by the pool.
     * Call this when the owning system shuts down so the pool does not leak.
     */
    (void)TestPool_destroy(&pool);
}
