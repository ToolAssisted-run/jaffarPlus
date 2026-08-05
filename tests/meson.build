testCommonCppArgs = [ '-Wfatal-errors', '-Wall', '-Werror' ]
testCommonLinkArgs = [ '-lgtest_main', '-lgtest' ]
testTimeout = 120

# Default test setup: point LeakSanitizer at our suppressions so the asan/LSan job doesn't flake on
# libnuma's internal (unfreeable) per-thread cpumask leak (see testEmulator/lsan.supp). Applies to
# every test (`ninja test` uses the default setup); harmless for non-sanitizer builds, which ignore
# LSAN_OPTIONS.
add_test_setup('default',
  is_default: true,
  env: [ 'LSAN_OPTIONS=suppressions=' + meson.current_source_dir() + '/testEmulator/lsan.supp:print_suppressions=0' ])

###### Unit tests (pure logic, emulator-independent)

# Uses the system gtest (libgtest-dev), which every CI build job installs via apt.
# 'main: true' links gtest_main.
gtestDependency = dependency('gtest', main: true)

unitPropertyCondition = executable('unit-propertyAndCondition',
      'unit/propertyAndCondition.cpp',
      cpp_args : testCommonCppArgs + jaffarCPPFlags,
      include_directories : jaffarIncludes,
      dependencies : [ jaffarCommonDependency, gtestDependency ])

test('propertyAndCondition',
      unitPropertyCondition,
      suite : [ 'unit' ])

unitRuleInputSet = executable('unit-ruleAndInputSet',
      'unit/ruleAndInputSet.cpp',
      cpp_args : testCommonCppArgs + jaffarCPPFlags,
      include_directories : jaffarIncludes,
      dependencies : [ jaffarCommonDependency, gtestDependency ])

test('ruleAndInputSet',
      unitRuleInputSet,
      suite : [ 'unit' ])

# Two-tier hash DB correctness. hashDb.hpp pulls in numa.hpp (libnuma) and the
# OpenMP-based thread id, so this target links those explicitly.
unitHashDb = executable('unit-hashDb',
      'unit/hashDb.cpp',
      cpp_args : testCommonCppArgs + jaffarCPPFlags,
      include_directories : jaffarIncludes,
      dependencies : [ jaffarCommonDependency, gtestDependency, dependency('numa'), dependency('openmp') ])

test('hashDb',
      unitHashDb,
      suite : [ 'unit' ])

# Input-history strategies (None / Raw / shared-prefix Trie), the strict factory, and the underlying
# SequenceTrie (sharing, ref-count GC, concurrent sharded free lists). Needs threads for the concurrency
# test; the trie uses std::atomic/std::mutex (no numa, no OpenMP).
unitInputHistory = executable('unit-inputHistory',
      'unit/inputHistory.cpp',
      cpp_args : testCommonCppArgs + jaffarCPPFlags,
      include_directories : jaffarIncludes,
      dependencies : [ jaffarCommonDependency, gtestDependency, dependency('threads') ])

test('inputHistory',
      unitInputHistory,
      suite : [ 'unit' ])

###### Adding example validator test

bash = find_program('bash')
validateExamplesScript = 'validateExamples.sh'

test('ValidateExamples',
      bash,
      workdir : meson.current_source_dir(),
      timeout: testTimeout,
      args : [ validateExamplesScript, emulator ],
      suite : [ 'ScriptValidation' ])

###### Documentation anti-drift checks
#
# Backward check: every .jaffar config key the engine core (source/*.hpp) reads must be documented
# in docs/02-config-reference.md (and no documented core key may have vanished from the code). This
# is emulator-independent -- it only reads source and the markdown -- so it runs in every build.
python3prog = find_program('python3')

test('DocsConfigReference',
      python3prog,
      timeout: testTimeout,
      args : [ meson.current_source_dir() + '/../docs/tools/checkConfigDocs.py', meson.project_source_root() ],
      suite : [ 'docs' ])

