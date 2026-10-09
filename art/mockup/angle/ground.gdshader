// The valley's ground drawn off the cell grid (#212), for the 2.5D mockup of #200: a chunk's
// image, rendered once when the map is built, from `cells`, one texel a cell: its ground kind in
// red, its level plus one in green, the material of the face it drops by in blue.
// For each pixel of the screen, as the world would be drawn unlifted from `origin`:
// - the cells' values are read through coordinates warped by noise and interpolated between
//   neighbouring cells, so edges and cliffs run along curves and not along the grid;
// - looking down the column from the highest ground that could show at the pixel, the first
//   point that shows at or above it is what is seen: its ground, or the face dropping from it;
// - ground kinds take a 4-step ramp each (`ramps`), varied per pixel in clusters, with large
//   patches of colour, a dithered band where two kinds meet, and rims: foam and shallows by the
//   shore, a trodden edge by lanes, shade at a forest's edge, a lit lip on a cliff's top and a
//   shadow at its foot; faces are drawn in strata, courses or treads by their material.
// With `pass` 1 it draws, in place of colour, what each pixel is: its sort row in red, as an
// offset from `row_from`, its level plus two in green, its kind (or 20 plus the face material)
// in blue. Pixels of rows outside [row_from, row_to) are left clear: another chunk draws them.
shader_type canvas_item;
render_mode unshaded, blend_disabled;

uniform sampler2D cells : filter_nearest;
uniform ivec2 map_size;
uniform vec2 origin;
uniform vec2 image_size;
uniform int row_from;
uniform int row_to;
uniform int pass_kind;
uniform vec3 ramps[44];

const float CW = 16.0;
const float CD = 12.0;
const float LP = 16.0;
const int TOP = 5;
const int LOW = -1;
const float WARP = 7.0;
const int WATER = 0;
const int PLAIN = 1;
const int MEADOW = 2;
const int FOREST = 3;
const int ROCK = 4;
const int LANE = 5;
const int PAVING = 6;
const int PADDY = 7;
const int GARDEN = 8;
const int STEPS = 9;
const int EARTH = 10;
const vec3 SHADE = vec3(0.078, 0.094, 0.157);

uint mix_hash(int x, int y, int salt) {
	uint n = uint(x) * 374761393u + uint(y) * 668265263u + uint(salt) * 2246822519u;
	n = (n ^ (n >> 13u)) * 1274126177u;
	return n ^ (n >> 16u);
}

float hash01(int x, int y, int salt) {
	return float(mix_hash(x, y, salt) & 0xffffu) / 65536.0;
}

float vnoise(vec2 p, int salt) {
	vec2 i = floor(p);
	vec2 f = p - i;
	f = f * f * (3.0 - 2.0 * f);
	int x = int(i.x);
	int y = int(i.y);
	float a = hash01(x, y, salt);
	float b = hash01(x + 1, y, salt);
	float c = hash01(x, y + 1, salt);
	float d = hash01(x + 1, y + 1, salt);
	return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p, int salt) {
	float sum = 0.0;
	float amp = 0.5;
	for (int i = 0; i < 3; i++) {
		sum += amp * vnoise(p, salt + i);
		p *= 2.03;
		amp *= 0.5;
	}
	return sum / 0.875;
}

float bayer(ivec2 p) {
	int m[16] = int[](0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5);
	return (float(m[(p.y & 3) * 4 + (p.x & 3)]) + 0.5) / 16.0;
}

ivec3 cell(ivec2 c) {
	c = clamp(c, ivec2(0), map_size - 1);
	vec4 t = texelFetch(cells, c, 0);
	return ivec3(int(round(t.r * 255.0)), int(round(t.g * 255.0)) - 1, int(round(t.b * 255.0)));
}

// Where a point on the ground reads its cells from: warped by noise, so edges wander.
vec2 warped(vec2 p) {
	return p + (vec2(fbm(p / 22.0 + vec2(7.0, 0.0), 11), fbm(p / 22.0 + vec2(0.0, 5.0), 12)) - 0.5) * 2.0 * WARP;
}

// The level of the ground at a point: its cells' levels interpolated, then taken to the nearest
// level one of those cells has, so the contour curves between levels without a ledge at a level
// none of them has.
int level_at(vec2 p) {
	vec2 q = warped(p);
	vec2 uv = vec2(q.x / CW, q.y / CD) - 0.5;
	ivec2 c = ivec2(floor(uv));
	vec2 f = uv - floor(uv);
	float corners[4] = float[](float(cell(c).y), float(cell(c + ivec2(1, 0)).y), float(cell(c + ivec2(0, 1)).y),
			float(cell(c + ivec2(1, 1)).y));
	float level = mix(mix(corners[0], corners[1], f.x), mix(corners[2], corners[3], f.x), f.y);
	float best = corners[0];
	for (int i = 1; i < 4; i++) {
		float here = abs(corners[i] - level);
		float there = abs(best - level);
		if (here < there || (here == there && corners[i] > best)) {
			best = corners[i];
		}
	}
	return int(best);
}

