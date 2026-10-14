// The shadows of clouds drifting over the ground, as grade.gd sets them by the time of day: the
// ground world_art draws takes this material, and nothing standing on it does. How dark the shadow
// is at each art pixel of the screen it reads from the alpha of grade.tscn's light field, where the
// clouds' noise is drawn once an art pixel; the ground leans toward the shade its own contact
// shadows take, in two bands whose edges the ordered dither steps on the world's pixels, so the
// shadow moves in whole art pixels and blends nothing.
shader_type canvas_item;

#include "res://world_art/dither.gdshaderinc"

// Whether the shadows show, above 0; the light field they are read from; and the screen pixels an
// art pixel spans.
uniform float clouds = 0.0;
uniform sampler2D field : filter_nearest;
uniform float art_pixel = 1.0;

// The shade the ground leans toward and how far, as world_art/ground.gd's SHADE and SHADE_SHARE.
const vec3 SHADE = vec3(0.078, 0.094, 0.157);
const float SHARE = 0.38;

varying vec2 world;

void vertex() {
	world = (MODEL_MATRIX * vec4(VERTEX, 0.0, 1.0)).xy;
}

void fragment() {
	if (clouds > 0.0) {
		float shade = texelFetch(field, ivec2(floor(FRAGCOORD.xy / art_pixel)), 0).a;
		float band = floor(shade * 2.0 + dither_threshold(floor(world))) / 2.0;
		COLOR.rgb = mix(COLOR.rgb, SHADE, band * SHARE);
	}
}