######## Full run tests

# Setting env variables

## Test Emulator / GridWalker -- a designed puzzle with a known optimal solution,
## used to test the engine's main execution path end-to-end (search + reproduce).

if emulator == 'TestEmulator'

# Forward check: every example configuration shipped under docs/examples/ must pass --dryRun, so a
# renamed/removed engine key turns silent doc rot into a CI failure. ROM-free (TestEmulator).
test('DocsExamples',
      bash,
      workdir : meson.current_source_dir(),
      timeout: testTimeout,
      args : [ 'checkDocsExamples.sh', jaffar.full_path() ],
      suite : [ 'docs', 'TestEmulator' ])

# Core correctness: optimality + save-solution + reproduction + determinism + differential equivalence
test('gridWalker_reference_floor',
      bash,
      workdir : meson.current_source_dir() + '/testEmulator',
      timeout: testTimeout,
      args : [ 'checkReferenceFloor.sh', jaffar.full_path(), jaffarPlayer.full_path() ],
      suite : [ 'runs', 'TestEmulator', 'gridWalker' ])

test('gridWalker_optimal',
      bash,
      workdir : meson.current_source_dir() + '/testEmulator',
      timeout: testTimeout,
      args : [ 'checkOptimal.sh', jaffar.full_path(), jaffarPlayer.full_path() ],
      suite : [ 'runs', 'TestEmulator', 'gridWalker' ])

# Fail-rule pruning forces a detour around a trapped column: optimal solution is 6 moves
test('gridWalker_detour',
      bash,
      workdir : meson.current_source_dir() + '/testEmulator',
      timeout: testTimeout,
      args : [ 'runPuzzle.sh', jaffar.full_path(), 'gridWalker.detour.jaffar', '/tmp/jaffar.gw.detour.best.sol', 'win', '6' ],
      suite : [ 'runs', 'TestEmulator', 'gridWalker' ])

# Goal walled off by fail rules: the search must exhaust the space without a win
test('gridWalker_unreachable',
      bash,
      workdir : meson.current_source_dir() + '/testEmulator',
      timeout: testTimeout,
      args : [ 'runPuzzle.sh', jaffar.full_path(), 'gridWalker.unreachable.jaffar', '/tmp/jaffar.gw.unreachable.best.sol', 'nowin' ],
      suite : [ 'runs', 'TestEmulator', 'gridWalker' ])

# Max Steps set below the optimal depth: the engine must stop without finding a win
test('gridWalker_maxsteps',
      bash,
      workdir : meson.current_source_dir() + '/testEmulator',
      timeout: testTimeout,
      args : [ 'runPuzzle.sh', jaffar.full_path(), 'gridWalker.maxsteps.jaffar', '/tmp/jaffar.gw.maxsteps.best.sol', 'nowin' ],
      suite : [ 'runs', 'TestEmulator', 'gridWalker' ])

# Bypass Emulator State: the game serializes its own state instead of the emulator; optimal is still 8
test('gridWalker_bypass',
      bash,
      workdir : meson.current_source_dir() + '/testEmulator',
      timeout: testTimeout,
      args : [ 'runPuzzle.sh', jaffar.full_path(), 'gridWalker.bypass.jaffar', '/tmp/jaffar.gw.bypass.best.sol', 'win', '8' ],
      suite : [ 'runs', 'TestEmulator', 'gridWalker' ])

# Candidate Input Sets: a right-only allowed set cannot solve the maze, but the |D| candidate
# input (probed per-cell) completes it -- proving candidate inputs are tested, reported, and gated
# by "Test Candidate Inputs".
test('gridWalker_candidate_inputs',
      bash,
      workdir : meson.current_source_dir() + '/testEmulator',
      timeout: testTimeout,
      args : [ 'checkCandidate.sh', jaffar.full_path() ],
      suite : [ 'runs', 'TestEmulator', 'gridWalker' ])