// The kind of ground at a point on level `level`: of the four cells round it, the kind whose
// cells weigh most, those on the level first; a little noise in the weights dithers the band
// where two kinds are nearly even.
int kind_at(vec2 p, int level) {
	vec2 q = warped(p);
	vec2 uv = vec2(q.x / CW, q.y / CD) - 0.5;
	ivec2 c = ivec2(floor(uv));
	vec2 f = uv - floor(uv);
	ivec3 cs[4] = ivec3[](cell(c), cell(c + ivec2(1, 0)), cell(c + ivec2(0, 1)), cell(c + ivec2(1, 1)));
	float w[4] = float[]((1.0 - f.x) * (1.0 - f.y), f.x * (1.0 - f.y), (1.0 - f.x) * f.y, f.x * f.y);
	int best = cs[0].x;
	float best_score = -1.0;
	ivec2 px = ivec2(floor(p));
	for (int i = 0; i < 4; i++) {
		float score = 0.0;
		for (int j = 0; j < 4; j++) {
			if (cs[j].x == cs[i].x) {
				score += w[j] * (cs[j].y == level ? 1.0 : 0.25);
			}
		}
		score += (hash01(px.x, px.y, 31 + cs[i].x) - 0.5) * 0.14;
		if (score > best_score) {
			best_score = score;
			best = cs[i].x;
		}
	}
	return best;
}

vec3 ramp(int kind, int step) {
	return ramps[kind * 4 + clamp(step, 0, 3)];
}

// Within `r` pixels of `p` on its level, is there ground of another kind than `kind`?
bool near_other(vec2 p, int level, int kind, int r) {
	for (int dy = -r; dy <= r; dy += r) {
		for (int dx = -r; dx <= r; dx += r) {
			if ((dx != 0 || dy != 0) && kind_at(p + vec2(float(dx), float(dy)), level) != kind) {
				return true;
			}
		}
	}
	return false;
}

bool near_kind(vec2 p, int level, int other, int r) {
	for (int dy = -r; dy <= r; dy += r) {
		for (int dx = -r; dx <= r; dx += r) {
			if (kind_at(p + vec2(float(dx), float(dy)), level) == other) {
				return true;
			}
		}
	}
	return false;
}

vec3 surface(vec2 p, int level, int kind) {
	ivec2 px = ivec2(floor(p));
	float tex = 0.62 * vnoise(p / 2.3 + 5.0, 21) + 0.38 * hash01(px.x, px.y, 22);
	float macro = (fbm(p / 48.0 + vec2(11.0, 3.0), 23) - 0.5) * 0.5;
	int step = tex < 0.3 + macro * 0.5 ? 0 : (tex > 0.74 + macro * 0.5 ? 2 : 1);
	if (kind == WATER) {
		step = vnoise(p / 9.0, 24) > 0.55 ? 1 : 0;
		if (near_other(p, level, WATER, 1)) {
			step = 3;
		} else if (near_other(p, level, WATER, 3)) {
			step = 2;
		}
	} else if (kind == PAVING || kind == STEPS) {
		int course = px.y / 6;
		int sx = (px.x + (course % 2) * 4) % 8;
		step = (sx == 0 || px.y % 6 == 0) ? 0 : (hash01((px.x + (course % 2) * 4) / 8, course, 25) > 0.7 ? 2 : 1);
		if (kind == STEPS) {
			step = px.y % 4 == 0 ? 3 : (px.y % 4 == 3 ? 0 : 1);
		}
	} else if (kind == GARDEN) {
		step = px.y % 4 < 2 ? 0 : ((px.x % 3 == 0) ? 3 : 2);
	} else if (kind == PADDY) {
		step = ((px.x + px.y % 2) % 4 == 0) ? 3 : (tex > 0.7 ? 2 : 1);
	} else if (kind == LANE) {
		if (near_other(p, level, LANE, 1)) {
			step = 0;
		}
	} else if (kind == FOREST) {
		if (near_other(p, level, FOREST, 2)) {
			step = 0;
		}
	} else if ((kind == PLAIN || kind == MEADOW) && near_kind(p, level, LANE, 1)) {
		step = 0;
	}
	// Rock wears moss and lichen in patches.
	if (kind == ROCK && fbm(p / 18.0 + vec2(3.0, 9.0), 30) > 0.6 && bayer(px) < 0.5) {
		return ramp(FOREST, step == 2 ? 2 : 1);
	}
	vec3 colour = ramp(kind, step);
	// The lit lip where the ground turns down into a face.
	if (level_at(p + vec2(0.0, 1.0)) < level) {
		colour = ramp(kind == ROCK ? ROCK : (kind == WATER ? WATER : MEADOW), 3);
	}
	// The foot of a face is in its shadow, thinning away from it.
	for (int d = 1; d <= 4; d++) {
		if (level_at(p - vec2(0.0, float(d))) > level) {
			if (bayer(px) < (1.0 - float(d - 1) / 4.0) * 0.9) {
				colour = mix(colour, SHADE, 0.38);
			}
			break;
		}
	}
	return colour;
}

