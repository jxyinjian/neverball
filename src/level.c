/*   
 * Copyright (C) 2003 Robert Kooima
 *
 * NEVERBALL is  free software; you can redistribute  it and/or modify
 * it under the  terms of the GNU General  Public License as published
 * by the Free  Software Foundation; either version 2  of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT  ANY  WARRANTY;  without   even  the  implied  warranty  of
 * MERCHANTABILITY or  FITNESS FOR A PARTICULAR PURPOSE.   See the GNU
 * General Public License for more details.
 */

#include <stdio.h>

#include "level.h"
#include "game.h"
#include "geom.h"
#include "back.h"
#include "audio.h"

/*---------------------------------------------------------------------------*/

#define MAXSTR 256
#define MAXLVL 32

struct level
{
    char file[MAXSTR];
    char back[MAXSTR];
    int  time;
};

static int coins;                       /* Current score                */
static int balls;                       /* Current life count           */
static int level;                       /* Current level number         */
static int count;                       /* Number of levels             */

static struct level level_v[MAXLVL];

/*---------------------------------------------------------------------------*/

void level_init(void)
{
    FILE *fin = fopen(LEVEL_FILE, "r");

    count = 0;
    level = 0;
    coins = 0;
    balls = 2;

    if (fin)
    {
        while (fscanf(fin, "%s %s %d",
                       level_v[count].file,
                       level_v[count].back,
                      &level_v[count].time) == 3)
            count++;

        fclose(fin);
    }
}

void level_free(void)
{
    game_free();
    back_free();

    count = 0;
}

int level_exists(int i)
{
    return (0 < i && i < count);
}

/*---------------------------------------------------------------------------*/

int curr_level(void) { return level; }
int curr_balls(void) { return balls; }
int curr_coins(void) { return coins; }

/*---------------------------------------------------------------------------*/

void level_goto(int i)
{
    level = i;

    back_init(level_v[level].back);
    game_init(level_v[level].file,
              level_v[level].time);
}

int level_pass(void)
{
    if (++level < count)
    {
        game_free();
        back_free();

        back_init(level_v[level].back);
        game_init(level_v[level].file,
                  level_v[level].time);

        return 1;
    }
    return 0;
}

int level_fail(void)
{
    if (--balls >= 0)
    {
        game_free();
        back_free();

        back_init(level_v[level].back);
        game_init(level_v[level].file,
                  level_v[level].time);

        return 1;
    }
    return 0;
}

/*---------------------------------------------------------------------------*/

void level_score(int n)
{
    coins += n;

    if (coins >= 100)
    {
        coins -= 100;
        balls += 1;
        audio_play(AUD_BALL, 1.0f);
    }
    else
        audio_play(AUD_COIN, 1.0f);
}
