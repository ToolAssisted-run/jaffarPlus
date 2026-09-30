# Prince of Persia 2 (SDLPoP2)

These scripts run on the QuickerSDLPoP2 emulator (`meson setup build -Demulator=QuickerSDLPoP2`).

They need the original game's files (DOS 1.0), which are not included. Put the game's folder, or a link to it, next to each script as `prince2`:

    ln -s /path/to/prince2 0101/prince2

The DOS release played follows the files: the Collection CD's are 1.1. `"Game Version"` in the emulator configuration chooses another one: `"1.1"`, `"1.0"` (on the same files) or `"IR"`, the initial release (on its own files); `"Auto"`, the default, plays the files' own.

- `0101`: level 1, from the start to room 2.
