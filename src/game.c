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

#include <SDL.h>
#include <math.h>

#include "gl.h"
#include "game.h"
#include "vec3.h"
#include "geom.h"
#include "back.h"
#include "part.h"
#include "image.h"
#include "audio.h"
#include "solid.h"
#include "level.h"
#include "config.h"

/*---------------------------------------------------------------------------*/

static struct s_file file;

static double time  = 0.0;              /* Simulation time                   */
static int    clock = 0;                /* Clock time                        */

static double game_ix;                  /* Input rotation about X axis       */
static double game_iz;                  /* Input rotation about Z axis       */
static double game_rx;                  /* Floor rotation about X axis       */
static double game_rz;                  /* Floor rotation about Z axis       */

static double view_r;                   /* Ideal view rotation about Y axis  */
static double view_dy;                  /* Ideal view distance above ball    */
static double view_dz;                  /* Ideal view distance behind ball   */

static double view_p[3];                /* Current view position             */
static double view_e[3][3];             /* Current view orientation          */

static GLuint shadow_text;              /* Shadow texture object             */

/*---------------------------------------------------------------------------*/

static void view_init(void)
{
    view_r  = 0.0;
    view_dy = 4.0;
    view_dz = 6.0;

    view_p[0] =     0.0;
    view_p[1] = view_dy;
    view_p[2] = view_dz;

    view_e[0][0] = 1.0;
    view_e[0][1] = 0.0;
    view_e[0][2] = 0.0;
    view_e[1][0] = 0.0;
    view_e[1][1] = 1.0;
    view_e[1][2] = 0.0;
    view_e[2][0] = 0.0;
    view_e[2][1] = 0.0;
    view_e[2][2] = 1.0;
}

void game_init(const char *s, int t)
{
    game_ix = 0.0;
    game_iz = 0.0;
    game_rx = 0.0;
    game_rz = 0.0;

    view_init();
    part_init(GOAL_HEIGHT);

    sol_load(&file, s, config_text());

    clock = t;
    time  = t;

    shadow_text = make_image_from_file(NULL, NULL, IMG_SHADOW);
}

void game_free(void)
{
    if (glIsTexture(shadow_text))
        glDeleteTextures(1, &shadow_text);

    sol_free(&file);
    part_free();
}

/*---------------------------------------------------------------------------*/

int curr_clock(void)
{
    return clock;
}

char *curr_intro(void)
{
    return file.av;
}

/*---------------------------------------------------------------------------*/

static void game_draw_balls(const struct s_file *fp)
{
    double M[16];
    int ui;

    for (ui = 0; ui < fp->uc; ui++)
    {
        m_basis(M, fp->uv[ui].e[0], fp->uv[ui].e[1], fp->uv[ui].e[2]);

        glPushMatrix();
        {
            glTranslated(fp->uv[ui].p[0],
                         fp->uv[ui].p[1],
                         fp->uv[ui].p[2]);
            glMultMatrixd(M);
            glScaled(fp->uv[ui].r,
                     fp->uv[ui].r,
                     fp->uv[ui].r);

            ball_draw();
        }
        glPopMatrix();
    }
}

static void game_draw_coins(const struct s_file *fp)
{
    int ci;

    for (ci = 0; ci < fp->cc; ci++)
        if (fp->cv[ci].n > 0)
        {
            glPushMatrix();
            {
                glTranslated(fp->cv[ci].p[0],
                             fp->cv[ci].p[1],
                             fp->cv[ci].p[2]);
                coin_draw(fp->cv[ci].n);
            }
            glPopMatrix();
        }
}

static void game_draw_goals(const struct s_file *fp, double rx, double ry)
{
    int zi;

    for (zi = 0; zi < fp->zc; zi++)
    {
        glPushMatrix();
        {
            glTranslated(fp->zv[zi].p[0],
                         fp->zv[zi].p[1],
                         fp->zv[zi].p[2]);

            part_draw_goal(rx, ry);

            glScaled(fp->zv[zi].r, 1.0, fp->zv[zi].r);
            goal_draw();
        }
        glPopMatrix();
    }
}

/*---------------------------------------------------------------------------*/

/*
 * A note about lighting and shadow: technically speaking, it's wrong.
 * The  light  position  and   shadow  projection  behave  as  if  the
 * light-source rotates with the  floor.  However, the skybox does not
 * rotate, thus the light should also remain stationary.
 *
 * The  correct behavior  would eliminate  a significant  3D  cue: the
 * shadow of  the ball indicates  the ball's position relative  to the
 * floor even  when the ball is  in the air.  This  was the motivating
 * idea  behind the  shadow  in  the first  place,  so correct  shadow
 * projection would only magnify the problem.
 */

