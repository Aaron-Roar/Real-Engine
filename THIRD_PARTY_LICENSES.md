# Third-party software

Rohr Engine vendors dependencies under `lib/`. Each dependency remains under
its own license:

- SDL3: zlib license, `lib/SDL/LICENSE.txt`.
- SDL3_image: zlib license, `lib/SDL_image/LICENSE.txt`.
- SDL3_ttf: zlib license, `lib/SDL_ttf/LICENSE.txt`.
- FreeType: FreeType License, with details in
  `lib/SDL_ttf/external/freetype/LICENSE.TXT` and
  `lib/SDL_ttf/external/freetype/docs/FTL.TXT`.
- Lua: MIT license, `lib/lua/LICENSE`.
- yyjson: MIT license, `lib/yyjson.LICENSE`.
- JetBrains Mono font: SIL Open Font License 1.1,
  `third_party_licenses/jetbrains_mono_ofl.txt`.

Additional source embedded within a vendored dependency retains the notices in
that dependency's source tree. SDK packaging installs the direct dependency
notices alongside Rohr's license files.
