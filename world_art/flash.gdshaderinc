// A struck body's flash, shared by the bands that draw it and the
// outline pass, which must know the colour the body shows on screen: a body drawn `white` of the
// way to white shows its atlas pixel `atlas_pixel` in IMPACT where the ordered dither says so.

#include "res://world_art/dither.gdshaderinc"

// The fire ramp's lightest, the impact core of docs/art-direction.md §Palette.
const vec3 IMPACT = vec3(1.0, 0.973, 0.863);

bool flashed(float white, vec2 atlas_pixel) {
	return white > dither_threshold(atlas_pixel);
}