#ifdef GL_ARB_multitexture

static void game_set_shadow(const struct s_file *fp)
{
    const double *ball_p = fp->uv->p;
    const double  ball_r = fp->uv->r;

    glActiveTextureARB(GL_TEXTURE1_ARB);
    glMatrixMode(GL_TEXTURE);
    {
        double k = 0.5 / ball_r;

        glEnable(GL_TEXTURE_2D);

        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glBindTexture(GL_TEXTURE_2D, shadow_text);

        glLoadIdentity();
        glTranslated(0.5 - ball_p[0] * k,
                     0.5 - ball_p[2] * k, 0.0);
        glScaled(k, k, 1.0);
    }
    glMatrixMode(GL_MODELVIEW);
    glActiveTextureARB(GL_TEXTURE0_ARB);
}

static void game_clr_shadow(void)
{
    glActiveTextureARB(GL_TEXTURE1_ARB);
    {
        glDisable(GL_TEXTURE_2D);
    }
    glActiveTextureARB(GL_TEXTURE0_ARB);
}

#endif /* GL_ARB_multitexture */

/*---------------------------------------------------------------------------*/

void game_draw(void)
{
    const float light_p[4] = { 8.0, 32.0, 8.0, 1.0 };

    const struct s_file *fp = &file;
    const double *ball_p = file.uv->p;
    
    config_push_persp(FOV, 0.1, 1000.0);
    glPushAttrib(GL_LIGHTING_BIT);
    glPushMatrix();
    {
        double v[3], rx, ry;

        v_sub(v, ball_p, view_p);

        rx = V_DEG(atan2(-v[1], sqrt(v[0] * v[0] + v[2] * v[2])));
        ry = V_DEG(atan2(+v[0], -v[2]));

        glTranslated(0.0, 0.0, -v_len(v));
        glRotated(rx, 1.0, 0.0, 0.0);
        glRotated(ry, 0.0, 1.0, 0.0);
        glTranslated(-ball_p[0], -ball_p[1], -ball_p[2]);

        /* Center the skybox about the position of the camera. */

        glPushMatrix();
        {
            glTranslated(view_p[0], view_p[1], view_p[2]);
            back_draw();
        }
        glPopMatrix();

        /* Rotate the environment about the position of the ball. */

        glTranslated(+ball_p[0], +ball_p[1], +ball_p[2]);
        glRotated(-game_rz, view_e[2][0], view_e[2][1], view_e[2][2]);
        glRotated(-game_rx, view_e[0][0], view_e[0][1], view_e[0][2]);
        glTranslated(-ball_p[0], -ball_p[1], -ball_p[2]);

        /* Configure the lighting. */

        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        glLightfv(GL_LIGHT0, GL_POSITION, light_p);

        /* Draw the floor. */

#ifdef GL_ARB_multitexture
        game_set_shadow(fp);
        sol_draw(fp);
        game_clr_shadow();
#else
        sol_draw(fp);
#endif

        /* Draw the game elements. */

        game_draw_coins(fp);
        game_draw_balls(fp);
        game_draw_goals(fp, -rx, -ry);
        part_draw_coin(-rx, -ry);
    }
    glPopMatrix();
    glPopAttrib();
    config_pop_matrix();
}

/*---------------------------------------------------------------------------*/

static void game_update_grav(double h[3], const double g[3])
{
    struct s_file *fp = &file;

    double x[3];
    double y[3] = { 0.0, 1.0, 0.0 };
    double z[3];
    double X[16];
    double Z[16];
    double M[16];

    /* Compute the gravity vector from the given world rotations. */

    v_sub(z, view_p, fp->uv->p);
    v_crs(x, y, z);
    v_crs(z, x, y);
    v_nrm(x, x);
    v_nrm(z, z);

    m_rot (Z, z, V_RAD(game_rz));
    m_rot (X, x, V_RAD(game_rx));
    m_mult(M, Z, X);
    m_vxfm(h, M, g);
}

static void game_update_view(double dt)
{
    double *ball_p = file.uv->p;

    double d[3];
    double dy;
    double dz;

    /* Orthonormalize the basis of the view of the ball in its new position. */

    v_sub(view_e[2], view_p, ball_p);

    v_crs(view_e[0], view_e[1], view_e[2]);
    v_crs(view_e[2], view_e[0], view_e[1]);
    v_nrm(view_e[0], view_e[0]);
    v_nrm(view_e[2], view_e[2]);

    /* The current view (dy, dz) approaches the ideal (view_dy, view_dz). */

    v_sub(d, view_p, ball_p);

    dy = v_dot(view_e[1], d);
    dz = v_dot(view_e[2], d);

    dy += (view_dy - dy) * dt * 2.0;
    dz += (view_dz - dz) * dt * 2.0;

    /* Compute the new view position. */

    view_p[0] = view_p[1] = view_p[2] = 0.0;

    v_mad(view_p, ball_p, view_e[0], view_r * dt);
    v_mad(view_p, view_p, view_e[1], dy);
    v_mad(view_p, view_p, view_e[2], dz);
}