# Hash database: correct with deduplication disabled, and correct while its sliding window rolls
# (older hash stores aged out) under a deliberately tiny store size.
test('gridWalker_hash_database',
      bash,
      workdir : meson.current_source_dir() + '/testEmulator',
      timeout: testTimeout,
      args : [ 'checkHashDb.sh', jaffar.full_path() ],
      suite : [ 'runs', 'TestEmulator', 'gridWalker' ])

# Frameskip (repeat-last): rate 1 holds each decided input for one extra frame, so the optimal
# 4-decision route to (4,4) records 8 inputs. The held frames must be recorded and reproduce to Win.
test('gridWalker_frameskip',
      bash,
      workdir : meson.current_source_dir() + '/testEmulator',
      timeout: testTimeout,
      args : [ 'checkReproduce.sh', jaffar.full_path(), jaffarPlayer.full_path(), 'gridWalker.frameskip.jaffar', '/tmp/jaffar.gw.frameskip.best.sol', '8', 'frameskip rate=1 repeat-last' ],
      suite : [ 'runs', 'TestEmulator', 'gridWalker' ])

# Frameskip (custom input): rate 1 with a custom "|.|" (no-op) held frame, so each of the 8 decided
# moves records a move + a no-op (16 inputs). Exercises the custom-input frameskip branch.
test('gridWalker_frameskip_custom',
      bash,
      workdir : meson.current_source_dir() + '/testEmulator',
      timeout: testTimeout,
      args : [ 'checkReproduce.sh', jaffar.full_path(), jaffarPlayer.full_path(), 'gridWalker.frameskip_custom.jaffar', '/tmp/jaffar.gw.frameskip_custom.best.sol', '16', 'frameskip rate=1 custom |.|' ],
      suite : [ 'runs', 'TestEmulator', 'gridWalker' ])

# Bypass Hash Calculation: the runner takes the game's getDirectStateHash() verbatim instead of its
# MetroHash pass. Dedup must still be correct, so the optimal solution is still 8 and reproduces.
test('gridWalker_bypasshash',
      bash,
      workdir : meson.current_source_dir() + '/testEmulator',
      timeout: testTimeout,
      args : [ 'checkReproduce.sh', jaffar.full_path(), jaffarPlayer.full_path(), 'gridWalker.bypasshash.jaffar', '/tmp/jaffar.gw.bypasshash.best.sol', '8', 'bypass hash calculation' ],
      suite : [ 'runs', 'TestEmulator', 'gridWalker' ])

# Hash Step Tolerance: a non-zero tolerance folds the step-phase into the state hash (so a cell may
# be revisited at different phases). Optimality must be preserved: the solution is still 8.
test('gridWalker_tolerance',
      bash,
      workdir : meson.current_source_dir() + '/testEmulator',
      timeout: testTimeout,
      args : [ 'checkReproduce.sh', jaffar.full_path(), jaffarPlayer.full_path(), 'gridWalker.tolerance.jaffar', '/tmp/jaffar.gw.tolerance.best.sol', '8', 'hash step tolerance=3' ],
      suite : [ 'runs', 'TestEmulator', 'gridWalker' ])

# Win State Collection + Stop Frames After First Win: a 4-step window past the first win harvests
# the later, longer wins too (dedup on "Steps" -> >= 2 saved solutions), and a Max Files cap of 1
# must terminate the run with "Win collection complete".
test('gridWalker_wincollect',
      bash,
      workdir : meson.current_source_dir() + '/testEmulator',
      timeout: testTimeout,
      args : [ 'checkWinCollect.sh', jaffar.full_path() ],
      suite : [ 'runs', 'TestEmulator', 'gridWalker' ])

