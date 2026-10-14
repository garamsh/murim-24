// Grades the world drawn under it by time of day and season, as grade.gd sets it from
// grades.json: a gain and a lift per channel, the shadows tinted toward `shadow` and the lights
// toward `light` by how dark or light each pixel is, then saturation and contrast; the darkest
// shadows lean further toward blue-violet and the brightest lights warm, by `hue_shift`. A light
// vignette darkens toward the screen's corners, on the art's pixel grid and by the ordered dither,
// so its edge steps in whole art pixels and blends nothing. By `lamp`, the lights grade.gd gives
// warm the pixels round them toward the lamp's amber at their own lightness, in bands stepped by
// the dither, and a lit window's panes, of wood's darkest step, take the fire ramp. Emitters, the
// fire ramp's three brightest steps, keep their own colour, and by `bloom` the art pixels round
// them lift toward its amber, dithered.
// By `fog`, a mist the grade tints thickens toward the screen's top, the far ground, in bands the
// dither steps, drifting with the wind; ground FOG_LEVELS levels high, and whatever stands on it,
// rises above it, read from the relief's `cells` as world_art/ground.gdshader reads them.
shader_type canvas_item;

#include "res://grade/noise.gdshaderinc"

uniform sampler2D screen : hint_screen_texture, filter_nearest;
uniform vec3 gain = vec3(1.0);
uniform vec3 lift = vec3(0.0);
uniform vec3 shadow = vec3(1.0);
uniform vec3 light = vec3(1.0);
uniform float saturation = 1.0;
uniform float contrast = 1.0;
uniform float hue_shift = 0.0;
// How much the corners darken, 0 for no vignette, and the screen pixels an art pixel spans.
uniform float vignette = 0.0;
uniform float art_pixel = 1.0;
uniform float lamp = 0.0;
uniform float bloom = 0.0;
// How many lights show, and the light field grade.gd draws them into, one pixel an art pixel: how
// lit each is in red, and in green 1 within a lit window's rect.
uniform int light_count = 0;
uniform sampler2D light_field : filter_nearest;
// How thick the fog is, 0 for none; the world's pixel at the screen's top-left; the seconds the wind
// has blown; and the relief's cells, one texel a cell, its level plus one in green, `map_size` of them.
uniform float fog = 0.0;
uniform vec2 view_origin = vec2(0.0);
uniform float drift = 0.0;
uniform sampler2D cells : filter_nearest;
uniform ivec2 map_size = ivec2(0);

const float BAYER[16] = float[](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0, 3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);

// The emitters, the fire ramp's three brightest steps: amber, yellow and white-hot.
const vec3 EMITTERS[3] = vec3[](vec3(0.961, 0.639, 0.235), vec3(0.984, 0.890, 0.478), vec3(1.0, 0.973, 0.863));
// The colour lamplight gives what it falls on, as a gain on its lightness, and how much of the lit
// colour's own hue it keeps; a window pane's colour; and the eight directions bloom looks for
// emitters in.
const vec3 LAMPLIGHT = vec3(1.45, 1.02, 0.56);
const float OWN_HUE = 0.25;
const vec3 PANE = vec3(0.133, 0.075, 0.173);
const vec2 TAPS[8] = vec2[](vec2(1.0, 0.0), vec2(-1.0, 0.0), vec2(0.0, 1.0), vec2(0.0, -1.0),
		vec2(1.0, 1.0), vec2(-1.0, 1.0), vec2(1.0, -1.0), vec2(-1.0, -1.0));
// The fog's colour before the grade tints it, the pixels its noise varies over, its wind in pixels a
// second, and the level from which ground stands clear of it; a cell's size and a level's height in
// pixels, and the highest level, as world_art/relief.gd's CELL, LEVEL_PX and TOP.
const vec3 FOG_COLOUR = vec3(0.8, 0.84, 0.9);
const float FOG_SCALE = 96.0;
const vec2 FOG_WIND = vec2(6.0, 1.5);
const float FOG_LEVELS = 4.0;
const vec2 CELL = vec2(16.0, 12.0);
const float LEVEL_PX = 16.0;
const int TOP = 5;

float luma(vec3 c) {
	return dot(c, vec3(0.299, 0.587, 0.114));
}

