/* snd GAMEDIR SEED CHAIN.sol RUN.sol [FROM]: the sound model along a replay (the level ends wait for the level-end music,
 * a MIDI piece: whatever else starts on the MIDI channel ends that wait). First the table of the game's sounds (number,
 * kind: D digital / M MIDI, length in ticks of 1/12 s); then, from RUN's tick FROM on, every tick with what was queued
 * and started (the core's sound_debug lines, stderr merged) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "/home/jaffar/jaffarPlus-run/extern/quickerSDLPoP2/extern/SDLPoP2/source/core.h"
#include "/home/jaffar/jaffarPlus-run/extern/quickerSDLPoP2/extern/SDLPoP2/source/types.h"
#include "/home/jaffar/jaffarPlus-run/extern/quickerSDLPoP2/extern/SDLPoP2/source/globals.h"
#include "/home/jaffar/jaffarPlus-run/extern/quickerSDLPoP2/extern/SDLPoP2/source/dat.h"
extern int sound_debug; int snd_len_of(int n); int snd_ch_id(int k); int snd_ch_left(int k);
extern uint16_t word_0882, word_0884;
static int parse(const char *s, pop2_input *in)
{
	memset(in, 0, sizeof *in); if (strlen(s) < 10 || s[0] != (char)124) return 0;
	if (s[1] == 82) in->restart_level = 1; else if (s[1] == 75) in->keystroke = 1;
	in->x = s[3] == 76 ? -1 : s[4] == 82 ? 1 : 0; in->y = s[5] == 85 ? -1 : s[6] == 68 ? 1 : 0;
	in->shift = s[8] == 67 ? 2 : s[7] == 83 ? 1 : 0; return 1;
}
int main(int c, char **v)
{
	pop2_set_game_version(0); pop2_set_skip_scenes(1); if (!pop2_init(v[1])) return 1; pop2_new_game(1, strtoul(v[2], 0, 0));
	if (c < 4) {
		char p[600]; dat_file dm, dd; snprintf(p, sizeof p, "%s/MIDISND.DAT", v[1]); int om = dat_open(&dm, p);
		snprintf(p, sizeof p, "%s/DIGISND.DAT", v[1]); int od = dat_open(&dd, p);
		for (int i = 0; i < 0x200; i++) { uint16_t n; int m = om && dat_find(&dm, "DNS", 10000 + i, &n), d = od && dat_find(&dd, "DNS", 10000 + i, &n);
			if (m || d) printf("%3X %s%s %7.1f\n", i, m ? "M" : "-", d ? "D" : "-", snd_len_of(i) / 83335.0); }
		return 0;
	}
	FILE *fp = fopen(v[3], "r"); char line[64]; pop2_input in; while (fgets(line, sizeof line, fp)) if (parse(line, &in)) pop2_frame(&in); fclose(fp);
	int from = c > 5 ? atoi(v[5]) : 1; dup2(1, 2); setvbuf(stdout, 0, _IONBF, 0);
	fp = fopen(v[4], "r"); int t = 0;
	int killat = getenv("KILLAT") ? atoi(getenv("KILLAT")) : 0;   /* KILLAT=tick: the prince's hit points taken before that tick */
	while (fgets(line, sizeof line, fp)) if (parse(line, &in)) { t++; if (t >= from) sound_debug = 3; if (t == killat) Kid.f12 = 0; pop2_frame(&in);
		if (t >= from) printf("%d L%d room %d x %d frame %d hp %d alive %d | effect %X music %X | ch0 %X %.1f ch1 %X %.1f\n", t, pop2_level(), Kid.room, Kid.x, Kid.frame, Kid.f12, (int8_t)Kid.alive,
			word_0882, word_0884, snd_ch_id(0), snd_ch_left(0) / 83335.0, snd_ch_id(1), snd_ch_left(1) / 83335.0); }
}