# End On First Win State = false: two wins exist -- a shallow low-reward win at (4,0) (depth 4) and a
# deeper high-reward win at (4,4) (depth 8). Not stopping at the first win, the search must keep the
# higher-reward 8-move solution. (Stopping on first win would instead yield the 4-move one.)
test('gridWalker_searchall',
      bash,
      workdir : meson.current_source_dir() + '/testEmulator',
      timeout: testTimeout,
      args : [ 'checkReproduce.sh', jaffar.full_path(), jaffarPlayer.full_path(), 'gridWalker.searchall.jaffar', '/tmp/jaffar.gw.searchall.best.sol', '8', 'end-on-first-win=false' ],
      suite : [ 'runs', 'TestEmulator', 'gridWalker' ])

# Store Input History smaller than the solution: Max Size 4 with an 8-move optimum. The search is
# unaffected (it keys on state hashes, not the history), but the recorded solution is silently
# clamped to 4 inputs. Asserts graceful, memory-safe truncation (ASAN/UBSan job catches any OOB).
test('gridWalker_truncated_history',
      bash,
      workdir : meson.current_source_dir() + '/testEmulator',
      timeout: testTimeout,
      args : [ 'runPuzzle.sh', jaffar.full_path(), 'gridWalker.truncated.jaffar', '/tmp/jaffar.gw.truncated.best.sol', 'win', '4' ],
      suite : [ 'runs', 'TestEmulator', 'gridWalker' ])

## jaffar-player (reproduction tool) -- thorough headless exercise of its batch paths and the
## machine-checkable --printFinalState oracle, plus negative paths that must fail cleanly.

# Positive paths: deterministic reproduction to Win, partial (non-win) solution, not-allowed-input
# reporting, and the --runCommand quit/quicksave commands.
test('player_reproduction',
      bash,
      workdir : meson.current_source_dir() + '/testEmulator',
      timeout: testTimeout,
      args : [ 'checkPlayer.sh', jaffar.full_path(), jaffarPlayer.full_path() ],
      suite : [ 'reproductions', 'TestEmulator', 'gridWalker' ])

# Correctness of the player's diagnostic reports: not-allowed-input detection and repeated-state
# (duplicate-hash) detection, each checked against crafted solutions with an exact expected answer.
test('player_reports',
      bash,
      workdir : meson.current_source_dir() + '/testEmulator',
      timeout: testTimeout,
      args : [ 'checkPlayerReports.sh', jaffar.full_path(), jaffarPlayer.full_path() ],
      suite : [ 'reproductions', 'TestEmulator', 'gridWalker' ])

# --dumpReward: per-step reward trace of a replayed solution, checked against GridWalker's exact
# -(distance) + win-bonus rewards for a winning and a non-winning route (Reference Reward Floor source).
test('player_dump_reward',
      bash,
      workdir : meson.current_source_dir() + '/testEmulator',
      timeout: testTimeout,
      args : [ 'checkPlayerDumpReward.sh', jaffar.full_path(), jaffarPlayer.full_path() ],
      suite : [ 'reproductions', 'TestEmulator', 'gridWalker' ])

# Interactive frame advance/go-back controls: drives the real interactive loop via piped keystrokes
# and verifies step arithmetic, boundary clamping, hash consistency, and absolute loaded state.
test('player_navigation',
      bash,
      workdir : meson.current_source_dir() + '/testEmulator',
      timeout: testTimeout,
      args : [ 'checkPlayerNavigation.sh', jaffar.full_path(), jaffarPlayer.full_path() ],
      suite : [ 'reproductions', 'TestEmulator', 'gridWalker' ])

# Negative path: a missing config file must make the player fail cleanly.
test('player_missing_config',
      jaffarPlayer,
      workdir : meson.current_source_dir() + '/testEmulator',
      timeout: testTimeout,
      args : [ 'does_not_exist.jaffar', 'unused.sol', '--reproduce', '--disableRender', '--exitOnEnd', '--unattended' ],
      should_fail : true,
      suite : [ 'reproductions', 'TestEmulator', 'gridWalker' ])

