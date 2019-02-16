#include <yirl/game.h>
#include <yirl/menu.h>
#include <yirl/map.h>
#include <yirl/container.h>
#include <yirl/rect.h>
#include <yirl/events.h>
#include <yirl/text-screen.h>
#include <yirl/entity-script.h>

const static int NB_PIECES = 4;
static int otl;
const static int TETRIS_W = 16;

const static int LINE_MASK = 0xffff;

static int l_swap_mode;
static int t0_swap_mode;
static int t1_swap_mode;

enum {
	TETRIS_LINE,
	TETRIS_SQUARE,
	// ok now you can imagine the dev be like:
	// hummm how the fuck is that shape call
	TETRIS_THING0,
	TETRIS_THING1
};

void *initTetris(int nbArgs, void **args)
{
	Entity *mod = args[0];
	Entity *init;

	YEntityBlock {
		mod.score = 0;
	}
	init = yeCreateArray(NULL, NULL);
	yeCreateString("tetris-ascii", init, "name");
	yeCreateFunction("tetris_init", ygGetManager("tcc"), init, "callback");
	ywidAddSubType(init);
	return NULL;
}

static int merge_mask(Entity *masks, Entity *piece, int p, int px)
{
	for (int i = 0; i < yeLen(piece); ++i) {
		Entity *m = yeGet(masks, p - i);
		if (p - i < 0) {
			// lose
			printf("LOSE %d %d !\n", p, i);
			return 1;
		}
		printf("%x - %x - %x\n", yeGetInt(m), yeGetIntAt(piece, i),
		       yeGetInt(m) | yeGetIntAt(piece, i));
		yeSetInt(m, yeGetInt(m) | (yeGetIntAt(piece, i) << px));
	}
	return 0;
}

static void gen_piece(Entity *tetris, Entity *cp, Entity *piece, int p)
{
	if (p < 0)
		yeSetInt(cp, yuiRand() % NB_PIECES);
	else
		yeSetInt(cp, p);
	piece = yeGet(yeGet(tetris, "pieces"), yeGetInt(cp));
	int l = 0;
	YE_ARRAY_FOREACH(piece, p_part) {
		uint64_t cl = YUI_COUNT_1_BIT(yeGetInt(p_part));
		if (cl > l)
			l = cl;
	}
	int px = TETRIS_W / 2 - l / 2;

       	YEntityBlock {
		tetris.ppx = px;
		tetris.ppy = 0;
		tetris.pl = l;
	}
}


void *reset_otl(int nbArgs, void **args)
{
	ywTurnLengthOverwrite = otl;
}

