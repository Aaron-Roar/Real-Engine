# Error handling {#errors}

Fallible public APIs return explicit result types generated with
`ERROR_DECLARE_RESULT_TYPE`. Every generated type uses the same result prefix,
so the generic public helpers accept any Rohr result value.

```c
EngineResult result = rohr_engine_init();
if(rohr_error_check(result)) {
    fprintf(stderr, "error %d: %s\n",
        result.result.error,
        rohr_error_message_get(result));
    return 1;
}
```

`rohr_error_message_get(result)` is the preferred top-level message. It combines
the stable Rohr error-code description with a captured lower-level cause, such
as the SDL error produced at the failing call site.

Use `rohr_error_result_message_get(result)` when only the captured result cause
is wanted, or `rohr_error_code_message_get(code)` for context-free information
about one `EngineError` value.

Engine modules should capture external-library details immediately, return a
real error, and let callers propagate it. Engine libraries do not own the
application's stderr, notification, recovery, or termination policy. Tools and
examples print the combined message and numeric error code only at their
top-level boundary.
