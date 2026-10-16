#!/bin/bash
# chimera_check.sh PROJECT PACKAGE MOVIE : the project played by Chimera's own headless runner (chimera-run: a build
# of upstream's main in ~/chimera-new on jaffanator2, a worktree of ~/chimera; `meson setup build/meson-linux .`, then
# `meson compile -C build/meson-linux chimera-run miniboxhost zstd`) against the core's native runner on the movie the
# project was made from: the core's memory domains after the last frame, byte for byte. The firmware (the game's
# files, the MT-32's ROMs) comes from ~/pop2render/l8, a file an id.
PRJ=$1; PKG=$2; MOV=$3; G=$HOME/pop2render/l8; C=$HOME/chimera-new/build/meson-linux; T=/dev/shm/chk
DOMAINS=("Game State" "Kid" "Characters" "Level" "Mobs" "Trobs")
mkdir -p $T; rm -f $T/*
FW=(); for id in $(python3 -c "import json,sys; print(' '.join(f['id'] for f in json.load(open(sys.argv[1]))['firmware']))" $PRJ); do FW+=(--firmware "$id=$G/$id"); done
DU=(); for d in "${DOMAINS[@]}"; do DU+=(--dump "$d=$T/cr_${d// /_}.bin"); done
( cd $C && ./chimera-run --project $PRJ $PKG "${FW[@]}" "${DU[@]}" --meta $T/meta.txt > $T/cr_out.txt 2>&1 )
echo "chimera-run ($(git -C $HOME/chimera-new log -1 --format=%h)): $(grep -E '^(status|detail|frames|gameTime)=' $T/meta.txt | sort -u | tr '\n' ' ')"
grep -q "^status=OK" $T/meta.txt || { tail -3 $T/cr_out.txt; exit 1; }
N=$(wc -l < $MOV); bad=0
for d in "${DOMAINS[@]}"; do
  f=${d// /_}
  ( cd $G && ../bin/run-native-a5ee . --movie $MOV --frames $N --dump-domain "$d" $T/nat_$f.bin > /dev/null 2>&1 )
  if cmp -s $T/cr_$f.bin $T/nat_$f.bin; then printf "%s (%d bytes) equal; " "$d" $(stat -c %s $T/nat_$f.bin); else printf "%s DIFFERS; " "$d"; bad=1; fi
done
echo
[ $bad = 0 ] && echo "Chimera's runner against the core's runner after $N frames: every domain equal" || { echo "Chimera's runner DIFFERS from the core's runner"; exit 1; }
