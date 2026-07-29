CC := clang
AR := ar

PKGS := sdl3 sdl3-image
CFLAGS := -Iinclude -Isrc -Ilib -I. -Iexamples/test-assets $(shell pkg-config --cflags $(PKGS))
LIBS := $(shell pkg-config --libs $(PKGS)) -lm

ENGINE_SRC := \
	src/console.c \
	src/engine.c \
	src/error.c \
	src/entity_components.c \
	src/graphics.c \
	src/math2d.c \
	src/physics.c \
	src/platform_process.c \
	src/rohr.c \
	src/systems.c \
	src/tools.c \
	src/level_editor.c\
	src/grid.c\
	src/controller.c \
	src/game_state.c

ENGINE_OBJ := $(patsubst src/%.c,build/obj/%.o,$(ENGINE_SRC))
ENGINE_OBJ += build/obj/yyjson.o
ENGINE_LIB := lib/librohr_engine.a
DOXYGEN := doxygen
PANDOC := pandoc
DOCS_DOXYFILE := docs/Doxyfile
DOCS_OUTPUT := build/docs
API_MARKDOWN := docs/public_api.md
PUBLIC_API_GENERATOR := scripts/generate_public_api_markdown.pl
README_HTML := $(DOCS_OUTPUT)/readme.html
STATIC_OUTPUT := build/static
STATIC_README_HTML := $(STATIC_OUTPUT)/readme.html

ASSET_SRC := \
	examples/test-assets/elder-fly/elderfly_descriptors.c

PIT_BINARY := build/examples/flies_in_pit
BALL_BINARY := build/examples/flies_around_ball
VIEW_BINARY := build/examples/view_port
FINISH_BINARY := build/examples/fly_to_finish
STATE_BINARY := build/examples/game_state

.PHONY: help all build build-engine build-example-pit build-example-ball build-example-view build-example-finish build-example-state run-pit run-ball run-view run-finish run-state docs clean-docs clean

help:
	@printf '%s\n' \
		"Build targets:" \
		"" \
		"  build" \
		"		  Builds all examples" \
		"		  Example source code is located in the examples/ directory" \
		"		  Compiled binaries are output to build/examples/" \
		"" \
		"  all" \
		"		  Equivalent to make build" \
		"" \
		"  build-engine" \
		"		  Builds lib/librohr_engine.a" \
		"" \
		"  build-example-pit" \
		"		  Builds examples/flies-in-pit/flies_in_pit.c" \
		"		  Outputs build/examples/flies_in_pit" \
		"" \
		"  build-example-ball" \
		"		  Builds examples/flies-around-ball/flies_around_ball.c" \
		"		  Outputs build/examples/flies_around_ball" \
		"" \
		"  build-example-view" \
		"		  Builds examples/view-port/view_port.c" \
		"		  Outputs build/examples/view_port" \
		"" \
		"  build-example-finish" \
		"		  Builds examples/fly-to-finish/fly_to_finish.c" \
		"		  Outputs build/examples/fly_to_finish" \
		"" \
		"  build-example-state" \
		"		  Builds examples/game-state/game_state.c" \
		"		  Outputs build/examples/game_state" \
		"" \
		"  run-pit" \
		"		  Builds and runs the flies_in_pit example" \
		"" \
		"  run-ball" \
		"		  Builds and runs the flies_around_ball example" \
		"" \
		"  run-view" \
		"		  Builds and runs the view_port example" \
		"" \
		"  run-finish" \
		"		  Builds and runs the fly_to_finish example" \
		"" \
		"  run-state" \
		"		  Builds and runs the JSON game-state example" \
		"" \
		"  docs" \
		"		  Updates docs/public_api.md from include/rohr.h" \
		"		  Builds Doxygen HTML docs into build/docs/html" \
		"		  Renders README.md into build/docs/readme.html" \
		"		  Renders a standalone README preview into build/static/readme.html" \
		"		  Copies linked README MP4 assets into build/static/docs/assets/" \
		"" \
		"  clean-docs" \
		"		  Removes generated documentation" \
		"" \
		"  clean" \
		"		  Removes the build directory"

