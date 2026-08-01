add_executable(basic_editor src/main.c)
target_link_libraries(basic_editor PRIVATE rohr_editor)
rohr_stage_example_assets(basic_editor editor/basic-editor)
