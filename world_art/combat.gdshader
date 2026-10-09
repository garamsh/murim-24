// Draws what combat.gd draws in whole palette pixels: each colour's alpha is the share of its
// pixels shown, by an ordered dither over the world's pixels, so a flash thins out as it fades
// rather than blending with what lies under it.
shader_type canvas_item;

#include "res://world_art/dither.gdshaderinc"

varying vec2 world_pixel;

void vertex() {
	world_pixel = VERTEX;
}

void fragment() {
	vec4 drawn = texture(TEXTURE, UV) * COLOR;
	if (drawn.a <= dither_threshold(world_pixel)) {
		discard;
	}
	COLOR = vec4(drawn.rgb, 1.0);
}
