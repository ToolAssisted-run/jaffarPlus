/* trj GAMEDIR SEED HPLEVEL HP CHAIN.sol RUN.sol: trl for joining solutions (version 2: a level's old solution taken
 * up again after a stretch searched anew). Replays the chain, then prints the run tick by tick, a line a tick:
 *   tick level | room x y frame direction action fall_x fall_y hp maxhp seq_id f19 f10 seq_pos (routegen.py's
 *   WINSTATE, in its order) charid alive f0f f23 f24 hp_delta | seed |
 *   tiles' hash (every room's tiles and modifiers) | the drawn room's characters [charid room x y frame action hp alive]
 * The prince's fields are the ones the game's rules can ask (princeOfPersia2.hpp registerCharacter): a state with
 * all of them equal plays on the same under the same keys as long as the rest of the level is the same.
 *   gcc -O2 -w -o trj trj.c BUILD/extern/quickerSDLPoP2/extern/SDLPoP2/source/libsdlpop2.a -lm */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "/home/jaffar/sdlpop2/source/core.h"
#include "/home/jaffar/sdlpop2/source/types.h"
#include "/home/jaffar/sdlpop2/source/globals.h"
static int parse(const char *s, pop2_input *in)
{
	memset(in, 0, sizeof *in); if (strlen(s) < 10 || s[0] != (char)124) return 0;
	if (s[1] == 82) in->restart_level = 1; else if (s[1] == 77) in->music_toggle = 1; else if (s[1] == 75) in->keystroke = 1;
	in->x = s[3] == 76 ? -1 : s[4] == 82 ? 1 : 0; in->y = s[5] == 85 ? -1 : s[6] == 68 ? 1 : 0;
	in->shift = s[8] == 67 ? 2 : s[7] == 83 ? 1 : 0; return 1;
}
static int hl, hv, done;
static void hpfix(void) { if (!done && hl && pop2_level() == hl) { Kid.f12 = Kid.f13 = start_hp = hv; done = 1; } }
static unsigned tiles_hash(void)
{
	unsigned h = 2166136261u;
	for (int r = 1; r <= 32; r++) {
		const unsigned char *t = (const unsigned char *)ROOM_TILES(r), *a = (const unsigned char *)ROOM_ATTRS(r);
		for (int k = 0; k < 30; k++) h = (h ^ t[k]) * 16777619u;
		for (int k = 0; k < 120; k++) h = (h ^ a[k]) * 16777619u;
	}
	return h;
}
int main(int c, char **v)
{
	pop2_set_game_version(0); pop2_set_skip_scenes(1); if (!pop2_init(v[1])) return 1; pop2_new_game(1, strtoul(v[2], 0, 0));
	hl = atoi(v[3]); hv = atoi(v[4]); hpfix();
	FILE *fp = fopen(v[5], "r"); char line[64]; pop2_input in; while (fgets(line, sizeof line, fp)) if (parse(line, &in)) { pop2_frame(&in); hpfix(); } fclose(fp);
	fp = fopen(v[6], "r"); int t = 0;
	while (fgets(line, sizeof line, fp)) if (parse(line, &in)) { pop2_frame(&in); hpfix(); t++;
		printf("%d L%d | %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d | %u | %08x |", t, pop2_level(), Kid.room, Kid.x, Kid.y, Kid.frame, Kid.direction, Kid.action, Kid.fall_x, Kid.fall_y,
		       Kid.f12, Kid.f13, Kid.seq_id, Kid.f19, Kid.f10, Kid.seq_pos, Kid.charid, Kid.alive, Kid.f0f, Kid.f23, Kid.f24, Kid.hp_delta, (unsigned)random_seed, tiles_hash());
		int n = (int8_t)ROOM_REC(drawn_room)->nchars;
		for (int i = 0; i < n && i < 5; i++) printf(" [c%d r%d x%d y%d f%d a%d hp%d al%d]", chars[i].charid, chars[i].room, chars[i].x, chars[i].y, chars[i].frame, chars[i].action, chars[i].f12, chars[i].alive);
		printf("\n"); }
}
