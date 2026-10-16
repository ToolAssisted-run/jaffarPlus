/* trh GAMEDIR SEED HPLEVEL HP CHAIN.sol RUN.sol: trs with the other characters. Replays the chain, then prints the run
 * tick by tick: tick room x y frame hp seed level, then each of chars[0..4] in a room ([index:charid room x y frame
 * action hp alive direction]; the prince is not among them: a room's characters take the slots from 0), the opponent
 * slot as the tick leaves it (the last character processed: with a head and a snake in the room, the snake) and the
 * prince's opp_index. env REC=room: that room's records too ({index: tilepos x direction seq/pos hp skill type st
 * b0..b5}: st is the record's bytes +0x11.., a head's state (heads.c: 0 idle, 1 hunting, 2 retreating, 3 biting; its
 * spot's column and row) or a snake's wait timer (beast.c)).
 * Built with -DDRAWS -no-pie -Wl,--wrap=random_2751 it also prints every random draw on stderr ("draw TICK max=M ->
 * V from ADDR"; addr2line -f -e trh ADDR names the caller), e.g. which of a room's characters draws and when.
 * (Level 7's room 7: the head's drift and hunt against the prince in the wall; what a room entry draws.)
 *   gcc -O2 -w -o trh trh.c BUILD/extern/quickerSDLPoP2/extern/SDLPoP2/source/libsdlpop2.a -lm */
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
#ifdef DRAWS
int __real_random_2751(int); static int T_;
int __wrap_random_2751(int m) { int v = __real_random_2751(m); fprintf(stderr, "draw %d max=%d -> %d from %p\n", T_ + 1, m, v, __builtin_return_address(0)); return v; }
#define TICK(t) (T_ = (t))
#else
#define TICK(t) (void)0
#endif
static int hl, hv, done, hl2, hv2, done2;
static void hpfix(void) { if (!done2 && hl2 && pop2_level() == hl2) { Kid.f12 = Kid.f13 = start_hp = hv2; done2 = 1; } if (!done && hl && pop2_level() == hl) { Kid.f12 = Kid.f13 = start_hp = hv; done = 1; } }
int main(int c, char **v)
{
	pop2_set_game_version(0); pop2_set_skip_scenes(1); if (!pop2_init(v[1])) return 1; pop2_new_game(1, strtoul(v[2], 0, 0));
	hl = atoi(v[3]); hv = atoi(v[4]); hpfix(); if (getenv("HP14")) { hl2 = 14; hv2 = atoi(getenv("HP14")); }
	FILE *fp = fopen(v[5], "r"); char line[64]; pop2_input in; while (fgets(line, sizeof line, fp)) if (parse(line, &in)) { pop2_frame(&in); hpfix(); } fclose(fp);
	printf("start: level %d room %d x %d y %d frame %d hp %d/%d\n", pop2_level(), Kid.room, Kid.x, Kid.y, Kid.frame, Kid.f12, Kid.f13);
	fp = fopen(v[6], "r"); int t = 0;
	while (fgets(line, sizeof line, fp)) if (parse(line, &in)) { TICK(t); pop2_frame(&in); hpfix(); line[strcspn(line, "\n")] = 0; t++;
		printf("%d %d %d %d %d %d %u L%d", t, Kid.room, Kid.x, Kid.y, Kid.frame, Kid.f12, random_seed, pop2_level());
		for (int i = 0; i < 5; i++) if (chars[i].room) printf("  [%d:c%d r%d x%d y%d f%d a%d hp%d al%d d%d]", i, chars[i].charid, chars[i].room, chars[i].x, chars[i].y, chars[i].frame, chars[i].action, chars[i].f12, chars[i].alive, chars[i].direction);
		printf("  Opp(c%d r%d x%d y%d f%d) kopp=%d", Opp.charid, Opp.room, Opp.x, Opp.y, Opp.frame, Kid.opp_index);
		if (getenv("REC")) { int rm = atoi(getenv("REC")); int n = room_nchars(rm); for (int i = 0; i < n; i++) { level_char_init *r = room_char_record(i, rm); uint8_t *b = (uint8_t *)r; printf("  {%d: tp%d x%d d%d seq%x/%x hp%d f04=%d type%d st %d %d %d %d %d %d}", i, r->tilepos, r->x, r->direction, r->seq_id, r->seq_pos, r->hp, r->f04, r->type, b[0x11], b[0x12], b[0x13], b[0x14], b[0x15], b[0x16]); } }
		printf("\n"); }
}
