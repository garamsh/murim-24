// Draws each instance of a blood MultiMesh, a pixel of a drop or of a spray: a whole pixel in the
// colour its instance carries, shown while it lasts and thinned by the ordered dither as it fades,
// so it blends nothing (docs/art-direction.md §Palette). INSTANCE_CUSTOM holds when it fell, in
// seconds; its dither threshold, from the 4x4 Bayer matrix, given with it because a MultiMesh's
// VERTEX is its quad's own and not the pixel's; and how far it flies, in pixels, none for a drop.
// A pixel flies over the first `flight` share of its time, quickly at first and slowing, falling
// `fall` pixels by then, by whole pixels, and fades where it lands. `now` is the time drawn and
// `lasts` how long a pixel shows.
shader_type canvas_item;

uniform float now;
uniform float lasts = 1.0;
uniform float fall = 0.0;
uniform float flight = 0.3;

varying float left;
varying float threshold;

void vertex() {
	float age = clamp((now - INSTANCE_CUSTOM.x) / lasts, 0.0, 1.0);
	left = 1.0 - (now - INSTANCE_CUSTOM.x) / lasts;
	threshold = INSTANCE_CUSTOM.y;
	float flying = min(1.0, age / flight);
	float flown = 1.0 - (1.0 - flying) * (1.0 - flying);
	VERTEX += floor(INSTANCE_CUSTOM.zw * flown + vec2(0.0, fall * flying * flying) + 0.5);
}

void fragment() {
	if (left <= threshold) {
		discard;
	}
	COLOR = vec4(COLOR.rgb, 1.0);
}
