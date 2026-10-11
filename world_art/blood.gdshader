// Draws each instance of a blood MultiMesh, a drop or a pixel of a spray: a whole pixel in the
// colour its instance carries, shown while it lasts and thinned by the ordered dither as it fades,
// so it blends nothing (docs/art-direction.md §Palette). INSTANCE_CUSTOM holds when it fell, in
// seconds, and its dither threshold, the 4x4 Bayer matrix's at its pixel, given with it because a
// MultiMesh's VERTEX is its quad's own and not the pixel's. `now` is the time drawn and `lasts`
// how long a pixel shows.
shader_type canvas_item;

uniform float now;
uniform float lasts = 1.0;

varying float left;
varying float threshold;

void vertex() {
	left = 1.0 - (now - INSTANCE_CUSTOM.x) / lasts;
	threshold = INSTANCE_CUSTOM.y;
}

void fragment() {
	if (left <= threshold) {
		discard;
	}
	COLOR = vec4(COLOR.rgb, 1.0);
}
