# ExciteBikeNative emulator

Drives the native, byte-exact Excitebike physics model (`excitebike::Engine`) as a JaffarPlus
emulation core, instead of emulating the NES 6502. Far faster than QuickerNES with a tiny 64-byte
hashable state (see the [exciteBot](https://github.com/ToolAssisted-run/exciteBot) project and its
`docs/2d_engine_spec.md`).

## Source layout
- `exciteBikeNative.hpp` — the JaffarPlus emulator wrapper (this repo).
- `inputParser.hpp` — NES joypad parser (copied from quickerNES; same `|..|UDLRSsBA|` decoding).
- `engine.hpp` — **from the `extern/exciteBot` git submodule** (`extern/exciteBot/source/engine.hpp`),
  the single source of truth for the physics. Initialize with:
  `git submodule update --init extern/exciteBot`
- `track_layout.hpp`, `track_sections.hpp` — ROM-derived game data (per-lane terrain + section
  features). **Generated, gitignored** (like a ROM). Two ways to produce them:
  - **ROM-free (CI / flat-baseline tests):** `python3 gen_stub_track.py` writes a SYNTHETIC all-flat
    stub (no ROM content) — enough to build and run the flat tests. CI does this automatically.
  - **Full fidelity (needs the ROM / `tas.ram`):** regenerate the real headers from the submodule and
    copy them in (they take precedence — the stub generator only writes a header if it is missing):
    ```
    cd extern/exciteBot
    python3 tools/extract_track_layout.py        # writes source/track_layout.hpp (needs tas.ram)
    python3 tools/gen_sections_header.py          # writes source/track_sections.hpp
    cp source/track_layout.hpp source/track_sections.hpp ../../emulators/exciteBikeNative/
    ```

## Build & run
```
meson setup build-native -Demulator=ExciteBikeNative -Dgame=exciteBike
ninja -C build-native
```
The boot/countdown is not modeled, so seed the race-start RAM via the emulator-config key
`"Race Start RAM File Path"` (a 2048-byte snapshot; config `"Initial Sequence File Path"` empty).
See `examples/nes/exciteBike/race01a.native.jaffar`. With `"Bypass Emulator State": false`, the search
dedups on the 64-byte engine state.
