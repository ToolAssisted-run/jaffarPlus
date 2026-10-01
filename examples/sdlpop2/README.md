# Prince of Persia 2 (SDLPoP2)

These scripts run on the QuickerSDLPoP2 emulator (`meson setup build -Demulator=QuickerSDLPoP2`).

They need the original game's files (DOS 1.0), which are not included. Put the game's folder, or a link to it, next to each script as `prince2`:

    ln -s /path/to/prince2 0101/prince2

The DOS release is `"Game Version"` in the emulator configuration, which every script names: `"1.1"` (the Collection CD's), `"1.0"` (on the same files) or `"IR"`, the initial release (on its own files). It is never taken from the files.

- `0101`: level 1, from the start to room 2.
- `0300`: level 3, its five routes for the big potions (+1 max HP: room 10 and room 20), from the level's start as the
  restart combo reaches it (`level0102.sol`: `0100/level01-restart.sol` then `0200/seeds/seed18.sol`, seed 7881336;
  level 3 at tick 535). `gen.py` writes the scripts (`script-ROUTE.jaffar`, 100 GB); `bigqueue.sh` runs them one after
  another. Ticks from the level's start to level 4:

  | route | big potions | ticks |
  |---|---|---|
  | `left`: 1 5 7 8 11 13 | none | 476 |
  | `right`: 1 4 16 3 2 14 15 17 19 22 30 31 30 | none | 594 |
  | `p1`: ... 14 10 14 ... 30 31 30 | room 10 | 752 |
  | `p2`: ... 22 18 20 21 26 27 28 25 24 23 13 | room 20 | 985 |
  | `both` | rooms 10, 20 | 1136 |
