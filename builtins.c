/*
 * builtins.c
 *
 * MathMap
 *
 * Copyright (C) 1997-2002 Mark Probst
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#ifdef GIMP
#include <libgimp/gimp.h>
#endif

#include "builtins.h"
#include "postfix.h"
#include "tags.h"
#include "overload.h"
#include "mathmap.h"

#ifdef GIMP
extern int previewing;
extern gint preview_width, preview_height;
extern guchar *fast_image_source;
extern gint sel_x1, sel_y1, sel_width, sel_height;
#endif

builtin *firstBuiltin = 0;

static void
get_pixel (mathmap_invocation_t *invocation, int x, int y, guchar *pixel, int drawable_index, int frame)
{ 
    if (invocation->edge_behaviour == EDGE_BEHAVIOUR_WRAP)
    {
	if (x < 0)
	    x = x % invocation->img_width + invocation->img_width;
	else if (x >= invocation->img_width)
	    x %= invocation->img_width;
	if (y < 0)
	    y = y % invocation->img_height + invocation->img_height;
	else if (y >= invocation->img_height)
	    y %= invocation->img_height;
    }
    else if (invocation->edge_behaviour == EDGE_BEHAVIOUR_REFLECT)
    {
	if (x < 0)
	    x = -x % invocation->img_width;
	else if (x >= invocation->img_width)
	    x = (invocation->img_width - 1) - (x % invocation->img_width);
	if (y < 0)
	    y = -y % invocation->img_height;
	else if (y >= invocation->img_height)
	    y = (invocation->img_height - 1) - (y % invocation->img_height);
    }

#ifdef GIMP
    if (previewing)
    {
	x = (x - sel_x1) * preview_width / sel_width;
	y = (y - sel_y1) * preview_height / sel_height;

	mathmap_get_fast_pixel(invocation, drawable_index, x, y, pixel);
    }
    else
#endif
	mathmap_get_pixel(invocation, drawable_index, frame, x, y, pixel);
}

void
get_orig_val_pixel (mathmap_invocation_t *invocation, float x, float y, unsigned char *pixel, int drawable_index, int frame)
{
    x += invocation->middle_x + invocation->origin_x;
    y = -y + invocation->middle_y + invocation->origin_y;

    if (!invocation->supersampling)
    {
	x += 0.5;
	y += 0.5;
    }

    get_pixel(invocation, floor(x), floor(y), pixel, drawable_index, frame);
}

void
get_orig_val_intersample_pixel (mathmap_invocation_t *invocation, float x, float y, unsigned char *pixel, int drawable_index, int frame)
{
    int x1,
	x2,
	y1,
	y2;
    float x2fact,
	y2fact,
	x1fact,
	y1fact,
	p1fact,
	p2fact,
	p3fact,
	p4fact;
    unsigned char pixel1a[4],
	pixel2a[4],
	pixel3a[4],
	pixel4a[4];
    unsigned char *pixel1 = pixel1a,
	*pixel2 = pixel2a,
	*pixel3 = pixel3a,
	*pixel4 = pixel4a;
    int i;

    x += invocation->middle_x + invocation->origin_x;
    y = -y + invocation->middle_y + invocation->origin_y;

    x1 = floor(x);
    x2 = x1 + 1;
    y1 = floor(y);
    y2 = y1 + 1;
    x2fact = (x - x1);
    y2fact = (y - y1);
    x1fact = 1.0 - x2fact;
    y1fact = 1.0 - y2fact;
    p1fact = x1fact * y1fact;
    p2fact = x1fact * y2fact;
    p3fact = x2fact * y1fact;
    p4fact = x2fact * y2fact;

    get_pixel(invocation, x1, y1, pixel1, drawable_index, frame);
    get_pixel(invocation, x1, y2, pixel2, drawable_index, frame);
    get_pixel(invocation, x2, y1, pixel3, drawable_index, frame);
    get_pixel(invocation, x2, y2, pixel4, drawable_index, frame);

    for (i = 0; i < 4; ++i)
	pixel[i] = pixel1[i] * p1fact
	    + pixel2[i] * p2fact
	    + pixel3[i] * p3fact
	    + pixel4[i] * p4fact;
}

#define MAX_LINEAR_DIM       10
#define MAT(r,c)             (a[exch[r] * dim + (c)])
#define RHS(r)               (b[exch[r]])

void
solve_linear_equations (int dim, float *a, float *b)
{
    float r[MAX_LINEAR_DIM];
    int exch[MAX_LINEAR_DIM];
    int i;

    assert(dim <= MAX_LINEAR_DIM);

    for (i = 0; i < dim; ++i)
	exch[i] = i;

    for (i = 0; i < dim - 1; ++i)
    {
	int p;

	for (p = i; p < dim; ++p) /* find pivot element */
	    if (MAT(p, i) != 0.0)
		break;

	if (p != dim)
	{
	    int j;

	    if (p != i)
	    {
		int tmp;

		tmp = exch[p];
		exch[p] = exch[i];
		exch[i] = tmp;
	    }

	    for (j = i + 1; j < dim; ++j)
	    {
		if (MAT(j, i) != 0.0)
		{
		    float f = MAT(i, i) / MAT(j, i);
		    int k;

		    MAT(j, i) = 0.0;
		    for (k = i + 1; k < dim; ++k)
			MAT(j, k) = MAT(j, k) * f - MAT(i, k);

		    RHS(j) = RHS(j) * f - RHS(i);
		}
	    }
	}
    }

    for (i = dim - 1; i >= 0; --i)
    {
	if (MAT(i, i) == 0.0)
	    RHS(i) = 0.0;	/* this should be an error condition */
	else
	{
	    int j;
	    float v = 0.0;

	    for (j = i + 1; j < dim; ++j)
		v += MAT(i, j) * r[j];

	    r[i] = (RHS(i) - v) / MAT(i, i);
	}
    }

    for (i = 0; i < dim; ++i)
	b[i] = r[i];
}

