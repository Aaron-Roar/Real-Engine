add_executable(user_interface src/main.c)
target_link_libraries(user_interface PRIVATE rohr_engine)
rohr_stage_example_assets(user_interface engine-core/user-interface)