# Negative path: a missing solution file must make the player fail cleanly.
test('player_missing_solution',
      jaffarPlayer,
      workdir : meson.current_source_dir() + '/testEmulator',
      timeout: testTimeout,
      args : [ 'gridWalker.jaffar', 'does_not_exist.sol', '--reproduce', '--disableRender', '--exitOnEnd', '--unattended' ],
      should_fail : true,
      suite : [ 'reproductions', 'TestEmulator', 'gridWalker' ])

# Negative path: a malformed (invalid JSON) config must make the player fail cleanly.
test('player_malformed_config',
      jaffarPlayer,
      workdir : meson.current_source_dir() + '/testEmulator',
      timeout: testTimeout,
      args : [ 'malformed.jaffar', 'unused.sol', '--reproduce', '--disableRender', '--exitOnEnd', '--unattended' ],
      should_fail : true,
      suite : [ 'reproductions', 'TestEmulator', 'gridWalker' ])

# Negative path: a non-numeric --frameskip must make the player fail cleanly.
test('player_bad_frameskip',
      jaffarPlayer,
      workdir : meson.current_source_dir() + '/testEmulator',
      timeout: testTimeout,
      args : [ 'gridWalker.jaffar', 'unused.sol', '--reproduce', '--disableRender', '--exitOnEnd', '--unattended', '--frameskip', 'abc' ],
      should_fail : true,
      suite : [ 'reproductions', 'TestEmulator', 'gridWalker' ])

endif

## NES / Sprilo

if emulator == 'QuickerNES'

testEnvVars = [ 'JAFFAR_ENGINE_OVERRIDE_MAX_STATEDB_SIZE_MB=10' ]
testEnvVars += 'JAFFAR_ENGINE_OVERRIDE_MAX_HASHDB_SIZE_MB=100'

test('race04_short',
      jaffar,
      workdir : meson.current_source_dir() + '/nes/sprilo',
      timeout: testTimeout,
      args : 'race04_short.jaffar',
      env : testEnvVars,
      suite : [ 'runs', 'quickerNES', 'sprilo' ])

test('race04_short_reproduce',
      jaffarPlayer,
      workdir : meson.current_source_dir() + '/nes/sprilo',
      timeout: testTimeout,
      args : [ 'race04_short.jaffar', 'race04_short.sol', '--reproduce', '--disableRender', '--exitOnEnd', '--unattended' ],
      suite : [ 'reproductions', 'quickerNES', 'sprilo' ])

# Negative path: a missing solution file must make the player fail cleanly
test('race04_short_missing_solution',
      jaffarPlayer,
      workdir : meson.current_source_dir() + '/nes/sprilo',
      timeout: testTimeout,
      args : [ 'race04_short.jaffar', 'does_not_exist.sol', '--reproduce', '--disableRender', '--exitOnEnd', '--unattended' ],
      should_fail : true,
      suite : [ 'reproductions', 'quickerNES', 'sprilo' ])

endif

## NES / Excite Bike -- native physics engine core (excitebike::Engine, via the exciteBot submodule)

if emulator == 'ExciteBikeNative'

# ROM-free: the native engine starts from its flat baseline (no race-start RAM seed), so these tests
# carry NO copyrighted ROM/RAM. They exercise the full integration end-to-end: the emulator API, the
# 64-byte packed state, the search (reaches the posX>200 win) and deterministic reproduction of the
# found solution. DB pools are shrunk via env (same mechanism the sprilo tests use).
exciteBikeNativeEnv  = [ 'JAFFAR_ENGINE_OVERRIDE_MAX_STATEDB_SIZE_MB=10' ]
exciteBikeNativeEnv += 'JAFFAR_ENGINE_OVERRIDE_MAX_HASHDB_SIZE_MB=100'

test('exciteBike_native_search',
      jaffar,
      workdir : meson.current_source_dir() + '/nes/exciteBike',
      timeout: testTimeout,
      args : 'flatRun.jaffar',
      env : exciteBikeNativeEnv,
      suite : [ 'runs', 'ExciteBikeNative', 'exciteBike' ])

