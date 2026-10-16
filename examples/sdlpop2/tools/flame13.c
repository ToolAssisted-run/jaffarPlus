/* flame13 GAMEDIR SEED HP X0 MODE ...: level 13's room 4 by experiment. A new game at level 13 with HP hit points; the
 * prince flown (the fly cheat: no physics) left along the middle row into room 4 to x X0, then:
 *   trace N [KILLAT]  N ticks without input, events printed (KILLAT: from that tick on his hit points are 1)
 *   bfs CAP DEPTH     a beam search over his inputs: the exit doors opened alive (the plate), then back into room 29 */
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
static unsigned d0v(int set) { static unsigned v; if (set) v = (unsigned)ROOM_ATTRS(28)[16] & 0xFFFF; return v; }
static unsigned door(void) { return (unsigned)ROOM_ATTRS(28)[16] & 0xFFFF; }
static void show(const char *tag, int t)
{
	printf("%s %4d: room %d x %d y %d row %d col %d dir %d frame %d seq %d hp %d/%d alive %d f24 %d | plate %08x flame %08x door28 %08x | w0996 %d 5cdc %x 5cda %x |", tag, t, Kid.room, Kid.x, Kid.y, Kid.curr_row, Kid.curr_col,
	       Kid.direction, Kid.frame, Kid.f19, Kid.f12, Kid.f13, Kid.alive, Kid.f24, (unsigned)ROOM_ATTRS(4)[12], (unsigned)ROOM_ATTRS(4)[11], (unsigned)ROOM_ATTRS(28)[16], (int16_t)word_0996, word_5cdc, word_5cda);
	int n = (int8_t)ROOM_REC(Kid.room)->nchars;
	for (int i = 0; i < n && i < 5; i++) printf(" [c%d id %d x %d row %d frame %d hp %d alive %d f24 %d]", i, chars[i].charid, chars[i].x, chars[i].curr_row, chars[i].frame, chars[i].f12, chars[i].alive, chars[i].f24);
	printf("\n");
}
typedef struct { int parent; uint8_t in; } node;
int main(int c, char **v)
{
	setvbuf(stdout, NULL, _IONBF, 0); pop2_set_game_version(0); pop2_set_skip_scenes(1); if (!pop2_init(v[1])) return 1; pop2_new_game(13, strtoul(v[2], 0, 0));
	int hp = atoi(v[3]), x0 = atoi(v[4]); pop2_input z = {0}, l = {0}; l.x = -1;
	Kid.f12 = Kid.f13 = start_hp = (uint8_t)hp;
	for (int t = 0; t < 14; t++) pop2_frame(&z);
	show("start", 0);
	cheat_fly_key = 1; int t = 0;
	for (; t < 2000 && !(Kid.room == 4 && Kid.x <= x0); t++) pop2_frame(&l);
	cheat_fly_key = 0; for (int k = 0; k < 6; k++) pop2_frame(&z);
	show("flown", t); d0v(1);
	if (!strcmp(v[5], "trace")) {
		int n = atoi(v[6]), killat = c > 7 ? atoi(v[7]) : -1; unsigned pd = door(), pp = (unsigned)ROOM_ATTRS(4)[12]; int pa = Kid.alive, ph = Kid.f12, pn = (int8_t)ROOM_REC(4)->nchars, pf24 = Kid.f24, p96 = (int16_t)word_0996; unsigned pdc = word_5cdc;
		for (int k = 1; k <= n; k++) {
			if (killat >= 0 && k >= killat && Kid.alive < 0 && Kid.f12 > 1 && Kid.f24 != 0xD) Kid.f12 = 1;
			pop2_frame(&z);
			int nn = (int8_t)ROOM_REC(4)->nchars;
			if (door() != pd && (pd == 0 || door() == 0xFF) || (unsigned)ROOM_ATTRS(4)[12] != pp || (Kid.alive != pa && (Kid.alive < 1 || Kid.alive > 6)) || Kid.f12 != ph || nn != pn || Kid.f24 != pf24 || (int16_t)word_0996 != p96 || word_5cdc != pdc || k % 100 == 0 || k == n)
				show("tick", k);
			pd = door(); pp = (unsigned)ROOM_ATTRS(4)[12]; pa = Kid.alive; ph = Kid.f12; pn = nn; pf24 = Kid.f24; p96 = (int16_t)word_0996; pdc = word_5cdc;
		}
		return 0;
	}
	if (!strcmp(v[5], "back")) {
		/* from where he was flown to (the plate): a breadth-first search back right into room 29 */
		size_t cap = atoi(v[6]); int depth = atoi(v[7]); size_t sz = pop2_state_size(); seen = calloc(HS, 8); size_t room_ = 17 * cap;
		uint8_t *cur = malloc(cap * sz), *nxt = malloc(room_ * sz); int *sc = malloc(room_ * sizeof(int)); node **tree = calloc(depth + 1, sizeof *tree); node *tn = malloc(room_ * sizeof(node));
		for (int k = 0; k < 12; k++) pop2_frame(&z);
		show("from", 0);
		size_t ncur = 1; pop2_save(cur); seen_add(state_hash()); tree[0] = calloc(1, sizeof(node)); int right = 0;
		for (int d = 1; d <= depth && ncur; d++) {
			size_t nn = 0;
			for (size_t i = 0; i < ncur; i++)
				for (int kk = 0; kk < NIN; kk++) {
					pop2_load(cur + i * sz); pop2_input in; parse(ins[kk], &in); pop2_frame(&in);
					if (Kid.alive >= 0 || Kid.f12 == 0 || pop2_level() != 13) continue;
					if (!right && Kid.room == 4 && Kid.curr_row == 1 && Kid.x >= 338 && Kid.frame == 15) { right = d; show("STANDING RIGHT OF THE GAP, depth", d); }
					if (Kid.room == 29) {
						show("BACK IN ROOM 29, depth", d);
						FILE *o = fopen("flame13.sol", "w"); int idx = (int)i; const char *seq[4096]; int n = 0; seq[n++] = ins[kk];
						for (int dd = d - 1; dd > 0; dd--) { seq[n++] = ins[tree[dd][idx].in]; idx = tree[dd][idx].parent; }
						while (n) fprintf(o, "%s\n", seq[--n]); fclose(o); return 0;
					}
					if (Kid.room != 4 || !seen_add(state_hash())) continue;
					pop2_save(nxt + nn * sz); tn[nn].parent = (int)i; tn[nn].in = (uint8_t)kk; sc[nn] = Kid.x - (Kid.y > 125 ? (Kid.y - 125) : 0); nn++;
				}
			int thr = -10000000;
			if (nn > cap) { int *tmp = malloc(nn * sizeof(int)); memcpy(tmp, sc, nn * sizeof(int)); int cmp(const void *a, const void *b) { return *(const int *)b - *(const int *)a; } qsort(tmp, nn, sizeof(int), cmp); thr = tmp[cap - 1]; free(tmp); }
			size_t kept = 0; tree[d] = malloc((cap + 1) * sizeof(node));
			for (size_t j = 0; j < nn && kept < cap; j++) if (sc[j] >= thr) { tree[d][kept] = tn[j]; memcpy(cur + kept++ * sz, nxt + j * sz, sz); }
			ncur = kept; if (d % 10 == 0) printf("depth %d: %zu states (of %zu)\n", d, ncur, nn);
		}
		printf("no way back found\n"); return 0;
	}
	if (!strcmp(v[5], "die")) {
		/* a beam search to his earliest death by the guard on the middle row (not burnt), then no input until the shadow
		 * has given him his life back, then a beam search back into room 29 */
		size_t cap = atoi(v[6]); int depth = atoi(v[7]); size_t sz = pop2_state_size(); seen = calloc(HS, 8); size_t room_ = 17 * cap;
		uint8_t *cur = malloc(cap * sz), *nxt = malloc(room_ * sz), *dead = malloc(sz); int *sc = malloc(room_ * sizeof(int));
		size_t ncur = 1; pop2_save(cur); seen_add(state_hash()); int dd = 0;
		for (int d = 1; d <= depth && ncur && !dd; d++) {
			size_t nn = 0;
			for (size_t i = 0; i < ncur && !dd; i++)
				for (int k = 0; k < NIN; k++) {
					pop2_load(cur + i * sz); pop2_input in; parse(ins[k], &in); pop2_frame(&in);
					if (pop2_level() != 13 || Kid.room != 4 || Kid.curr_row != 1 || Kid.f19 == 0xE6) continue;
					if (Kid.f12 == 0 || Kid.alive >= 0) { dd = d; pop2_save(dead); show("KILLED, depth", d); break; }
					if (!seen_add(state_hash())) continue;
					int g = (int8_t)ROOM_REC(4)->nchars > 0 ? chars[0].x : 352;
					pop2_save(nxt + nn * sz); sc[nn] = -1000 * Kid.f12 - abs(Kid.x - g); nn++;
				}
			if (dd) break;
			int thr = -10000000;
			if (nn > cap) { int *tmp = malloc(nn * sizeof(int)); memcpy(tmp, sc, nn * sizeof(int)); int cmp(const void *a, const void *b) { return *(const int *)b - *(const int *)a; } qsort(tmp, nn, sizeof(int), cmp); thr = tmp[cap - 1]; free(tmp); }
			size_t kept = 0; for (size_t j = 0; j < nn && kept < cap; j++) if (sc[j] >= thr) memcpy(cur + kept++ * sz, nxt + j * sz, sz);
			ncur = kept; if (d % 10 == 0) printf("depth %d: %zu states, best score %d\n", d, ncur, thr);
		}
		if (!dd) { printf("no death found\n"); return 0; }
		pop2_load(dead); int k = 0, pa = Kid.alive, pn = (int8_t)ROOM_REC(4)->nchars, pf = Kid.f24; unsigned pd = door(), pdc = word_5cdc; int p96 = (int16_t)word_0996;
		for (k = 1; k <= 4000; k++) {
			pop2_frame(&z); int nn = (int8_t)ROOM_REC(4)->nchars;
			if ((door() != pd && (pd == d0v(0) || door() == 0xFF)) || nn != pn || Kid.f24 != pf || (int16_t)word_0996 != p96 || word_5cdc != pdc || (Kid.alive != pa && (Kid.alive < 1 || Kid.alive > 7)) || k % 200 == 0) show("  after", k);
			pd = door(); pn = nn; pf = Kid.f24; p96 = (int16_t)word_0996; pdc = word_5cdc; pa = Kid.alive;
			if (Kid.alive < 0 && Kid.f24 == 0xD && Kid.f12 > 0) break;
			if (pop2_level() != 13) { show("  level left", k); return 0; }
		}
		show("ALIVE AGAIN, ticks after the death", k); int total = dd + k;
		memset(seen, 0, (size_t)HS * 8); ncur = 1; pop2_save(cur); seen_add(state_hash());
		for (int d = 1; d <= 400 && ncur; d++) {
			size_t nn = 0;
			for (size_t i = 0; i < ncur; i++)
				for (int kk = 0; kk < NIN; kk++) {
					pop2_load(cur + i * sz); pop2_input in; parse(ins[kk], &in); pop2_frame(&in);
					if (Kid.alive >= 0 || Kid.f12 == 0 || pop2_level() != 13) continue;
					if (Kid.room == 29) { show("BACK IN ROOM 29, ticks after the revival", d); printf("total %d\n", total + d); return 0; }
					if (Kid.room != 4 || !seen_add(state_hash())) continue;
					pop2_save(nxt + nn * sz); sc[nn] = Kid.x; nn++;
				}
			int thr = -10000000;
			if (nn > cap) { int *tmp = malloc(nn * sizeof(int)); memcpy(tmp, sc, nn * sizeof(int)); int cmp(const void *a, const void *b) { return *(const int *)b - *(const int *)a; } qsort(tmp, nn, sizeof(int), cmp); thr = tmp[cap - 1]; free(tmp); }
			size_t kept = 0; for (size_t j = 0; j < nn && kept < cap; j++) if (sc[j] >= thr) memcpy(cur + kept++ * sz, nxt + j * sz, sz);
			ncur = kept;
		}
		printf("no way back found\n"); return 0;
	}
	/* bfs: two classes (doors shut / opening), each kept to CAP states a depth by its score */
	size_t cap = atoi(v[6]); int depth = atoi(v[7]); size_t sz = pop2_state_size(); printf("state %zu bytes\n", sz);
	seen = calloc(HS, 8); size_t room_ = 6 * cap;
	uint8_t *cur = malloc(room_ * sz), *nxt = malloc(room_ * sz); int *sc = malloc(room_ * sizeof(int)), *cl = malloc(room_ * sizeof(int));
	node **tree = calloc(depth + 1, sizeof *tree); node *tn = malloc(room_ * sizeof(node));
	size_t ncur = 1; pop2_save(cur); seen_add(state_hash()); tree[0] = calloc(1, sizeof(node)); tree[0][0].parent = -1;
	int g1 = 0, g2 = 0, stop_shut = 1 << 30; unsigned d0 = door();
	for (int d = 1; d <= depth && ncur; d++) {
		size_t nn = 0, nopen = 0, nshut = 0;
		for (size_t i = 0; i < ncur; i++)
			for (int k = 0; k < NIN; k++) {
				pop2_load(cur + i * sz); int was_open = door() != d0; if (!was_open && d > stop_shut) break;
				pop2_input in; parse(ins[k], &in); pop2_frame(&in);
				if (Kid.alive >= 0 || Kid.f12 == 0 || pop2_level() != 13 || (Kid.room != 4 && Kid.room != 29)) continue;
				if (!seen_add(state_hash())) continue;
				int open = door() != d0;
				if (open && !g1) { g1 = d; stop_shut = d + 12; show("DOORS OPENING, alive, depth", d); }
				if (open && Kid.room == 29 && !g2) {
					g2 = d; show("BACK IN ROOM 29, depth", d);
					FILE *o = fopen("flame13.sol", "w"); int idx = (int)i; const char *seq[4096]; int n = 0; seq[n++] = ins[k];
					for (int dd = d - 1; dd > 0; dd--) { seq[n++] = ins[tree[dd][idx].in]; idx = tree[dd][idx].parent; }
					while (n) fprintf(o, "%s\n", seq[--n]); fclose(o); return 0;
				}
				if (nn >= room_) continue;
				pop2_save(nxt + nn * sz); tn[nn].parent = (int)i; tn[nn].in = (uint8_t)k; cl[nn] = open;
				sc[nn] = open ? (Kid.room == 29 ? 2000 : Kid.x) - (Kid.y > 125 ? 40 : 0) : -abs(Kid.x - 224) - (Kid.y > 125 ? 40 : 0);
				if (open) nopen++; else nshut++;
				nn++;
			}
		/* keep the best CAP of each class */
		size_t kept = 0; tree[d] = malloc((2 * cap + 1) * sizeof(node));
		for (int cls = 0; cls < 2; cls++) {
			size_t cnt = cls ? nopen : nshut; if (!cnt) continue;
			int thr = -100000;
			if (cnt > cap) { int *tmp = malloc(cnt * sizeof(int)); size_t m = 0; for (size_t j = 0; j < nn; j++) if (cl[j] == cls) tmp[m++] = sc[j];
				int cmp(const void *a, const void *b) { return *(const int *)b - *(const int *)a; } qsort(tmp, m, sizeof(int), cmp); thr = tmp[cap - 1]; free(tmp); }
			size_t k2 = 0;
			for (size_t j = 0; j < nn && k2 < cap; j++) if (cl[j] == cls && sc[j] >= thr) { if (kept != j) memcpy(cur + kept * sz, nxt + j * sz, sz); else memcpy(cur + kept * sz, nxt + j * sz, sz); tree[d][kept] = tn[j]; kept++; k2++; }
		}
		ncur = kept;
		if (d % 5 == 0 || g1 == d) printf("depth %d: %zu shut, %zu opening -> kept %zu\n", d, nshut, nopen, ncur);
	}
	printf("no result: g1 %d g2 %d\n", g1, g2);
}
