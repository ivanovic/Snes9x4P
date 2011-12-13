#include "simple_noAA_scaler.h"
#include "string.h"

void render_x_single_xy(uint16* destination_pointer_address, uint16 screen_pitch_half,
                     uint16* gfx_screen, uint16 source_panewidth, int Width, int Height) {
	uint16 height_desired = Height;
	uint16 bytes_per_line = Width << 1; // bytes_per_line = Width * 2
	
	// sp is the pointer to a pixel in the SDL screen
	register uint16 *sp16 = gfx_screen;
	// dp is the pointer to a pixel in the "real" screen
	register uint16 *dp16 = destination_pointer_address;
	
	for (register uint16 i = 0; i < height_desired; ++i) {
		//since we apply no scaling in X dimension here, we can simply use a single memcpy instead of a for loop
		memcpy ( dp16, sp16, bytes_per_line );
		
		// update the references to display and sdl screen for the next run
		dp16 += screen_pitch_half;
		sp16 += source_panewidth;
	} // for each height unit
}

void render_x_single(uint16* destination_pointer_address, uint16 screen_pitch_half,
                     uint16* gfx_screen, uint16 source_panewidth, int Width, int Height) {
	uint16 height_desired = Height << 1; // height_desired = Height * 2
	uint16 bytes_per_line = Width << 1; // bytes_per_line = Width * 2
	
	// dp is the pointer to a pixel in the "real" screen
	register uint16 *dp16 = destination_pointer_address;
	
	for (register uint16 i = 0; i < height_desired; ++i) {
		// sp is the pointer to a pixel in the SDL screen
		register uint16 *sp16 = gfx_screen + ( ( i >> 1 ) * source_panewidth );
		
		//since we apply no scaling in X dimension here, we can simply use a single memcpy instead of a for loop
		memcpy ( dp16, sp16, bytes_per_line );
		
		// update the references to display for the next run
		dp16 += screen_pitch_half;
	} // for each height unit
}

void render_x_double(uint16* destination_pointer_address, uint16 screen_pitch_half,
                     uint16* gfx_screen, uint16 source_panewidth, int Width, int Height) {
	uint16 height_desired = Height << 1; // height_desired = Height * 2
	for (register uint16 i = 0; i < height_desired; ++i) {
		// dp is pointer to destination in "real" screen
		register uint16 *dp16 = destination_pointer_address + ( i * screen_pitch_half );
		
		// sp is the pointer to a pixel in the SDL screen
		register uint16 *sp16 = gfx_screen;
		// ( i / 2 ) * source_panewidth; "i/2" for doubling lines, aka "deinterlacing"
		sp16 += ( ( i >> 1 ) * source_panewidth );
		
		for (register uint16 j = 0; j < Width ; ++j, ++sp16) {
			*dp16++ = *sp16;
			*dp16++ = *sp16; // doubled
		}
	} // for each height unit
}

void render_x_triple(uint16* destination_pointer_address, uint16 screen_pitch_half,
                     uint16* gfx_screen, uint16 source_panewidth, int Width, int Height) {
	uint16 height_desired = Height << 1; // height_desired = Height * 2
	for (register uint16 i = 0; i < height_desired; ++i) {
		// dp is pointer to destination in "real" screen
		register uint16 *dp16 = destination_pointer_address + ( i * screen_pitch_half );
		
		// sp is the pointer to a pixel in the SDL screen
		register uint16 *sp16 = gfx_screen;
		// ( i / 2 ) * source_panewidth; "i/2" for doubling lines, aka "deinterlacing"
		sp16 += ( ( i >> 1 ) * source_panewidth );
		
		for (register uint16 j = 0; j < Width ; ++j, ++sp16) {
			*dp16++ = *sp16;
			*dp16++ = *sp16; // doubled
			*dp16++ = *sp16; // tripled
		}
	} // for each height unit
}

void render_x_oneandhalf(uint16* destination_pointer_address, uint16 screen_pitch_half,
                         uint16* gfx_screen, uint16 source_panewidth, int Width, int Height) {
	uint16 height_desired = Height << 1; // height_desired = Height * 2
	uint16 Width_half = Width >> 1;  // Width_half = Width / 2
	for (register uint16 i = 0; i < height_desired; ++i) {
		// dp is pointer to destination in "real" screen
		register uint16 *dp16 = destination_pointer_address + ( i * screen_pitch_half );
		
		// sp is the pointer to a pixel in the SDL screen
		register uint16 *sp16 = gfx_screen;
		// ( i / 2 ) * source_panewidth; "i/2" for doubling lines, aka "deinterlacing"
		sp16 += ( ( i >> 1 ) * source_panewidth );
		
		//only use every 2nd pixel but paint it thrice -> very simple 1.5x scaling for every pixel in X direction
		for (register uint16 j = 0; j < Width_half; ++j, ++(++sp16)) {
			*dp16++ = *sp16;
			*dp16++ = *sp16; // doubled
			*dp16++ = *sp16; // tripled
		}
	}// for each height unit
}