void
convert_rgb_to_hsv (float *rgb, float *hsv)
{
    float max, min;
    int i;

    for (i = 0; i < 3; ++i)
	if (rgb[i] < 0.0)
	    rgb[i] = 0.0;
	else if (rgb[i] > 1.0)
	    rgb[i] = 1.0;

    max = MAX(rgb[0], MAX(rgb[1], rgb[2]));
    min = MIN(rgb[0], MIN(rgb[1], rgb[2]));
    hsv[2] = max;

    if (max != 0)
	hsv[1] = (max - min) / max;
    else
	hsv[1] = 0.0;

    if (hsv[1] == 0.0)
	hsv[0] = 0.0;		/* actually undefined */
    else
    {
	float delta = max - min;

	if (rgb[0] == max)
	    hsv[0] = (rgb[1] - rgb[2]) / delta;
	else if (rgb[1] == max)
	    hsv[0] = 2 + (rgb[2] - rgb[0]) / delta;
	else
	    hsv[0] = 4 + (rgb[0] - rgb[1]) / delta;

	hsv[0] /= 6.0;

	if (hsv[0] < 0.0)
	    hsv[0] += 1.0;
    }
}

void
convert_hsv_to_rgb (float *hsv, float *rgb)
{
    int i;

    for (i = 0; i < 3; ++i)
	if (hsv[i] < 0.0)
	    hsv[i] = 0.0;
	else if (hsv[i] > 1.0)
	    hsv[i] = 1.0;

    if (hsv[1] == 0.0)
	rgb[0] = rgb[1] = rgb[2] = hsv[2];
    else
    {
	float h = hsv[0];
	float f, p, q, t;
	int i;

	if (h >= 1.0)
	    h = 0.0;
	h *= 6.0;

	i = floor(h);
	f = h - i;
	p = hsv[2] * (1 - hsv[1]);
	q = hsv[2] * (1 - (hsv[1] * f));
	t = hsv[2] * (1 - (hsv[1] * (1 - f)));

	switch (i)
	{
	    case 0 :
		rgb[0] = hsv[2]; rgb[1] = t; rgb[2] = p;
		break;

	    case 1 :
		rgb[0] = q; rgb[1] = hsv[2]; rgb[2] = p;
		break;

	    case 2 :
		rgb[0] = p; rgb[1] = hsv[2]; rgb[2] = t;
		break;

	    case 3 :
		rgb[0] = p; rgb[1] = q; rgb[2] = hsv[2];
		break;

	    case 4 :
		rgb[0] = t; rgb[1] = p; rgb[2] = hsv[2];
		break;

	    case 5 :
		rgb[0] = hsv[2]; rgb[1] = p; rgb[2] = q;
		break;

	    default :
		assert(0);
	}
    }
}