// A face's colour by its material, `depth` pixels below its top and `height` pixels high.
vec3 face(vec2 p, int material, float depth, float height) {
	ivec2 px = ivec2(int(floor(p.x)), int(depth));
	int step = 1;
	int kind = ROCK;
	if (material == 1) {
		kind = EARTH;
		step = depth < 2.0 ? 3 : ((int(depth) % 3 == 0) ? 0 : 1);
	} else if (material == 2) {
		kind = PAVING;
		int course = int(depth) / 4;
		step = (int(depth) % 4 == 3 || (px.x + course * 4) % 8 == 0) ? 0 : (int(depth) % 4 == 0 ? 2 : 1);
	} else if (material == 3) {
		kind = PAVING;
		step = int(depth) % 4 == 0 ? 3 : (int(depth) % 4 == 3 ? 0 : 1);
	} else {
		// Strata a few pixels thick, wandering along the face, split by vertical cracks.
		float band = depth + (vnoise(vec2(p.x / 9.0, 0.0), 26) - 0.5) * 4.0;
		int layer = int(floor(band / 3.0));
		step = (layer % 3 == 0) ? 3 : ((layer % 3 == 1) ? 2 : 1);
		if (hash01(px.x / 2, layer, 27) > 0.82) {
			step = max(step - 1, 0);
		}
		if (hash01(px.x, 0, 28) > 0.9 && vnoise(vec2(p.x, depth / 4.0), 29) > 0.4) {
			step = 0;
		}
	}
	if (depth < 1.0) {
		step = 3;
	} else if (depth > height - 3.0) {
		step = 0;
	} else if (material == 0 && depth > height * 0.6) {
		step = max(step - 1, 0);
	}
	return ramp(kind, step);
}

void fragment() {
	vec2 pixel = floor(UV * image_size);
	float x = origin.x + pixel.x + 0.5;
	float s = origin.y + pixel.y + 0.5;
	// Down the column from the highest ground that could show here to the lowest: the first
	// point that shows at or above this pixel is what is seen. Exactly here, it is the ground
	// itself; above, the pixel is on the face that drops from that point's edge.
	int level = 0;
	float ground_y = s;
	float shown = 0.0;
	for (int step_y = 0; step_y <= (TOP - LOW) * int(LP); step_y++) {
		float y = s + float(TOP) * LP - float(step_y);
		int l = level_at(vec2(x, y));
		float at = y - float(l) * LP;
		if (at <= s + 0.5) {
			level = l;
			ground_y = y;
			shown = at;
			break;
		}
	}
	vec4 out_colour = vec4(0.0);
	int sort_row = 0;
	int kind = 0;
	if (shown > s - 0.5) {
		kind = kind_at(vec2(x, ground_y), level);
		sort_row = int(floor(ground_y / CD));
		out_colour = vec4(surface(vec2(x, ground_y), level, kind), 1.0);
	} else {
		int foot = min(level_at(vec2(x, ground_y + 1.0)), level - 1);
		float depth = s - shown;
		float height = float(level - foot) * LP;
		vec2 q = warped(vec2(x, ground_y));
		int material = cell(ivec2(floor(vec2(q.x / CW, q.y / CD)))).z;
		kind = 20 + material;
		sort_row = int(floor(ground_y / CD));
		out_colour = vec4(face(vec2(x, s), material, depth, height), 1.0);
	}
	if (sort_row < row_from || sort_row >= row_to) {
		COLOR = vec4(0.0);
	} else if (pass_kind == 1) {
		COLOR = vec4(float(sort_row - row_from + 1) / 255.0, float(level + 2) / 255.0, float(kind) / 255.0, 1.0);
	} else {
		COLOR = out_colour;
	}
}