// The level of the ground drawn at `world`, a pixel of the world as the screen shows it: the highest
// level whose ground, lifted by that level, reaches the pixel. It reads the cells unwarped, near
// enough for a mist.
float level_at(vec2 world) {
	for (int level = TOP; level > 0; level--) {
		ivec2 cell = ivec2(floor((world + vec2(0.0, float(level) * LEVEL_PX)) / CELL));
		if (cell.x >= 0 && cell.y >= 0 && cell.x < map_size.x && cell.y < map_size.y
				&& texelFetch(cells, cell, 0).g * 255.0 - 1.0 >= float(level) - 0.5) {
			return float(level);
		}
	}
	return 0.0;
}

bool is_emitter(vec3 c) {
	// Every emitter's red is above 0.95, which turns most pixels away at once.
	if (c.r < 0.95) {
		return false;
	}
	for (int i = 0; i < 3; i++) {
		if (distance(c, EMITTERS[i]) < 0.01) {
			return true;
		}
	}
	return false;
}

void fragment() {
	vec3 c = texture(screen, SCREEN_UV).rgb;
	float l = clamp(luma(c), 0.0, 1.0);
	vec3 g = c * gain + lift;
	g *= mix(shadow, light, l);
	float l2 = luma(g);
	g = mix(vec3(l2), g, saturation);
	g = (g - 0.3) * contrast + 0.3;
	// The darks toward blue-violet, the lights warmer.
	if (l < 0.5) {
		float a = (0.5 - l) * 1.2 * hue_shift;
		g += (vec3(l * 0.62, l * 0.66, l * 1.4) - g) * a;
	} else if (l > 0.62) {
		float a = (l - 0.62) * 0.6 * hue_shift;
		g *= vec3(1.0 + 0.1 * a, 1.0 + 0.03 * a, 1.0 - 0.14 * a);
	}
	vec2 cell = floor(FRAGCOORD.xy / art_pixel);
	float dither = (BAYER[(int(mod(cell.y, 4.0)) * 4) + int(mod(cell.x, 4.0))] + 0.5) / 16.0;
	if (lamp > 0.0 && light_count > 0) {
		vec2 field = texelFetch(light_field, ivec2(cell), 0).rg;
		float lit = field.r;
		bool pane = field.g > 0.5;
		// Three bands, their edges dithered on the art's pixels.
		float band = floor(clamp(lit, 0.0, 1.0) * 3.0 + dither) / 3.0;
		vec3 warm = mix(vec3(l), c, OWN_HUE) * LAMPLIGHT;
		g = mix(g, warm, band * lamp * 0.85);
		if (pane && distance(c, PANE) < 0.01 && dither < lamp) {
			g = EMITTERS[dither < 0.5 ? 1 : 0];
		}
	}
	if (fog > 0.0) {
		vec2 world = view_origin + cell + 0.5;
		float far = smoothstep(0.1, 1.0, 1.0 - FRAGCOORD.y * SCREEN_PIXEL_SIZE.y);
		float clear = clamp(1.0 - level_at(world) / FOG_LEVELS, 0.0, 1.0);
		float drifting = 0.5 + 0.5 * noise_at((world - drift * FOG_WIND) / FOG_SCALE, 21);
		float band = floor(clamp(fog * far * clear * drifting, 0.0, 1.0) * 3.0 + dither) / 3.0;
		g = mix(g, FOG_COLOUR * gain + lift, band * 0.7);
	}
	if (vignette > 0.0) {
		vec2 centred = (cell + 0.5) * art_pixel * SCREEN_PIXEL_SIZE * 2.0 - 1.0;
		if (dither < smoothstep(0.55, 1.35, length(centred))) {
			g *= 1.0 - vignette;
		}
	}
	if (is_emitter(c)) {
		g = c;
	} else if (bloom > 0.0) {
		// Emitters within one and two art pixels, the nearer counting twice.
		vec2 centre = (cell + 0.5) * art_pixel * SCREEN_PIXEL_SIZE;
		vec2 step = art_pixel * SCREEN_PIXEL_SIZE;
		float near = 0.0;
		for (int i = 0; i < 8; i++) {
			near += is_emitter(texture(screen, centre + TAPS[i] * step).rgb) ? 2.0 : 0.0;
			near += is_emitter(texture(screen, centre + TAPS[i] * step * 2.0).rgb) ? 1.0 : 0.0;
		}
		if (dither < near / 8.0 * bloom * mix(0.6, 1.0, lamp)) {
			g = mix(g, EMITTERS[0], 0.5);
		}
	}
	COLOR = vec4(clamp(g, 0.0, 1.0), 1.0);
}
