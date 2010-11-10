#include "simple_noAA_scaler.h"

void render_x_single(uint16* destination_pointer_address, uint16 screen_pitch_half,
                     uint16* gfx_screen, uint16 source_panewidth, int Width, int Height) {
	uint16 height_desired = Height * 2;
	for (register uint16 i = 0; i < height_desired; ++i) {
		register uint16 *dp16 = destination_pointer_address + ( i * screen_pitch_half );
		
		register uint16 *sp16 = gfx_screen;
		sp16 += ( ( i / 2 ) * source_panewidth );
		
		for (register uint16 j = 0; j < Width ; ++j, ++sp16) {
			*dp16++ = *sp16;
		}
	} // for each height unit
}

void render_x_double(uint16* destination_pointer_address, uint16 screen_pitch_half,
                     uint16* gfx_screen, uint16 source_panewidth, int Width, int Height) {
	uint16 height_desired = Height * 2;
	for (register uint16 i = 0; i < height_desired; ++i) {
		register uint16 *dp16 = destination_pointer_address + ( i * screen_pitch_half );
		
		register uint16 *sp16 = gfx_screen;
		sp16 += ( ( i / 2 ) * source_panewidth );
		
		for (register uint16 j = 0; j < Width ; ++j, ++sp16) {
			*dp16++ = *sp16;
			*dp16++ = *sp16; // doubled
		}
	} // for each height unit
}

void render_x_triple(uint16* destination_pointer_address, uint16 screen_pitch_half,
                     uint16* gfx_screen, uint16 source_panewidth, int Width, int Height) {
	uint16 height_desired = Height * 2;
	for (register uint16 i = 0; i < height_desired; ++i) {
		register uint16 *dp16 = destination_pointer_address + ( i * screen_pitch_half );
		
		register uint16 *sp16 = gfx_screen;
		sp16 += ( ( i / 2 ) * source_panewidth );
		
		for (register uint16 j = 0; j < Width ; ++j, ++sp16) {
			*dp16++ = *sp16;
			*dp16++ = *sp16; // doubled
			*dp16++ = *sp16; // tripled
		}
	} // for each height unit
}

void render_x_oneandhalf(uint16* destination_pointer_address, uint16 screen_pitch_half,
                         uint16* gfx_screen, uint16 source_panewidth, int Width, int Height) {
	uint16 height_desired = Height * 2;
	uint16 Width_half = Width >> 1;
	for (register uint16 i = 0; i < height_desired; ++i) {
		register uint16 *dp16 = destination_pointer_address + ( i * screen_pitch_half );
		
		register uint16 *sp16 = gfx_screen;
		sp16 += ( ( i / 2 ) * source_panewidth );
		
		//only use every 2nd pixel but paint it thrice -> very simple 1.5x scaling
		for (register uint16 j = 0; j < Width_half; ++j, ++(++sp16)) {
			*dp16++ = *sp16;
			*dp16++ = *sp16; // doubled
			*dp16++ = *sp16; // tripled
		}
	}// for each height unit
}