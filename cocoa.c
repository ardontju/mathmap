/*
 *  cocoa.c
 *  MathMap
 *
 *  Created by Herbert Poetzl on 10/23/07.
 *  Copyright (c) 2007 Herbert Poetzl. All rights reserved.
 *
 */

#include <stdio.h>
#include <assert.h>

#include "cocoa.h"

mathmap_t* compile_mathmap_cocoa (const char *expression, char *template_filename, char *opmacros_filename)
{
    FILE *template = fopen(template_filename, "r");
    mathmap_t *ret;

    ret = compile_mathmap ((char *)expression, template, opmacros_filename);
    fclose(template);
    return ret;
}


void call_invocation(mathmap_invocation_t *invocation, int first_row, int last_row, unsigned char *output)
{
    call_invocation_parallel_and_join (invocation, 0, first_row, invocation->img_width, last_row, output, 1);
}

void
delete_expression_marker (void)
{
    printf("FIXME: delete expression marker\n");	
}

void
set_expression_marker (int line, int column)
{
    delete_expression_marker();
	
    printf("FIXME: the error is at %d:%d\n", line, column);	
}

void
gimp_message (const char *msg)
{
    printf("message: %s\n", msg);
}

CALLBACK_SYMBOL
void
save_debug_tuples (mathmap_invocation_t *invocation, int row, int col)
{
}

void
drawable_get_pixel_inc (mathmap_invocation_t *invocation, input_drawable_t *drawable, int *inc_x, int *inc_y)
{
    *inc_x = *inc_y = 1;
}

color_t
mathmap_get_pixel (mathmap_invocation_t *invocation, input_drawable_t *drawable,
		   int frame, int x, int y)
{
/*
    printf("[%p, %d,%d]>%d,%d,%p ", 
        invocation, x, y, 
        userval->v.image.width, userval->v.image.height, 
        userval->v.image.data);
*/

    assert(drawable != 0);

    if (x < 0 || x >= drawable->width)
	return invocation->edge_color_x;
    if (y < 0 || y >= drawable->height)
	return invocation->edge_color_y;

    guchar *pixel = (guchar*)drawable->v.openstep.data + 4 * x +
	y * drawable->v.openstep.row_stride;

//    printf("<%d,%d,%d,%d> ", pixel[0], pixel[1], pixel[2], pixel[3]);

    return MAKE_RGBA_COLOR(pixel[0], pixel[1], pixel[2], pixel[3]);
}

