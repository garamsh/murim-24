// Grades the world drawn under it by time of day and season, as grade.gd sets it from
// grades.json: a gain and a lift per channel, the shadows tinted toward `shadow` and the lights
// toward `light` by how dark or light each pixel is, then saturation and contrast; the darkest
// shadows lean further toward blue-violet and the brightest lights warm, by `hue_shift`. A light
// vignette darkens toward the screen's corners, on the art's pixel grid and by the ordered dither,
// so its edge steps in whole art pixels and blends nothing.
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

const float BAYER[16] = float[](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0, 3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);

float luma(vec3 c) {
	return dot(c, vec3(0.299, 0.587, 0.114));
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
	if (vignette > 0.0) {
		vec2 cell = floor(FRAGCOORD.xy / art_pixel);
		vec2 centred = SCREEN_UV * 2.0 - 1.0;
		float reach = smoothstep(0.55, 1.35, length(centred));
		int k = (int(mod(cell.y, 4.0)) * 4) + int(mod(cell.x, 4.0));
		if ((BAYER[k] + 0.5) / 16.0 < reach) {
			g *= 1.0 - vignette;
		}
	}
	COLOR = vec4(clamp(g, 0.0, 1.0), 1.0);
}
