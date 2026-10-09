// Draws each instance of a body band's MultiMesh from its own rectangle of the body atlas:
// INSTANCE_CUSTOM holds the rectangle's position, as fractions of the atlas, and `frame_size` its
// size. An instance coloured other than white is ringed in its colour: each clear pixel of its
// rectangle beside a solid one is drawn in it. INSTANCE_CUSTOM's third value is the share of the
// body's pixels drawn in the impact core's colour, as when it is struck, and its fourth the share
// drawn at all, as when it fades in death, each by an ordered dither over the sprite's own pixels.
shader_type canvas_item;

#include "res://world_art/dither.gdshaderinc"

// The fire ramp's lightest, the impact core of docs/art-direction.md §Palette.
const vec3 IMPACT = vec3(1.0, 0.973, 0.863);

uniform vec2 frame_size;

varying vec4 ring;
varying vec4 frame;
varying float white;
varying float shown;

void vertex() {
	UV = INSTANCE_CUSTOM.xy + UV * frame_size;
	ring = COLOR;
	frame = vec4(INSTANCE_CUSTOM.xy, frame_size);
	white = INSTANCE_CUSTOM.z;
	shown = INSTANCE_CUSTOM.w;
}

// Whether `uv` lies inside the instance's rectangle.
bool inside(vec2 uv) {
	return all(greaterThanEqual(uv, frame.xy)) && all(lessThan(uv, frame.xy + frame.zw));
}

void fragment() {
	vec4 pixel = texture(TEXTURE, UV);
	float threshold = dither_threshold(UV / TEXTURE_PIXEL_SIZE);
	if (pixel.a >= 0.5 && white > threshold) {
		pixel = vec4(IMPACT, 1.0);
	} else if (pixel.a == 0.0 && ring.rgb != vec3(1.0)) {
		vec2 sides[4] = {vec2(TEXTURE_PIXEL_SIZE.x, 0.0), vec2(-TEXTURE_PIXEL_SIZE.x, 0.0),
				vec2(0.0, TEXTURE_PIXEL_SIZE.y), vec2(0.0, -TEXTURE_PIXEL_SIZE.y)};
		for (int side = 0; side < 4; side++) {
			vec2 beside = UV + sides[side];
			if (inside(beside) && texture(TEXTURE, beside).a > 0.0) {
				pixel = vec4(ring.rgb, 1.0);
			}
		}
	}
	if (pixel.a < 0.5 || shown <= threshold) {
		discard;
	}
	COLOR = pixel;
}
