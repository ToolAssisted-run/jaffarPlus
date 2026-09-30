# Prince of Persia 2 (SDLPoP2)

These scripts run on the QuickerSDLPoP2 emulator (`meson setup build -Demulator=QuickerSDLPoP2`).

They need the original game's files (DOS 1.0), which are not included. Put the game's folder, or a link to it, next to each script as `prince2`:

    ln -s /path/to/prince2 0101/prince2

The DOS release is `"Game Version"` in the emulator configuration, which every script names: `"1.1"` (the Collection CD's), `"1.0"` (on the same files) or `"IR"`, the initial release (on its own files). It is never taken from the files.

- `0101`: level 1, from the start to room 2.