test('exciteBike_native_reproduce',
      jaffarPlayer,
      workdir : meson.current_source_dir() + '/nes/exciteBike',
      timeout: testTimeout,
      args : [ 'flatRun.jaffar', 'flatRun.sol', '--reproduce', '--disableRender', '--exitOnEnd', '--unattended' ],
      suite : [ 'reproductions', 'ExciteBikeNative', 'exciteBike' ])

# Negative path: a missing solution file must make the player fail cleanly
test('exciteBike_native_missing_solution',
      jaffarPlayer,
      workdir : meson.current_source_dir() + '/nes/exciteBike',
      timeout: testTimeout,
      args : [ 'flatRun.jaffar', 'does_not_exist.sol', '--reproduce', '--disableRender', '--exitOnEnd', '--unattended' ],
      should_fail : true,
      suite : [ 'reproductions', 'ExciteBikeNative', 'exciteBike' ])

endif

## SDLPoP / Prince Of Persia

if emulator == 'QuickerSDLPoP'

testEnvVars = [ 'JAFFAR_ENGINE_OVERRIDE_MAX_STATEDB_SIZE_MB=10' ]
testEnvVars += 'JAFFAR_ENGINE_OVERRIDE_MAX_HASHDB_SIZE_MB=100'

test('lvl01',
      jaffar,
      workdir : meson.current_source_dir() + '/sdlpop',
      timeout: testTimeout,
      args : 'lvl01a.jaffar',
      env : testEnvVars,
      suite : [ 'runs', 'sdlpop' ])

test('lvl01_reproduce',
      jaffarPlayer,
      workdir : meson.current_source_dir() + '/sdlpop',
      timeout: testTimeout,
      args : [ 'lvl01a.jaffar', 'lvl01a.sol', '--reproduce', '--disableRender', '--exitOnEnd', '--unattended' ],
      suite : [ 'reproductions', 'sdlpop' ])

endif

if emulator == 'QuickerSnes9x'

## SNES / Christmas Craze

testEnvVars = [ 'JAFFAR_ENGINE_OVERRIDE_MAX_STATEDB_SIZE_MB=10' ]
testEnvVars += 'JAFFAR_ENGINE_OVERRIDE_MAX_HASHDB_SIZE_MB=100'

test('stage01',
      jaffar,
      workdir : meson.current_source_dir() + '/snes/christmasCraze',
      timeout: testTimeout,
      args : 'stage01.jaffar',
      env : testEnvVars,
      suite : [ 'runs', 'QuickerSnes9x', 'christmasCraze' ])

endif

## Genesis / Dino Runner

if emulator == 'QuickerGPGX'

testEnvVars = [ 'JAFFAR_ENGINE_OVERRIDE_MAX_STATEDB_SIZE_MB=10' ]
testEnvVars += 'JAFFAR_ENGINE_OVERRIDE_MAX_HASHDB_SIZE_MB=100'

test('stage01',
      jaffar,
      workdir : meson.current_source_dir() + '/genesis/dinoRunner',
      timeout: testTimeout,
      args : 'stage01.jaffar',
      env : testEnvVars,
      suite : [ 'runs', 'QuickerGPGX', 'dinoRunner' ])

endif

## A2600 / Hellway

if emulator == 'QuickerStella'

testEnvVars = [ 'JAFFAR_ENGINE_OVERRIDE_MAX_STATEDB_SIZE_MB=10' ]
testEnvVars += 'JAFFAR_ENGINE_OVERRIDE_MAX_HASHDB_SIZE_MB=100'

test('stage01',
      jaffar,
      workdir : meson.current_source_dir() + '/a2600/hellway',
      timeout: testTimeout,
      args : 'stage01.stella.jaffar',
      env : testEnvVars,
      suite : [ 'runs', 'QuickerStella', 'hellway' ])
      
endif

## A2600 / Hellway

if emulator == 'Atari2600Hawk'

testEnvVars = [ 'JAFFAR_ENGINE_OVERRIDE_MAX_STATEDB_SIZE_MB=10' ]
testEnvVars += 'JAFFAR_ENGINE_OVERRIDE_MAX_HASHDB_SIZE_MB=100'