static void game_update_time(double dt)
{
    int seconds;

   /* The ticking clock. */

    time -= dt;
    seconds = (int) ceil(time);

    if (0 <= seconds && seconds < clock && clock < 99)
    {
        audio_play(AUD_TICK, 1.f);
        clock = seconds;
    }
}

static int game_update_state(void)
{
    struct s_file *fp = &file;
    double p[3];
    double c[3];
    int n;

    /* Test for a coin grab and a possible 1UP. */

    if ((n = sol_coin_test(fp, p, COIN_RADIUS)) > 0)
    {
        coin_color(c, n);
        part_burst(p, c);
        level_score(n);
    }

    /* Test for a goal. */

    if (sol_goal_test(fp, p))
        return GAME_GOAL;

    /* Test for time-out. */

    if (clock <= 0)
        return GAME_TIME;

    /* Test for fall-out. */

    if (fp->uv[0].p[1] < -10.0)
        return GAME_FALL;

    return GAME_NONE;
}

/*
 * On  most  hardware, rendering  requires  much  more  computing power  than
 * physics.  Since  physics takes less time  than graphics, it  make sense to
 * detach  the physics update  time step  from the  graphics frame  rate.  By
 * performing multiple physics updates for  each graphics update, we get away
 * with higher quality physics with little impact on overall performance.
 *
 * Toward this  end, we establish a  baseline maximum physics  time step.  If
 * the measured  frame time  exceeds this  maximum, we cut  the time  step in
 * half, and  do two updates.  If THIS  time step exceeds the  maximum, we do
 * four updates.  And  so on.  In this way, the physics  system is allowed to
 * seek an optimal update rate independant of, yet in integral sync with, the
 * graphics frame rate.
 */

int game_step(const double g[3], double dt)
{
    struct s_file *fp = &file;

    double h[3];
    double d = 0.0;
    double b = 0.0;
    double t = dt;
    int i, n = 1;

    /* Smooth jittery or discontinuous input. */

    game_rx += (game_ix - game_rx) * dt / RESPONSE;
    game_rz += (game_iz - game_rz) * dt / RESPONSE;

    game_update_grav(h, g);
    part_step(h, dt);

    /* Run the sim. */

    while (t > MAX_DT && n < MAX_DN)
    {
        t /= 2;
        n *= 2;
    }

    for (i = 0; i < n; i++)
        if (b < (d = sol_step(fp, h, t)))
            b = d;

    /* Mix the sound of a ball bounce. */

    if (b > 0.5)
        audio_play(AUD_BUMP, (float) (b - 0.5) * 2.0f);

    game_update_view(dt);
    game_update_time(dt);

    return game_update_state();
}

/*---------------------------------------------------------------------------*/

void game_set_x(int k)
{
    game_ix = -20.0 * k / JOY_MAX;
}

void game_set_z(int k)
{
    game_iz = +20.0 * k / JOY_MAX;
}

void game_set_pos(int x, int y)
{
    double bound = 20.0;

    game_ix += 40.0 * y / config_h();
    game_iz += 40.0 * x / config_h();

    if (game_ix > +bound) game_ix = +bound;
    if (game_ix < -bound) game_ix = -bound;
    if (game_iz > +bound) game_iz = +bound;
    if (game_iz < -bound) game_iz = -bound;
}

void game_set_rot(int r)
{
    view_r = r * 5.0;
}

/*---------------------------------------------------------------------------*/

void game_set_fly(double k)
{
    struct s_file *fp = &file;

    if (fp->pc > 0)
    {
        double p[3];
        double q[3];
        double v[3];

        p[0] = p[1] = p[2] = 0.0;

        if (fp->uc > 0) v_cpy(p, fp->uv[0].p);

        v_mad(p, p, view_e[1], view_dy);
        v_mad(p, p, view_e[2], view_dz);

        if (k >= 0.0)
        {
            q[0] = +fp->pv[0].p[0];
            q[1] = +fp->pv[0].p[1];
            q[2] = +fp->pv[0].p[2];
        }
        else
        {
            q[0] = -fp->pv[0].p[0];
            q[1] = +fp->pv[0].p[1];
            q[2] = +fp->pv[0].p[2];
        }

        v_sub(v, q, p);
        v_mad(view_p, p, v, +(k * k));
    }
}

/*---------------------------------------------------------------------------*/

