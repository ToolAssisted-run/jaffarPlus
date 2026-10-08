/* roomdump GAMEDIR SEED CHAIN.sol RUN.sol TICK ROOM [ROOM...]: the rooms' tiles (hex) and modifiers after RUN's tick TICK, and the level's room links */
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
int main(int c, char **v)
{
	pop2_set_game_version(0); pop2_set_skip_scenes(1); if (!pop2_init(v[1])) return 1; pop2_new_game(1, strtoul(v[2], 0, 0));
	FILE *fp = fopen(v[3], "r"); char line[64]; pop2_input in; while (fgets(line, sizeof line, fp)) if (parse(line, &in)) pop2_frame(&in); fclose(fp);
	fp = fopen(v[4], "r"); int t = 0, upto = atoi(v[5]);
	while (t < upto && fgets(line, sizeof line, fp)) if (parse(line, &in)) { pop2_frame(&in); t++; }
	printf("level %d tick %d: kid room %d x %d y %d row %d col %d frame %d\n", pop2_level(), t, Kid.room, Kid.x, Kid.y, Kid.curr_row, Kid.curr_col, Kid.frame);
	for (int a = 6; a < c; a++) { int r = atoi(v[a]); const uint8_t *tl = ROOM_TILES(r);
		printf("room %d\n", r);
		for (int row = 0; row < 3; row++) { for (int col = 0; col < 10; col++) printf(" %02X:%04X", tl[row * 10 + col], (uint16_t)ROOM_ATTRS(r)[row * 10 + col]); printf("\n"); } }
}
