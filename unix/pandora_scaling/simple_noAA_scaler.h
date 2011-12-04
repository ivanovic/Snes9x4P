

#ifndef __SIMPLE_NOAA_SCALER_H
#define __SIMPLE_NOAA_SCALER_H

typedef unsigned short uint16;

// used for rendering 1 pixel per source pixel in X direction and 1px per source pixel in Y direction
// this is eg required for HiRes games in 512x244 mode
void render_x_single_xy(uint16* destination_pointer_address, uint16 screen_pitch_half,
                     uint16* gfx_screen, uint16 source_panewidth, int Width, int Height);

// used for rendering 1 pixel per source pixel in X direction and 2px per source pixel in Y direction
// this is eg required for HiRes games in 512x244 mode
void render_x_single(uint16* destination_pointer_address, uint16 screen_pitch_half,
                     uint16* gfx_screen, uint16 source_panewidth, int Width, int Height);

// used for rendering 2 pixel per source pixel in X direction and 2px per source pixel in Y direction
// this is the most common mode, used for the menu (320x240), lowres games (256x244/256x239) as well
// as HiRes games when in lower res mode (256x244/256x239)
void render_x_double(uint16* destination_pointer_address, uint16 screen_pitch_half,
                     uint16* gfx_screen, uint16 source_panewidth, int Width, int Height);

// used for rendering 3 pixel per source pixel in X direction and 2px per source pixel in Y direction
// this is basically some stretching to widescreen mode
void render_x_triple(uint16* destination_pointer_address, uint16 screen_pitch_half,
                     uint16* gfx_screen, uint16 source_panewidth, int Width, int Height);

// used for rendering 1.5 pixel per source pixel in X direction and 2px per source pixel in Y direction
// this is only required for HiRes games when in 512x244 mode
void render_x_oneandhalf(uint16* destination_pointer_address, uint16 screen_pitch_half,
                         uint16* gfx_screen, uint16 source_panewidth, int Width, int Height);

#endif // __SIMPLE_NOAA_SCALER_H
