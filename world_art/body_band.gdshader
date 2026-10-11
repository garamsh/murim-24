// Draws each instance of a body band's MultiMesh from its own rectangle of the body atlas:
// INSTANCE_CUSTOM holds the rectangle's position, as fractions of the atlas, and `frame_size` its
// size. INSTANCE_CUSTOM's third value is the share of the body's pixels drawn in the impact core's
// colour, as when it is struck, and its fourth the share drawn at all, as when it fades in death,
// each by an ordered dither over the sprite's own pixels.
shader_type canvas_item;

#include "res://world_art/flash.gdshaderinc"

uniform vec2 frame_size;

varying float white;
varying float shown;

void vertex() {
	UV = INSTANCE_CUSTOM.xy + UV * frame_size;
	white = INSTANCE_CUSTOM.z;
	shown = INSTANCE_CUSTOM.w;
}

void fragment() {
	vec4 pixel = texture(TEXTURE, UV);
	float threshold = dither_threshold(UV / TEXTURE_PIXEL_SIZE);
	if (pixel.a >= 0.5 && flashed(white, UV / TEXTURE_PIXEL_SIZE)) {
		pixel = vec4(IMPACT, 1.0);
	}
	if (pixel.a < 0.5 || shown <= threshold) {
		discard;
	}
	COLOR = pixel;
}
