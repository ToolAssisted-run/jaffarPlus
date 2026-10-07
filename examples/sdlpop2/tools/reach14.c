/* reach14 GAMEDIR SEED HP ROOM X Y GOALROOM GOALROW DEPTH CAP: level 14, the prince flown (the fly cheat) to ROOM at
 * (X, Y); then a breadth-first search over his inputs for standing (frame 15) on GOALROW of GOALROOM; every (room, row)
 * he stands on is printed with its depth */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "/home/jaffar/jaffarPlus-pop2/extern/quickerSDLPoP2/extern/SDLPoP2/source/core.h"
#include "/home/jaffar/jaffarPlus-pop2/extern/quickerSDLPoP2/extern/SDLPoP2/source/types.h"
#include "/home/jaffar/jaffarPlus-pop2/extern/quickerSDLPoP2/extern/SDLPoP2/source/globals.h"
#include "/home/jaffar/jaffarPlus-pop2/extern/quickerSDLPoP2/extern/SDLPoP2/source/state.h"
static int parse(const char *s, pop2_input *in)
{
	memset(in, 0, sizeof *in); if (strlen(s) < 10 || s[0] != '|') return 0;
	in->x = s[3] == 'L' ? -1 : s[4] == 'R' ? 1 : 0; in->y = s[5] == 'U' ? -1 : s[6] == 'D' ? 1 : 0;
	in->shift = s[8] == 'C' ? 2 : s[7] == 'S' ? 1 : 0; return 1;
}
static const char *ins[] = { "|.|......|", "|.|L.....|", "|.|.R....|", "|.|..U...|", "|.|...D..|", "|.|....S.|", "|.|L.U...|", "|.|.RU...|",
                             "|.|L...S.|", "|.|.R..S.|", "|.|..U.S.|", "|.|...DS.|", "|.|.RU.S.|", "|.|L.U.S.|", "|.|.....C|", "|.|L....C|", "|.|.R...C|" };
#define NIN 17
#define HS (1 << 26)
static uint64_t *seen;
static int seen_add(uint64_t h) { if (!h) h = 1; uint32_t i = h & (HS - 1); while (seen[i]) { if (seen[i] == h) return 0; i = (i + 1) & (HS - 1); } seen[i] = h; return 1; }
typedef struct { int parent; uint8_t in; } node;
int main(int c, char **v)
{
	setvbuf(stdout, NULL, _IONBF, 0); pop2_set_game_version(0); pop2_set_skip_scenes(1); if (!pop2_init(v[1])) return 1; pop2_new_game(14, strtoul(v[2], 0, 0));
	int hp = atoi(v[3]), room = atoi(v[4]), tx = atoi(v[5]), ty = atoi(v[6]), groom = atoi(v[7]), grow = atoi(v[8]), depth = atoi(v[9]); size_t cap = atoi(v[10]);
	pop2_input z = {0}, in = {0};
	Kid.f12 = Kid.f13 = start_hp = (uint8_t)hp;
	for (int t = 0; t < 4; t++) pop2_frame(&z);
	cheat_fly_key = 1;
	for (int t = 0; t < 4000 && !(Kid.room == room && abs(Kid.y - ty) < 6); t++) { memset(&in, 0, sizeof in); in.y = (Kid.room != room) ? -1 : (Kid.y > ty ? -1 : 1); pop2_frame(&in); }
	for (int t = 0; t < 4000 && abs(Kid.x - tx) >= 6; t++) { memset(&in, 0, sizeof in); in.x = Kid.x > tx ? -1 : 1; pop2_frame(&in); }
	cheat_fly_key = 0; for (int k = 0; k < 12; k++) pop2_frame(&z);
	printf("placed: room %d x %d y %d row %d col %d frame %d dir %d hp %d/%d alive %d sword %d\n", Kid.room, Kid.x, Kid.y, Kid.curr_row, Kid.curr_col, Kid.frame, Kid.direction, Kid.f12, Kid.f13, Kid.alive, Kid.f10);
	size_t sz = pop2_state_size(); seen = calloc(HS, 8);
	uint8_t *cur = malloc(cap * sz), *nxt = malloc(cap * sz); size_t ncur = 1; pop2_save(cur); seen_add(state_hash());
	node **tree = calloc(depth + 1, sizeof *tree); tree[0] = calloc(1, sizeof(node));
	static int got[33][5];
	for (int d = 1; d <= depth && ncur; d++) {
		size_t nn = 0; tree[d] = malloc(cap * sizeof(node));
		for (size_t i = 0; i < ncur; i++)
			for (int k = 0; k < NIN; k++) {
				pop2_load(cur + i * sz); pop2_input q; parse(ins[k], &q); pop2_frame(&q);
				if (Kid.alive >= 0 || Kid.f12 == 0 || pop2_level() != 14 || Kid.room == 0) continue;
				if (getenv("ONLY4") && Kid.room != 4 && !(Kid.room == 3 && Kid.y < 70 && Kid.frame != 15)) continue;
				if (!seen_add(state_hash())) continue;
				if (nn >= cap) continue;
				pop2_save(nxt + nn * sz); tree[d][nn].parent = (int)i; tree[d][nn].in = (uint8_t)k;
				int r = Kid.room, w = Kid.curr_row < 0 ? 3 : Kid.curr_row > 2 ? 4 : Kid.curr_row;
				if (r <= 32 && !got[r][w] && Kid.frame == 15) {
					got[r][w] = d; printf("depth %3d: standing in room %2d row %d x %3d hp %d/%d\n", d, r, Kid.curr_row, Kid.x, Kid.f12, Kid.f13);
					if (r == groom && Kid.curr_row == grow) {
						FILE *o = fopen("reach14.sol", "w"); int idx = (int)nn; const char *seq[4096]; int n = 0;
						for (int dd = d; dd > 0; dd--) { seq[n++] = ins[tree[dd][idx].in]; idx = tree[dd][idx].parent; }
						while (n) fprintf(o, "%s\n", seq[--n]); fclose(o); printf("  -> reach14.sol\n"); return 0;
					}
				}
				nn++;
			}
		uint8_t *t = cur; cur = nxt; nxt = t; ncur = nn;
		if (d % 10 == 0) printf("depth %d: %zu states\n", d, ncur);
	}
	printf("goal not reached\n");
}
