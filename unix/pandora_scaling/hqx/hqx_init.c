/*
 * Copyright (C) 2010 Cameron Zemek ( grom@zeminvaders.net)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <stdint.h>
#include "hqx.h"
#include <SDL/SDL.h>

uint32_t   RGBtoYUV[65536];
uint32_t   YUV1, YUV2;

extern SDL_Surface *screen,*gfxscreen;

HQX_API void HQX_CALLCONV hqxInit(void)
{
    /* Initalize RGB to YUV lookup table */
    uint32_t c, r, g, b, y, u, v, temp;
    for (c = 0; c < 65536; c++) {
//        r = (c & 0xFF0000) >> 16;
//        g = (c & 0x00FF00) >> 8;
//        b = c & 0x0000FF;
		temp = c & screen->format->Rmask;
		temp = temp >> screen->format->Rshift;
		temp = temp << screen->format->Rloss;
		r = temp&0xFF;

		temp = c & screen->format->Gmask;
		temp = temp >> screen->format->Gshift;
		temp = temp << screen->format->Gloss;
		g = temp&0xFF;

		temp = c & screen->format->Bmask;
		temp = temp >> screen->format->Bshift;
		temp = temp << screen->format->Bloss;
		b = temp&0xFF;

		//r = ((c >> 11) & 31) << 3;
		//g = ((c >> 5) & 63) << 2;
		//b = (c & 31) << 3;
		// TODO: redo these calculations for 16 bit colours instead of 24/32
        y = (uint32_t)(0.299*r + 0.587*g + 0.114*b);
        u = (uint32_t)(-0.169*r - 0.331*g + 0.5*b) + 128;
        v = (uint32_t)(0.5*r - 0.419*g - 0.081*b) + 128;
        RGBtoYUV[c] = (y << 16) + (u << 8) + v;
        //LUT16to32[c] = (r << 16) + (g << 8) + b; //((c & 0xF800) << 8) + ((c & 0x07E0) << 5) + ((c & 0x001F) << 3);
    }
}