all: build

build: build-example-view build-example-pit build-example-ball build-example-finish build-example-state

build-engine: $(ENGINE_LIB)

build-example-pit: $(PIT_BINARY)

build-example-ball: $(BALL_BINARY)

build-example-view: $(VIEW_BINARY)

build-example-finish: $(FINISH_BINARY)

build-example-state: $(STATE_BINARY)

$(ENGINE_LIB): $(ENGINE_OBJ)
	@mkdir -p lib
	$(AR) rcs $@ $^

build/obj/%.o: src/%.c
	@mkdir -p build/obj
	$(CC) -c $< $(CFLAGS) -o $@

build/obj/yyjson.o: lib/yyjson.c
	@mkdir -p build/obj
	$(CC) -c $< $(CFLAGS) -o $@

$(PIT_BINARY): examples/flies-in-pit/flies_in_pit.c $(ENGINE_LIB) $(ASSET_SRC)
	@mkdir -p build/examples
	$(CC) $^ $(CFLAGS) -o $@ $(LIBS)

$(BALL_BINARY): examples/flies-around-ball/flies_around_ball.c $(ENGINE_LIB) $(ASSET_SRC)
	@mkdir -p build/examples
	$(CC) $^ $(CFLAGS) -o $@ $(LIBS)

$(VIEW_BINARY): examples/view-port/view_port.c $(ENGINE_LIB) $(ASSET_SRC)
	@mkdir -p build/examples
	$(CC) $^ $(CFLAGS) -o $@ $(LIBS)

$(FINISH_BINARY): examples/fly-to-finish/fly_to_finish.c $(ENGINE_LIB) $(ASSET_SRC)
	@mkdir -p build/examples
	$(CC) $^ $(CFLAGS) -o $@ $(LIBS)

$(STATE_BINARY): examples/game-state/game_state.c $(ENGINE_LIB)
	@mkdir -p build/examples
	$(CC) $^ $(CFLAGS) -o $@ $(LIBS)

run-pit: $(PIT_BINARY)
	./$(PIT_BINARY)

run-ball: $(BALL_BINARY)
	./$(BALL_BINARY)

run-view: $(VIEW_BINARY)
	./$(VIEW_BINARY)

run-finish: $(FINISH_BINARY)
	./$(FINISH_BINARY)

run-state: $(STATE_BINARY)
	SDL_VIDEODRIVER=dummy ./$(STATE_BINARY)

docs: $(API_MARKDOWN) $(README_HTML) $(STATIC_README_HTML)
	$(DOXYGEN) $(DOCS_DOXYFILE)

$(API_MARKDOWN): include/rohr.h $(PUBLIC_API_GENERATOR)
	perl $(PUBLIC_API_GENERATOR) include/rohr.h $@

$(README_HTML): README.md docs/README.md $(API_MARKDOWN) docs/assets/flies_in_pit.gif docs/assets/flies_around_ball.gif
	@mkdir -p $(DOCS_OUTPUT)
	$(PANDOC) README.md --standalone --embed-resources --metadata title="Rohr Engine" -o $@

$(STATIC_README_HTML): README.md docs/README.md $(API_MARKDOWN) docs/assets/flies_in_pit.gif docs/assets/flies_around_ball.gif docs/assets/flies_in_pit.mp4 docs/assets/flies_around_ball.mp4
	@mkdir -p $(STATIC_OUTPUT)/docs/assets
	$(PANDOC) README.md --standalone --embed-resources --metadata title="Rohr Engine" -o $@
	cp docs/assets/flies_in_pit.mp4 $(STATIC_OUTPUT)/docs/assets/
	cp docs/assets/flies_around_ball.mp4 $(STATIC_OUTPUT)/docs/assets/

clean-docs:
	rm -rf $(DOCS_OUTPUT)

clean:
	rm -rf build lib/*.a
