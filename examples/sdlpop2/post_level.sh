#!/bin/bash
# post_level.sh N TAG STAGE... : the final run's level N once its search TAG has found a solution (NN00/TAG.sol), on
# jaffanator2. The stages, in this order (ROUTING.md "The final run"):
#   cut    tools/cutlevel.py: NN00/lNNcut.sol (the level as the run plays it) and the next level's chain
#          (N+1)00/l0102..NNcut.sol
#   movie  the search core's trace of the level and tools/mvextend.py with it: ~/pop2tas/run_cut_lNN.mov, Chimera's
#          core checked against the search core on every tick (env AFTER: the rows pressed after the level)
#   video  ~/pop2render/l8/render-run.sh: levelN-TICKS.mp4 (the level's rows and 30 more; env SHOWHP=1)
#   dos    the chain in DOS in segments, both disks at once (sol2script.py with SYNC, orun.sh, tools/ocmp3.py); env
#          SCENES: the hits of the scene probe that get Space, as the run presses it (default "5": the scene after
#          level 5, whose skipping costs a draw; from level 8 on "5 6": the sword scene inside level 8, which does
#          not end without a key; from level 9 on "5 6 7": the scene after level 9; from level 13 on "5 6 7 8": the
#          scene after level 13; level 14: "5 6 7 8 9", its scene with Jaffar at the window), env END: the oracle's
#          last frame
#   project  the run so far as a Chimera project, ~/pop2tas/pop2-tas.chimeraProject (tools/mkproject.py, a marker a
#          level), after the packaged core (sandboxed: run-wbx with core.wbx, what Chimera loads) has played the
#          movie as the runner does. The package is a local build (~/pop2tas/sdlpop2-a5ee-local.chimeraCore, from
#          ~/pop2render/core-pkg: the core's tree with SDLPoP2's Music On/Off fix) until a release has the fix.
#          Then chimera_check.sh: the project played by Chimera's own headless runner, the core's memory domains at
#          the end against the core's runner.
# The chain before the level is NN00/l0102..(N-1)cut.sol, the movie before it ~/pop2tas/run_cut_l(N-1).mov.
# Level 14 (the last, searched in segments): its cut is 1400/run14.sh cut, not this script's; the other stages as
# for any level (TAG is not read by them).
set -e
N=$1; TAG=$2; shift 2
S=$HOME/jaffarPlus-run/examples/sdlpop2; SEED=7881336
NN=$(printf %02d $N); PP=$(printf %02d $((N - 1))); NX=$((N + 1))
D=$S/${NN}00; NXD=$S/$(printf %02d $NX)00
nums() { for i in $(seq 1 $1); do printf %02d $i; done; }
CHAIN=l$(nums $((N - 1)))cut.sol; NEWCHAIN=l$(nums $N)cut.sol; CUT=l${NN}cut.sol
for STAGE in "$@"; do
  case $STAGE in
  cut)
    mkdir -p $NXD; [ -e $NXD/prince2 ] || ln -s $(readlink -f $D/prince2) $NXD/prince2
    cd $D && python3 ../tools/cutlevel.py prince2 $SEED $CHAIN $TAG.sol $CUT $NXD/$NEWCHAIN
    ;;
  movie)
    cd $D && $HOME/pop2tools-m/trs prince2 $SEED 0 0 $CHAIN $CUT > /dev/shm/core_l$NN.txt
    # (level 14 is the last: no next level to look for, the movie is its rows and 20 empty ones, the final scene's start)
    [ $N -eq 14 ] && export PARTIAL=1
    cd $HOME/pop2render/l8 && CORE=/dev/shm/core_l$NN.txt python3 $S/tools/mvextend.py $HOME/pop2tas/run_cut_l$PP.mov $D/$CUT $NX $HOME/pop2tas/run_cut_l$NN.mov $AFTER
    ;;
  video)
    FIRST=$(wc -l < $HOME/pop2tas/run_cut_l$PP.mov); T=$(grep -c "^|" $D/$CUT)
    cp $HOME/pop2tas/run_cut_l$NN.mov /dev/shm/vid_l$NN.mov
    # the level ended on the stairs, its music cut: the next level's first row switches the music on again
    # (not level 14's: its key is the scene's, and the run ends there; env TAILROWS: the rows after the level, 30)
    if [ $N -lt 14 ] && tail -n +2 $D/$CUT | grep -q "^|M|"; then echo m >> /dev/shm/vid_l$NN.mov; fi
    for i in $(seq ${TAILROWS:-30}); do echo . >> /dev/shm/vid_l$NN.mov; done
    cd $HOME/pop2render/l8 && NEXT=$NX ./render-run.sh level$N-$T /dev/shm/vid_l$NN.mov $FIRST
    ;;
  dos)
    SOL=/dev/shm/dos_l$NN.sol; CORE=/dev/shm/core_dos$NN.txt; NAME=RUN$NN
    cp $NXD/$NEWCHAIN $SOL
    if [ $N -lt 14 ] && tail -n +2 $D/$CUT | grep -q "^|M|"; then echo "|M|......|" >> $SOL; else echo "|.|......|" >> $SOL; fi
    for i in $(seq 9); do echo "|.|......|" >> $SOL; done
    T=$(grep -c "^|" $SOL)
    cd $D && $HOME/pop2tools-m/trs prince2 $SEED 0 0 /dev/shm/empty.sol $SOL > $CORE
    cd $HOME/pop2dec/oracle
    E=${END:-$((13000 + T * 63 / 10 + 2500 * (N - 4)))}
    # (level 14: the lateness meter cleared at every tick, a machine never late, as the core is; room 3's tile draws
    # another way while the game is late, ahead of the guards' draws: ROUTING.md. env NOLATE0=1: left alone)
    if [ $N -ge 14 ] && [ -z "$NOLATE0" ]; then export LATE0=1; fi
    NOALT=1 SYNC=$CORE python3 sol2script.py $NAME $E $SOL
    DEFSC=5; [ $N -ge 8 ] && DEFSC="5 6"; [ $N -ge 9 ] && DEFSC="5 6 7"; [ $N -ge 13 ] && DEFSC="5 6 7 8"; [ $N -ge 14 ] && DEFSC="5 6 7 8 9"
    for k in ${SCENES:-$DEFSC}; do sed -i "s|^end $E|probepoke scene $k 41A 1E0020002039\nend $E|" $NAME.script; done
    for d in popmt32 pop; do ./orun.sh $NAME $d "prince yippeeyahoo LEVEL1" & done
    wait
    for d in popmt32 pop; do
      echo "== $d: $T ticks scripted, the oracle's last frame $E"
      python3 $HOME/pop2tools-run/ocmp3.py $NAME-$d-snap.txt $CORE
      grep -a -E "probe=[0-9]+ (scene|loadlevel) at" $NAME-$d-snap.txt | sed -E "s/ n=.*probe=[0-9]+ / /; s/ bx=.*//" | tr "\n" ";"; echo
    done
    ;;
  project)
    # (env COREPKG: the core's tree and package; default the local build of 2026-10-08. With a released package:
    # COREPKG="TREE PACKAGE", the tree built from the release's commit for its sandboxed core and tests/make-project.py)
    read -r P PKG <<< "${COREPKG:-$HOME/pop2render/core-pkg $HOME/pop2tas/sdlpop2-a5ee-local.chimeraCore}"
    MOV=$HOME/pop2tas/run_cut_l$NN.mov
    R=$(wc -l < $MOV); PR="Level,Kid.Room,Kid.X,Kid.Frame,Random Seed,Tick,IGT Ticks"
    cd $HOME/pop2render/l8
    $P/build/native/run-wbx $P/build/guest/core.wbx . --movie $MOV --frames $R --trace /dev/shm/tr_wbx.txt --trace-props "$PR" > /dev/null 2>&1
    ../bin/run-native-a5ee . --movie $MOV --frames $R --trace /dev/shm/tr_nat.txt --trace-props "$PR" > /dev/null 2>&1
    if cmp -s /dev/shm/tr_wbx.txt /dev/shm/tr_nat.txt; then echo "the packaged core (sandboxed) against the runner: $R rows equal; the last: $(tail -1 /dev/shm/tr_wbx.txt)"
    else echo "the packaged core DIFFERS from the runner"; exit 1; fi
    MARKS=("664:Level 1" "1107:Level 2" "1275:Level 3" "1673:Level 4")
    for k in $(seq 5 $N); do MARKS+=("$(wc -l < $HOME/pop2tas/run_cut_l$(printf %02d $((k - 1))).mov):Level $k"); done
    [ -n "$MARK" ] && MARKS+=("$MARK")   # (env MARK: one more, "ROW:NAME": the run's end)
    python3 $HOME/pop2tools-run/mkproject.py $PKG $P/waterbox/tests/make-project.py $MOV $HOME/pop2tas/pop2-tas.chimeraProject "${MARKS[@]}"
    # and the project in Chimera's own headless runner (a current build), against the core's runner
    [ -x $HOME/chimera-new/build/meson-linux/chimera-run ] && $S/chimera_check.sh $HOME/pop2tas/pop2-tas.chimeraProject $PKG $MOV
    ;;
  *) echo "unknown stage $STAGE"; exit 1;;
  esac
done
