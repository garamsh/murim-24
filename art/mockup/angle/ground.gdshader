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
uniform vec3 ramps[56];
// A region's air: mist lying in its low ground, in this colour, up to `mist.a` of the way.
uniform vec4 mist = vec4(0.0);
// Whether rock wears moss; a dry or snowy region's does not.
uniform bool moss = true;
// Whether slopes are lit as they face, so ground that rises without a cliff, as dunes do, shows
// its shape.
uniform bool relief = false;
// Whether water runs, streaked along its current.
uniform bool flow = false;

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
const int SAND = 11;
const int SNOW = 12;
const int WHEAT = 13;
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

// A cell: its kind, its level, its face material, and 1 where its ground rises as a slope.
ivec4 cell(ivec2 c) {
	c = clamp(c, ivec2(0), map_size - 1);
	vec4 t = texelFetch(cells, c, 0);
	return ivec4(int(round(t.r * 255.0)), int(round(t.g * 255.0)) - 1, int(round(t.b * 255.0)), t.a < 0.75 ? 1 : 0);
}

// Where a point on the ground reads its cells from: warped by noise, so edges wander.
vec2 warped(vec2 p) {
	return p + (vec2(fbm(p / 22.0 + vec2(7.0, 0.0), 11), fbm(p / 22.0 + vec2(0.0, 5.0), 12)) - 0.5) * 2.0 * WARP;
}

// The height of the ground at a point, in levels: its cells' levels interpolated, then taken to
// the nearest level one of those cells has, so the contour curves between levels without a
// ledge at a level none of them has; toward cells that rise as a slope, the interpolated height
// itself, so the ground climbs without a cliff.
float height_at(vec2 p) {
	vec2 q = warped(p);
	vec2 uv = vec2(q.x / CW, q.y / CD) - 0.5;
	ivec2 c = ivec2(floor(uv));
	vec2 f = uv - floor(uv);
	ivec4 cs[4] = ivec4[](cell(c), cell(c + ivec2(1, 0)), cell(c + ivec2(0, 1)), cell(c + ivec2(1, 1)));
	float level = mix(mix(float(cs[0].y), float(cs[1].y), f.x), mix(float(cs[2].y), float(cs[3].y), f.x), f.y);
	// How far the point is into a slope: the corners' flags interpolated, so a cliff fades into
	// the slope it meets instead of breaking off at a cell's edge.
	float slope = mix(mix(float(cs[0].w), float(cs[1].w), f.x), mix(float(cs[2].w), float(cs[3].w), f.x), f.y);
	float best = float(cs[0].y);
	for (int i = 1; i < 4; i++) {
		float here = abs(float(cs[i].y) - level);
		float there = abs(best - level);
		if (here < there || (here == there && float(cs[i].y) > best)) {
			best = float(cs[i].y);
		}
	}
	return mix(best, level, smoothstep(0.0, 0.5, slope));
}

int level_at(vec2 p) {
	return int(floor(height_at(p) + 0.5));
}

// Whether the ground drops by a cliff, not a slope, between `p` and `q` south of it.
bool drops(vec2 p, vec2 q) {
	return height_at(p) - height_at(q) > 0.5;
}

