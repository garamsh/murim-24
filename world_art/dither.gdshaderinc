// An ordered dither, so that what fades or tints draws whole palette pixels and blends nothing
// (docs/art-direction.md §Palette): a pixel shows where its share exceeds the threshold the 4x4
// Bayer matrix gives its place.
float dither_threshold(vec2 pixel) {
	const float BAYER[16] = float[](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0, 3.0, 11.0, 1.0, 9.0,
			15.0, 7.0, 13.0, 5.0);
	ivec2 place = ivec2(mod(floor(pixel), 4.0));
	return (BAYER[place.y * 4 + place.x] + 0.5) / 16.0;
}