test('stage01',
      jaffar,
      workdir : meson.current_source_dir() + '/a2600/hellway',
      timeout: testTimeout,
      args : 'stage01.a2600hawk.jaffar',
      env : testEnvVars,
      suite : [ 'runs', 'Atari2600Hawk', 'hellway' ])
      
endif

## NES / Super Mario Bros

if emulator == 'QuickerSMBC'

testEnvVars = [ 'JAFFAR_ENGINE_OVERRIDE_MAX_STATEDB_SIZE_MB=10' ]
testEnvVars += 'JAFFAR_ENGINE_OVERRIDE_MAX_HASHDB_SIZE_MB=100'

test('stage01',
      jaffar,
      workdir : meson.current_source_dir() + '/nes/superMarioBros',
      timeout: testTimeout,
      args : 'world101a.quickerSMBC.jaffar',
      env : testEnvVars,
      suite : [ 'runs', 'QuickerSMBC', 'superMarioBros' ])
endif

## ArkBot / Arkanoid

if emulator == 'QuickerArkBot'

testEnvVars = [ 'JAFFAR_ENGINE_OVERRIDE_MAX_STATEDB_SIZE_MB=10' ]
testEnvVars += 'JAFFAR_ENGINE_OVERRIDE_MAX_HASHDB_SIZE_MB=100'

test('stage01',
      jaffar,
      workdir : meson.current_source_dir() + '/arkbot',
      timeout: testTimeout,
      args : 'lvl01.jaffar',
      env : testEnvVars,
      suite : [ 'runs', 'QuickerArkBot', 'arkanoid' ])
endif


## QuickerMGBA / Toll Runner

if emulator == 'QuickerMGBA'

testEnvVars = [ 'JAFFAR_ENGINE_OVERRIDE_MAX_STATEDB_SIZE_MB=10' ]
testEnvVars += 'JAFFAR_ENGINE_OVERRIDE_MAX_HASHDB_SIZE_MB=100'

test('stage01',
      jaffar,
      workdir : meson.current_source_dir() + '/gba/tollRunner',
      timeout: testTimeout,
      args : 'stage01.jaffar',
      env : testEnvVars,
      suite : [ 'runs', 'QuickerMGBA', 'tollRunner' ])
endif


## QuickerGambatte / A Slime Travel

if emulator == 'QuickerGambatte'

testEnvVars = [ 'JAFFAR_ENGINE_OVERRIDE_MAX_STATEDB_SIZE_MB=10' ]
testEnvVars += 'JAFFAR_ENGINE_OVERRIDE_MAX_HASHDB_SIZE_MB=100'

test('stage01',
      jaffar,
      workdir : meson.current_source_dir() + '/gbc/aSlimeTravel',
      timeout: testTimeout,
      args : 'stage01.jaffar',
      env : testEnvVars,
      suite : [ 'runs', 'QuickerGambatte', 'aSlimeTravel' ])
endif

## QuickerDSDA / Doom

if emulator == 'QuickerDSDA'

testEnvVars = [ 'JAFFAR_ENGINE_OVERRIDE_MAX_STATEDB_SIZE_MB=100' ]
testEnvVars += 'JAFFAR_ENGINE_OVERRIDE_MAX_HASHDB_SIZE_MB=100'

# The 'stage01' doom run test is disabled: it needs the dsda-doom.wad engine support lump in the
# tests/doom working directory, which was purged from the repo (large binary / game data, supplied
# locally like a ROM). Re-enable once the WAD is provided out-of-tree.
# test('stage01',
#       jaffar,
#       workdir : meson.current_source_dir() + '/doom',
#       timeout: testTimeout,
#       args : 'freedoom011.reduced.e1m1.jaffar',
#       env : testEnvVars,
#       suite : [ 'runs', 'QuickerDSDA', 'Doom' ])
endif
