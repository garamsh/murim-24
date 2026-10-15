// What each art pixel of the screen takes from the lights, the fog and the clouds, drawn once a
// frame into grade.tscn's light field, a viewport of one pixel an art pixel, for grade.gdshader and
// the clouds' material to read: in red, how near the nearest of the lights is, 1 on its rect and
// falling to 0 at its reach; in green, 1 where the art pixel lies in a lit window's rect; in blue,
// how thick the fog is there, thickening toward the screen's top, thinning with the ground's level,
// and drifting with the wind; in alpha, how dark the clouds' shadow is there, where two octaves of
// noise over the world's pixels, moved by the wind, rise past a cover. Drawn at the art's pixels
// rather than the screen's, each costs an art pixel's share of the screen's.
shader_type canvas_item;
render_mode blend_disabled;

#include "res://grade/noise.gdshaderinc"

// The screen pixels an art pixel spans; where the world's art pixels begin on the screen, in screen
// pixels from its corner, from minus an art pixel to 0, so this field's first pixel is the art
// pixel the screen's corner shows; and each light's rect, x, y, width and height, and how far its
// light reaches, in screen pixels.
uniform float art_pixel = 1.0;
uniform vec2 grid_offset = vec2(0.0);
uniform int light_count = 0;
uniform vec4 light_rects[32];
uniform float light_reach[32];
// How thick the fog is, 0 for none; how dark the clouds' shadows are, 0 for none; the world's pixel
// of this field's first pixel; the seconds the wind has blown; and the relief's cells in view, one
// texel a cell, its level plus one in green, `cells_size` of them from cell `cells_origin`.
uniform float fog = 0.0;
uniform float clouds = 0.0;
uniform vec2 view_origin = vec2(0.0);
uniform float drift = 0.0;
uniform sampler2D cells : filter_nearest;
uniform ivec2 cells_origin = ivec2(0);
uniform ivec2 cells_size = ivec2(0);

// The pixels the fog's noise varies over, its wind in pixels a second, and the level from which
// ground stands clear of it; a cell's size and a level's height in pixels, and the highest level,
// as world_art/relief.gd's CELL, LEVEL_PX and TOP.
const float FOG_SCALE = 96.0;
const vec2 FOG_WIND = vec2(6.0, 1.5);
const float FOG_LEVELS = 4.0;
const vec2 CELL = vec2(16.0, 12.0);
const float LEVEL_PX = 16.0;
const int TOP = 5;
// The pixels a cloud's noise varies over, the wind in pixels a second, and the noise above which a
// cloud's shadow begins and is whole.
const float CLOUD_SCALE = 160.0;
const vec2 CLOUD_WIND = vec2(5.0, 2.0);
const float CLOUD_EDGE = 0.55;
const float CLOUD_CORE = 0.68;

// The level of the ground drawn at `world`, a pixel of the world as the screen shows it: the highest
// level whose ground, lifted by that level, reaches the pixel. It reads the cells unwarped, near
// enough for a mist.
float level_at(vec2 world) {
	for (int level = TOP; level > 0; level--) {
		ivec2 cell = ivec2(floor((world + vec2(0.0, float(level) * LEVEL_PX)) / CELL)) - cells_origin;
		if (cell.x >= 0 && cell.y >= 0 && cell.x < cells_size.x && cell.y < cells_size.y
				&& texelFetch(cells, cell, 0).g * 255.0 - 1.0 >= float(level) - 0.5) {
			return float(level);
		}
	}
	return 0.0;
}

void fragment() {
	vec2 at = FRAGCOORD.xy * art_pixel + grid_offset;
	float lit = 0.0;
	float pane = 0.0;
	for (int i = 0; i < light_count; i++) {
		vec4 r = light_rects[i];
		vec2 off = max(max(r.xy - at, at - (r.xy + r.zw)), vec2(0.0));
		lit = max(lit, 1.0 - length(off) / light_reach[i]);
		if (r.z > 0.0 && at.x >= r.x && at.y >= r.y && at.x < r.x + r.z && at.y < r.y + r.w) {
			pane = 1.0;
		}
	}
	vec2 pixel = view_origin + floor(FRAGCOORD.xy);
	float thick = 0.0;
	if (fog > 0.0) {
		vec2 world = pixel + 0.5;
		float far = smoothstep(0.1, 1.0, 1.0 - FRAGCOORD.y * SCREEN_PIXEL_SIZE.y);
		float clear = clamp(1.0 - level_at(world) / FOG_LEVELS, 0.0, 1.0);
		float drifting = 0.5 + 0.5 * noise_at((world - drift * FOG_WIND) / FOG_SCALE, 21);
		thick = clamp(fog * far * clear * drifting, 0.0, 1.0);
	}
	float shade = 0.0;
	if (clouds > 0.0) {
		vec2 p = (pixel - drift * CLOUD_WIND) / CLOUD_SCALE;
		float cover = (noise_at(p, 11) * 2.0 + noise_at(p * 2.07, 12)) / 3.0;
		shade = smoothstep(CLOUD_EDGE, CLOUD_CORE, cover) * clouds;
	}
	COLOR = vec4(clamp(lit, 0.0, 1.0), pane, thick, shade);
}
