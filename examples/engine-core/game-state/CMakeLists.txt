add_executable(game_state src/main.c)
rohr_add_example_runtime(game_state)
target_link_libraries(game_state PRIVATE rohr_engine)
rohr_stage_example_assets(game_state engine-core/game-state)
