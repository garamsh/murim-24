// Grades the world drawn under it by time of day and season, as grade.gd sets it from
// grades.json: a gain and a lift per channel, the shadows tinted toward `shadow` and the lights
// toward `light` by how dark or light each pixel is, then saturation and contrast; the darkest
// shadows lean further toward blue-violet and the brightest lights warm, by `hue_shift`. A light
// vignette darkens toward the screen's corners, on the art's pixel grid and by the ordered dither,
// so its edge steps in whole art pixels and blends nothing. By `lamp`, the lights grade.gd gives
// warm the pixels round them back toward their own colour, in bands stepped by the dither, and the
// dark panes of a lit window take the fire ramp. Emitters, the fire ramp's three brightest steps,
// keep their own colour, and by `bloom` the art pixels round them lift toward its amber, dithered.
shader_type canvas_item;

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
// Each light's rect, x, y, width and height, and how far its light reaches, in screen pixels.
uniform int light_count = 0;
uniform vec4 light_rects[32];
uniform float light_reach[32];

const float BAYER[16] = float[](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0, 3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);

// The emitters, the fire ramp's three brightest steps: amber, yellow and white-hot.
const vec3 EMITTERS[3] = vec3[](vec3(0.961, 0.639, 0.235), vec3(0.984, 0.890, 0.478), vec3(1.0, 0.973, 0.863));
// The colour lamplight gives what it falls on, and the eight directions bloom looks for emitters in.
const vec3 LAMPLIGHT = vec3(1.12, 0.9, 0.62);
const vec2 TAPS[8] = vec2[](vec2(1.0, 0.0), vec2(-1.0, 0.0), vec2(0.0, 1.0), vec2(0.0, -1.0),
		vec2(1.0, 1.0), vec2(-1.0, 1.0), vec2(1.0, -1.0), vec2(-1.0, -1.0));

float luma(vec3 c) {
	return dot(c, vec3(0.299, 0.587, 0.114));
}

bool is_emitter(vec3 c) {
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
		vec2 at = (cell + 0.5) * art_pixel;
		float lit = 0.0;
		bool pane = false;
		for (int i = 0; i < light_count; i++) {
			vec4 r = light_rects[i];
			vec2 off = max(max(r.xy - at, at - (r.xy + r.zw)), vec2(0.0));
			lit = max(lit, 1.0 - length(off) / light_reach[i]);
			pane = pane || (r.z > 0.0 && FRAGCOORD.x >= r.x && FRAGCOORD.y >= r.y
					&& FRAGCOORD.x < r.x + r.z && FRAGCOORD.y < r.y + r.w);
		}
		// Three bands, their edges dithered on the art's pixels.
		float band = floor(clamp(lit, 0.0, 1.0) * 3.0 + dither) / 3.0;
		g = mix(g, c * LAMPLIGHT, band * lamp * 0.85);
		if (pane && l < 0.15 && dither < lamp) {
			g = EMITTERS[dither < 0.5 ? 1 : 0];
		}
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