void *tetris_action(int nbArgs, void **args)
{
	Entity *tetris = args[0];
	Entity *events = args[1];
	Entity *ppy = yeGet(tetris, "ppy");
	Entity *ppx = yeGet(tetris, "ppx");
	Entity *cp = yeGet(tetris, "cp");
	Entity *piece = yeGet(yeGet(tetris, "pieces"), yeGetInt(cp));
	Entity *masks = yeGet(tetris, "masks");
	int ret = ACTION;

	if (yevIsKeyDown(events, Y_ESC_KEY)) {
		if (yeGet(tetris, "quit"))
			yesCall(yeGet(tetris, "quit"), tetris);
		else
			ygTerminate();
		return (void *)ACTION;
	}

	if (yevCheckKeys(events, YKEY_DOWN, Y_DOWN_KEY, 's')) {
		ywTurnLengthOverwrite = yeGetIntAt(tetris, "turn-length") / 2;
	}
	if (yevCheckKeys(events, YKEY_UP, Y_DOWN_KEY, 's')) {
		ywTurnLengthOverwrite = yeGetIntAt(tetris, "turn-length");
	}

	if (yevCheckKeys(events, YKEY_DOWN, Y_LEFT_KEY, 'a')) {
		if (yeGetInt(ppx) > 0)
			yeAddInt(ppx, -1);
	} else if (yevCheckKeys(events, YKEY_DOWN, Y_RIGHT_KEY, 'd')) {
		if (yeGetInt(ppx) < TETRIS_W - yeGetIntAt(tetris, "pl"))
			yeAddInt(ppx, +1);
	} else if (yevCheckKeys(events, YKEY_DOWN, Y_UP_KEY, 'w')) {
		if (yeGetInt(cp) == TETRIS_LINE) {
			++l_swap_mode;
			YEntityBlock {
				if (l_swap_mode & 1) {
					piece = [0xf];
				} else {
					piece = [1, 1, 1, 1];
				}
			}
		} else if (yeGetInt(cp) == TETRIS_THING1) {
			++t1_swap_mode;
			int t1_t = t1_swap_mode & 3;

			if (t1_t == 1) {
				yeSetAt(piece, 0, 7);
				yeSetAt(piece, 1, 4);
				yePopBack(piece);
			} else if (t1_t == 2) {
				yeSetAt(piece, 0, 4);
				yeCreateInt(6, piece, NULL);
			} else if (t1_t == 3) {
				yeSetAt(piece, 0, 0);
				yeSetAt(piece, 1, 4);
				yeSetAt(piece, 2, 7);
			} else {
				yeSetAt(piece, 0, 3);
				yeSetAt(piece, 1, 1);
				yeSetAt(piece, 2, 1);
			}
		} else if (yeGetInt(cp) == TETRIS_THING0) {
			int t0_t;

			++t0_swap_mode;
			t0_t = t0_swap_mode & 3;
			YEntityBlock {
				if (t0_t == 1) {
					piece = [1, 3, 1];
				} else if (t0_t == 2) {
					piece = [7, 2];
				} else if (t0_t == 3) {
					piece = [4, 6, 4];
				} else {
					piece = [2, 7];
				}
			}
		}
	}

	yeAddInt(ppy, 1);
	yeAddInt(ygGet("tetris-ascii.score"), 1);
	ywTextScreenReformat();
	printf("%d - %d %x | %d %d OTL: %d\n", yeGetInt(ppy), yeGetInt(cp),
	       yeGetIntAt(piece, 0), yeGetIntAt(tetris, "w"),
	       yeGetInt(ppx), ywTurnLengthOverwrite);

	int py = yeGetInt(ppy);
	int px = yeGetInt(ppx);
	int need_gen = 0;

	for (int i = py; i > 0 && i > py - yeLen(piece); --i) {
		int piece_on_y = py >= i && py < i + yeLen(piece);
		int p_mask = yeGetIntAt(piece, py - i);
		int m = yeGetIntAt(masks, i);

		if (piece_on_y && m & p_mask << px) {
			printf("COL !!!! %d %d %d\n", i, i - py,
				i + (i - py) - 1);
			if (merge_mask(masks, piece, i + (py - i) - 1, px)) {
				if (yeGet(tetris, "quit"))
					yesCall(yeGet(tetris, "die"), tetris);
				else
					ygTerminate();
				return (void *)ret;
			}
			need_gen = 1;
			break;
		}
	}

	int txt_treshold = 2;

	for (int i = 0; i < yeLen(masks) - 1; ++i) {
		Entity *txt = yeGet(yeGet(tetris, "text"), i + txt_treshold);
		int piece_on_y = py >= i && py < i + yeLen(piece);
		int p_mask = yeGetIntAt(piece, py - i);
		int m = yeGetIntAt(masks, i);

		if (piece_on_y)
			printf("piece_on_y p: %d\n", py - i);

		for (int j = 0; j < TETRIS_W; ++j) {
			if (piece_on_y && p_mask << px & 1 << j) {
				yeStringReplaceCharAt(txt, '#', j + 1);
			} else if (m & (1 << j)) {
				yeStringReplaceCharAt(txt, '&', j + 1);
			} else {
				yeStringReplaceCharAt(txt, ' ', j + 1);
			}
		}
	}

	for (int i = 0; i < yeLen(masks) - 1; ++i) {
		int m = yeGetIntAt(masks, i);
		if (!(LINE_MASK ^ m)) {
			yeSetAt(masks, i, 0);
			for (int j = i; j > 0; --j) {
				yeSwapElems(masks, yeGet(masks, j),
					yeGet(masks, j - 1));
			}
		}
	}

	if (need_gen)
		gen_piece(tetris, cp, piece, -1);
	return (void *)ret;
}

void *tetris_init(int nbArgs, void **args)
{
	Entity *tetris = args[0];

	YEntityBlock {
		tetris.text = {
		0:	    "score: {tetris-ascii.score}",
		1:      "|----------------|",
		2-20 :  "|................|",
		21 :    "|________________|"
		};
		tetris.masks = { 0-18: 0, 18: 0xffffff };
		tetris.text_l = 16;
		tetris["text-align"] = "center";
		tetris["turn-length"] = 300000;
		tetris.pieces = [
			[0x1, 0x1, 0x1, 0x1],
			[0x3, 0x3],
			[2, 7],
			[3, 1, 1]
			];
		tetris.cp = 10;
		tetris.ppy = 0;
		tetris.ppx = 0;
		tetris.w = TETRIS_W;
		tetris.background = "rgba: 255 255 255 255";
		tetris.fmt = "yirl";
		tetris.action = tetris_action;
		tetris.destroy = reset_otl;
	}
	yuiRandInit();
	l_swap_mode = 0;
	t0_swap_mode = 0;
	t1_swap_mode = 0;
	gen_piece(tetris, yeGet(tetris, "cp"), yeGet(tetris, "piece"), 2);
	printf("txt: %p - %p - %d\nta: %s - %d 0x%x\n", tetris, yeGet(tetris, "text"),
	       yeGetIntAt(tetris, "score"),
	       yeGetStringAt(tetris, "text-align"),
	       yeLen(yeGet(tetris, "pieces")),
	       yeGetIntAt(yeGet(yeGet(tetris, "pieces"), 0), 0));

	otl = ywTurnLengthOverwrite;
	void *ret = ywidNewWidget(tetris, "text-screen");
	printf("%d\n", tetris->refCount);
	return ret;
}
