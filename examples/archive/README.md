# Archived examples

These configurations no longer load with the current engine and games: a rule action lacks a key its
game now requires, uses an action the game no longer has, or the game configuration carries a key the
game no longer reads. Fixing them needs values only their author knows (e.g. the intensities of
Super Mario Bros.' newer trace magnet, or the room a Prince of Persia magnet applies to), so they are kept
here, untouched, for reference. `tests/validateExamples.sh` skips this folder; paths inside them are
relative to their original folders.

| Example | Why it does not load |
|---|---|
| `gamegear/pop/lvl01.jaffar` | [Error] JSON contains no field called 'Room'. |
| `gamegear/pop/lvl02.jaffar` | [Error] JSON contains no field called 'Room'. |
| `gamegear/pop/lvl02a.jaffar` | [Error] JSON contains no field called 'Room'. |
| `gamegear/pop/lvl02b.jaffar` | [Error] JSON contains no field called 'Room'. |
| `gamegear/pop/lvl02c.jaffar` | [Error] JSON contains no field called 'Room'. |
| `gamegear/pop/lvl02d.jaffar` | [Error] JSON contains no field called 'Room'. |
| `gamegear/pop/lvl02e.jaffar` | [Error] JSON contains no field called 'Room'. |
| `gamegear/pop/lvl04a.jaffar` | [Error] JSON contains no field called 'Room'. |
| `gamegear/pop/lvl04b.jaffar` | [Error] JSON contains no field called 'Room'. |
| `gbc/pop/stage01.jaffar` | [ERROR] Rule label 1002 is used by more than one rule |
| `nes/microMachines/race14a.jaffar` | [ERROR] Rule label 100 is used by more than one rule |
| `nes/rockman/gameEndGlitch.search.jaffar` | [ERROR] Unrecognized action 'Set Code Pattern Magnet' in rule 1. Valid actions are: Add Reward, Trigger Fail, Trigger Win, Trigger Checkpoint, Trigg |
| `nes/superMarioBros/full.jaffar` | Specified Trace Magnet, but no trace file was provided. |
| `nes/superMarioBros/world101a.jaffar` | [Error] JSON contains no field called 'Intensity Pos X'. |
| `nes/superMarioBros/world101b.jaffar` | [Error] JSON contains no field called 'Intensity Pos X'. |
| `nes/superMarioBros/world101c.jaffar` | [Error] JSON contains no field called 'Intensity Pos X'. |
| `nes/superMarioBros/world102a.jaffar` | [Error] JSON contains no field called 'Intensity Pos X'. |
| `nes/superMarioBros/world401a.jaffar` | [Error] JSON contains no field called 'Intensity Pos X'. |
| `nes/superMarioBros/world401b.jaffar` | [Error] JSON contains no field called 'Intensity Pos X'. |
| `pipeBot/_runScript.jaffar` | [Error] Unrecognized key(s) in Game Configuration: 'Last Input Step Reward'. Check for typos or unsupported options. |
| `pipeBot/pipeBotScript.jaffar` | [Error] Unrecognized key(s) in Game Configuration: 'Last Input Step Reward'. Check for typos or unsupported options. |
| `raw/amiga/stage01.jaffar` | Unrecognized action 'Set Point Magnet' (the RAW / AnotherWorld game has none) |
| `raw/amiga/stage01b.jaffar` | Unrecognized action 'Set Point Magnet' (the RAW / AnotherWorld game has none) |
