/* seedscan GAMEDIR L1.sol L2.sol FROM TO: for each Random Seed setting s in [FROM, TO) (the game advances it once at
 * start-up: level 1 begins with s * 214013 + 2531011), level 1's solution, the wait for level 2 (the level-end music),
 * then level 2's solution: prints the settings under which level 1 ends as it should, with level 2's puzzle (the sand
 * tiles' variant byte_14a0 and the right tile's column puzzle_answer, both drawn as the level starts) and whether
 * level 2's solution reaches level 3 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "/home/jaffar/jaffarPlus-run/extern/quickerSDLPoP2/extern/SDLPoP2/source/core.h"
#include "/home/jaffar/jaffarPlus-run/extern/quickerSDLPoP2/extern/SDLPoP2/source/types.h"
#include "/home/jaffar/jaffarPlus-run/extern/quickerSDLPoP2/extern/SDLPoP2/source/globals.h"
static int parse(const char *s, pop2_input *in)
{
	memset(in, 0, sizeof *in); if (strlen(s) < 10 || s[0] != (char)124) return 0;
	if (s[1] == 82) in->restart_level = 1; else if (s[1] == 75) in->keystroke = 1;
	in->x = s[3] == 76 ? -1 : s[4] == 82 ? 1 : 0; in->y = s[5] == 85 ? -1 : s[6] == 68 ? 1 : 0;
	in->shift = s[8] == 67 ? 2 : s[7] == 83 ? 1 : 0; return 1;
}
static pop2_input m1[4000], m2[4000]; static int n1, n2;
static int load(const char *p, pop2_input *m) { FILE *f = fopen(p, "r"); char l[64]; int n = 0; while (f && fgets(l, sizeof l, f)) if (parse(l, &m[n])) n++; if (f) fclose(f); return n; }
int main(int c, char **v)
{
	pop2_set_game_version(0); pop2_set_skip_scenes(1); if (!pop2_init(v[1])) return 1;
	n1 = load(v[2], m1); n2 = load(v[3], m2); long a = atol(v[4]), b = atol(v[5]); pop2_input idle; memset(&idle, 0, sizeof idle);
	for (long s = a; s < b; s++) {
		pop2_new_game(1, (uint32_t)s * 214013u + 2531011u);
		int t = 0, ok = 1;
		for (int i = 0; i < n1 && pop2_level() == 1; i++) { pop2_frame(&m1[i]); t++; }
		if ((int8_t)Kid.alive >= 0) ok = 0;
		for (int i = 0; ok && i < 400 && pop2_level() == 1; i++) { pop2_frame(&idle); t++; }
		if (!ok || pop2_level() != 2) continue;
		int t2 = 0, var = -1, ans = -1; uint32_t seed2 = random_seed;
		for (int i = 0; i < n2 && pop2_level() == 2; i++) { pop2_frame(&m2[i]); t2++; if (var < 0 && byte_14a0 != 0xFF) { var = byte_14a0; ans = puzzle_answer; } }
		for (int i = 0; i < 40 && pop2_level() == 2 && (int8_t)Kid.alive < 0; i++) { pop2_frame(&idle); t2++; }
		printf("%ld level2 at %d seed %u puzzle %d/%d %s %d\n", s, t, seed2, var, ans, pop2_level() == 3 ? "LEVEL3" : "no", t2);
	}
	return 0;
}