// The kind of ground at a point on level `level`: of the four cells round it, the kind whose
// cells weigh most, those on the level first; a little noise in the weights dithers the band
// where two kinds are nearly even.
int kind_at(vec2 p, int level) {
	vec2 q = warped(p);
	vec2 uv = vec2(q.x / CW, q.y / CD) - 0.5;
	ivec2 c = ivec2(floor(uv));
	vec2 f = uv - floor(uv);
	ivec4 cs[4] = ivec4[](cell(c), cell(c + ivec2(1, 0)), cell(c + ivec2(0, 1)), cell(c + ivec2(1, 1)));
	float w[4] = float[]((1.0 - f.x) * (1.0 - f.y), f.x * (1.0 - f.y), (1.0 - f.x) * f.y, f.x * f.y);
	int best = cs[0].x;
	float best_score = -1.0;
	// The jitter is per pixel of the image, not of the ground, so a slope facing the viewer does
	// not stretch it into streaks.
	ivec2 px = ivec2(floor(vec2(p.x, p.y - height_at(p) * LP)));
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

// The ground's colour at `p` on the ground, `screen` being where it shows: texture and patches
// follow the screen, so a slope facing the viewer is not streaked by its stretch; kinds, rims and
// laid patterns follow the ground.
vec3 surface(vec2 p, vec2 screen, int level, int kind) {
	ivec2 px = ivec2(floor(p));
	ivec2 sx = ivec2(floor(screen));
	float tex = 0.62 * vnoise(screen / 2.3 + 5.0, 21) + 0.38 * hash01(sx.x, sx.y, 22);
	float macro = (fbm(screen / 48.0 + vec2(11.0, 3.0), 23) - 0.5) * 0.5;
	int step = tex < 0.3 + macro * 0.5 ? 0 : (tex > 0.74 + macro * 0.5 ? 2 : 1);
	if (kind == WATER) {
		step = vnoise(p / 9.0, 24) > 0.55 ? 1 : 0;
		if (flow) {
			float streak = fract((p.y + (vnoise(vec2(p.x / 40.0, p.y / 9.0), 46) - 0.5) * 8.0) / 4.0);
			step = streak < 0.16 && vnoise(p / vec2(14.0, 3.0), 47) > 0.42 ? 3 : (vnoise(p / vec2(18.0, 5.0), 24) > 0.5 ? 1 : 0);
		}
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
	} else if (kind == SAND) {
		// Ripples the wind laid across the sand, wavering, lit on their windward side.
		float ripple = fract((p.y + p.x * 0.35 + (vnoise(p / 14.0, 42) - 0.5) * 10.0) / 5.0);
		step = ripple < 0.2 ? 3 : (ripple < 0.7 ? 2 : (tex > 0.6 ? 1 : 2));
		if (macro < -0.12) {
			step = max(step - 1, 1);
		}
	} else if (kind == SNOW) {
		// Snow lies smooth, blue in its hollows.
		step = tex < 0.18 + macro ? 1 : (tex > 0.8 ? 3 : 2);
	} else if (kind == WHEAT) {
		// Rows of wheat across each field, ears catching the light.
		step = px.y % 3 == 0 ? 1 : (hash01(px.x, px.y, 43) > 0.7 ? 3 : 2);
		if (near_other(p, level, WHEAT, 1)) {
			step = 0;
		}
	}
	// Rock wears moss and lichen in patches.
	if (moss && kind == ROCK && fbm(screen / 18.0 + vec2(3.0, 9.0), 30) > 0.58 && vnoise(screen / 1.7, 31) > 0.45) {
		return ramp(FOREST, step == 2 ? 2 : 1);
	}
	if (relief && kind != WATER) {
		// Lit from the upper left: rising eastward faces the light, rising northward turns from it.
		float tilt = height_at(p + vec2(3.0, 0.0)) - height_at(p - vec2(3.0, 0.0)) + 0.6 * (height_at(p + vec2(0.0, 3.0)) - height_at(p - vec2(0.0, 3.0)));
		step = clamp(step + (tilt > 0.12 ? 1 : 0) - (tilt < -0.12 ? 1 : 0), 0, 3);
	}
	vec3 colour = ramp(kind, step);
	// The lit lip where the ground turns down into a face.
	if (drops(p, p + vec2(0.0, 1.0))) {
		colour = ramp(kind == ROCK || kind >= SAND ? kind : (kind == WATER ? WATER : MEADOW), 3);
	}
	// The foot of a face is in its shadow, thinning away from it, and scree lies there in
	// clusters, more of it nearer the face.
	for (int d = 1; d <= 6; d++) {
		if (drops(p - vec2(0.0, float(d)), p - vec2(0.0, float(d - 1)))) {
			if (kind != WATER && vnoise(p / 2.0, 32) > 0.35 + float(d) * 0.08 && hash01(px.x, px.y, 33) > 0.35) {
				colour = ramp(ROCK, hash01(px.x, px.y, 34) > 0.6 ? 3 : 1);
			}
			if (d <= 4 && hash01(px.x, px.y, 35) < (1.0 - float(d - 1) / 4.0) * 0.9) {
				colour = mix(colour, SHADE, 0.38);
			}
			break;
		}
	}
	return colour;
}

// A face's colour by its material, `depth` pixels below its top and `height` pixels high.
// `lip` is the kind of the ground above, which hangs over the lip.
vec3 face(vec2 p, int material, float depth, float height, int lip) {
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
		// Rock: strata of uneven thickness that tilt and wander along the face, each a tone of
		// its own with a lit top and a dark underside, split by fissures.
		float thickness = 2.5 + 3.0 * vnoise(vec2(p.x / 27.0, 1.0), 26);
		float band = (depth + (vnoise(vec2(p.x / 11.0, 0.0), 26) - 0.5) * 6.0 + (vnoise(vec2(p.x / 37.0, 3.0), 36) - 0.5) * 10.0) / thickness;
		int layer = int(floor(band));
		float within = fract(band);
		step = 1 + int(hash01(layer, int(p.x / 40.0), 27) * 2.99);
		if (within < 0.2) {
			step = min(step + 1, 3);
		} else if (within > 0.8) {
			step = max(step - 1, 0);
		}
		if (hash01(px.x / 3, 0, 28) > 0.9 && depth > height * vnoise(vec2(p.x, 7.0), 29)) {
			step = 0;
		}
		// Plants hang over the lip in tufts, and scree piles at the foot to an uneven height.
		float hang = vnoise(vec2(p.x / 4.0, 9.0), 37) > 0.55 ? 4.0 * vnoise(vec2(p.x / 2.0, 1.0), 38) : 0.0;
		if (depth < hang) {
			if (lip == SNOW || lip == SAND) {
				return ramp(lip, depth < 1.0 ? 3 : 2);
			}
			if (!moss) {
				return ramp(ROCK, depth < 1.0 ? 3 : 2);
			}
			return ramp(FOREST, depth < 1.0 ? 2 : 1);
		}
		float pile = 2.0 + 9.0 * vnoise(vec2(p.x / 13.0, 5.0), 39);
		if (depth > height - pile) {
			return ramp(ROCK, hash01(px.x, int(depth), 40) > 0.55 ? 3 : (hash01(px.x / 2, int(depth) / 2, 41) > 0.5 ? 1 : 0));
		}
		return ramp(ROCK, depth < 1.0 ? 3 : step);
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
		float h = height_at(vec2(x, y));
		float at = y - h * LP;
		if (at <= s + 0.5) {
			// On a slope one step of ground moves the image by several pixels; where the ground
			// runs on without a drop, the point that shows exactly here lies between the steps.
			float h_next = height_at(vec2(x, y + 1.0));
			float at_next = y + 1.0 - h_next * LP;
			if (at < s - 0.5 && at_next > s && abs(h_next - h) < 0.5) {
				float t = (s - at) / (at_next - at);
				y += t;
				h = mix(h, h_next, t);
				at = s;
			}
			level = int(floor(h + 0.5));
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
		out_colour = vec4(surface(vec2(x, ground_y), vec2(x, s), level, kind), 1.0);
	} else {
		float top_h = height_at(vec2(x, ground_y));
		float foot_h = min(height_at(vec2(x, ground_y + 1.0)), top_h - 0.5);
		float depth = s - shown;
		float height = (top_h - foot_h) * LP;
		vec2 q = warped(vec2(x, ground_y));
		int material = cell(ivec2(floor(vec2(q.x / CW, q.y / CD)))).z;
		// An earth bank above the first level is rock, so a face keeps one material along its run.
		if (material == 1 && level >= 2) {
			material = 0;
		}
		kind = 20 + material;
		sort_row = int(floor(ground_y / CD));
		out_colour = vec4(face(vec2(x, s), material, depth, height, kind_at(vec2(x, ground_y - 1.0), level)), 1.0);
	}
	// Mist in the low ground, in drifting banks.
	if (mist.a > 0.0) {
		float low = level <= 0 ? 1.0 : (level == 1 ? 0.5 : 0.0);
		float bank = fbm(vec2(x, s) / vec2(90.0, 30.0) + vec2(17.0, 4.0), 44);
		float amount = mist.a * low * smoothstep(0.5, 0.8, bank);
		if (amount > vnoise(vec2(x, s) / 1.7, 45) * 0.5) {
			out_colour.rgb = mix(out_colour.rgb, mist.rgb, clamp(amount * 1.4, 0.0, 0.75));
		}
	}
	if (sort_row < row_from || sort_row >= row_to) {
		COLOR = vec4(0.0);
	} else if (pass_kind == 1) {
		COLOR = vec4(float(sort_row - row_from + 1) / 255.0, float(level + 2) / 255.0, float(kind) / 255.0, 1.0);
	} else {
		COLOR = out_colour;
	}
}
