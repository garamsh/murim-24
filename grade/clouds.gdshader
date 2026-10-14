// The shadows of clouds drifting over the ground, as grade.gd sets them by the time of day: the
// ground world_art draws takes this material, and nothing standing on it does. Where two octaves of
// noise over the world's pixels, moved by the wind as `drift` seconds pass, rise past a cover, the
// ground leans toward the shade its own contact shadows take, in two bands whose edges the ordered
// dither steps on the world's pixels, so the shadow moves in whole art pixels and blends nothing.
shader_type canvas_item;

#include "res://world_art/dither.gdshaderinc"
#include "res://grade/noise.gdshaderinc"

// How dark the shadows are, 0 for none, and the seconds the wind has blown.
uniform float clouds = 0.0;
uniform float drift = 0.0;

// The shade the ground leans toward and how far, as world_art/ground.gd's SHADE and SHADE_SHARE.
const vec3 SHADE = vec3(0.078, 0.094, 0.157);
const float SHARE = 0.38;
// The pixels a cloud's noise varies over, the wind in pixels a second, and the noise above which a
// cloud's shadow begins and is whole.
const float SCALE = 160.0;
const vec2 WIND = vec2(5.0, 2.0);
const float EDGE = 0.55;
const float CORE = 0.68;

varying vec2 world;

void vertex() {
	world = (MODEL_MATRIX * vec4(VERTEX, 0.0, 1.0)).xy;
}

void fragment() {
	if (clouds > 0.0) {
		vec2 pixel = floor(world);
		vec2 p = (pixel - drift * WIND) / SCALE;
		float cover = (noise_at(p, 11) * 2.0 + noise_at(p * 2.07, 12)) / 3.0;
		float shade = smoothstep(EDGE, CORE, cover) * clouds;
		float band = floor(shade * 2.0 + dither_threshold(pixel)) / 2.0;
		COLOR.rgb = mix(COLOR.rgb, SHADE, band * SHARE);
	}
}
