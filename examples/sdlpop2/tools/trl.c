/* trl GAMEDIR SEED HPLEVEL HP CHAIN.sol RUN.sol: replay the chain, then print the run tick by tick (Max HP At Level as JaffarPlus) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "/home/jaffar/jaffarPlus-pop2/extern/quickerSDLPoP2/extern/SDLPoP2/source/core.h"
#include "/home/jaffar/jaffarPlus-pop2/extern/quickerSDLPoP2/extern/SDLPoP2/source/types.h"
#include "/home/jaffar/jaffarPlus-pop2/extern/quickerSDLPoP2/extern/SDLPoP2/source/globals.h"
static int parse(const char *s, pop2_input *in)
{
	memset(in, 0, sizeof *in); if (strlen(s) < 10 || s[0] != (char)124) return 0;
	if (s[1] == 82) in->restart_level = 1; else if (s[1] == 75) in->keystroke = 1;
	in->x = s[3] == 76 ? -1 : s[4] == 82 ? 1 : 0; in->y = s[5] == 85 ? -1 : s[6] == 68 ? 1 : 0;
	in->shift = s[8] == 67 ? 2 : s[7] == 83 ? 1 : 0; return 1;
}
static int hl, hv, done, hl2, hv2, done2;
static void hpfix(void) { if (!done2 && hl2 && pop2_level() == hl2) { Kid.f12 = Kid.f13 = start_hp = hv2; done2 = 1; } if (!done && hl && pop2_level() == hl) { Kid.f12 = Kid.f13 = start_hp = hv; done = 1; } }
int main(int c, char **v)
{
	pop2_set_game_version(0); pop2_set_skip_scenes(1); if (!pop2_init(v[1])) return 1; pop2_new_game(1, strtoul(v[2], 0, 0));
	hl = atoi(v[3]); hv = atoi(v[4]); hpfix(); if (getenv("HP14")) { hl2 = 14; hv2 = atoi(getenv("HP14")); }
	FILE *fp = fopen(v[5], "r"); char line[64]; pop2_input in; while (fgets(line, sizeof line, fp)) if (parse(line, &in)) { pop2_frame(&in); hpfix(); } fclose(fp);
	printf("start: level %d room %d x %d y %d frame %d hp %d/%d\n", pop2_level(), Kid.room, Kid.x, Kid.y, Kid.frame, Kid.f12, Kid.f13);
	fp = fopen(v[6], "r"); int t = 0;
	while (fgets(line, sizeof line, fp)) if (parse(line, &in)) { pop2_frame(&in); hpfix(); line[strcspn(line, "\n")] = 0; t++;
		printf("%4d %s  L%d room %2d x %3d y %3d row %d frame %3d (0x%02x) seq %3d hp %d/%d", t, line, pop2_level(), Kid.room, Kid.x, Kid.y, Kid.curr_row, Kid.frame, Kid.frame, Kid.f19, Kid.f12, Kid.f13);
		for (char *p = getenv("TILES"); p && *p; ) { int r = strtol(p, &p, 10); if (*p == 58) p++; int tp = strtol(p, &p, 10); if (*p == 44) p++; printf(" [%d:%d %02x/%08x]", r, tp, ROOM_TILES(r)[tp - 1], (unsigned)ROOM_ATTRS(r)[tp - 1]); }
		printf(" dir %d alive %d f24 %d\n", Kid.direction, Kid.alive, Kid.f24); }
}
