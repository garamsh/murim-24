// Draws each instance of a body band's MultiMesh from its own rectangle of the body atlas:
// INSTANCE_CUSTOM holds the rectangle's position and size, as fractions of the atlas. An instance
// coloured other than white is ringed in its colour: each clear pixel of its rectangle beside a
// solid one is drawn in it.
shader_type canvas_item;

varying vec4 ring;
varying vec4 frame;

void vertex() {
	UV = INSTANCE_CUSTOM.xy + UV * INSTANCE_CUSTOM.zw;
	ring = COLOR;
	frame = INSTANCE_CUSTOM;
}

// Whether `uv` lies inside the instance's rectangle.
bool inside(vec2 uv) {
	return all(greaterThanEqual(uv, frame.xy)) && all(lessThan(uv, frame.xy + frame.zw));
}

void fragment() {
	vec4 pixel = texture(TEXTURE, UV);
	if (pixel.a == 0.0 && ring.rgb != vec3(1.0)) {
		vec2 sides[4] = {vec2(TEXTURE_PIXEL_SIZE.x, 0.0), vec2(-TEXTURE_PIXEL_SIZE.x, 0.0),
				vec2(0.0, TEXTURE_PIXEL_SIZE.y), vec2(0.0, -TEXTURE_PIXEL_SIZE.y)};
		for (int side = 0; side < 4; side++) {
			vec2 beside = UV + sides[side];
			if (inside(beside) && texture(TEXTURE, beside).a > 0.0) {
				pixel = vec4(ring.rgb, 1.0);
			}
		}
	}
	COLOR = pixel;
}
