// How lit each art pixel of the screen is, drawn once a frame into grade.tscn's light field, a
// viewport of one pixel an art pixel, for grade.gdshader to read: in red, how near the nearest of
// the lights is, 1 on its rect and falling to 0 at its reach; in green, 1 where the art pixel lies
// in a lit window's rect. Drawn at the art's pixels rather than the screen's, the lights cost an
// art pixel's share of the screen's.
shader_type canvas_item;

// The screen pixels an art pixel spans, and each light's rect, x, y, width and height, and how far
// its light reaches, in screen pixels.
uniform float art_pixel = 1.0;
uniform int light_count = 0;
uniform vec4 light_rects[32];
uniform float light_reach[32];

void fragment() {
	vec2 at = FRAGCOORD.xy * art_pixel;
	float lit = 0.0;
	float pane = 0.0;
	for (int i = 0; i < light_count; i++) {
		vec4 r = light_rects[i];
		vec2 off = max(max(r.xy - at, at - (r.xy + r.zw)), vec2(0.0));
		lit = max(lit, 1.0 - length(off) / light_reach[i]);
		if (r.z > 0.0 && at.x >= r.x && at.y >= r.y && at.x < r.x + r.z && at.y < r.y + r.w) {
			pane = 1.0;
		}
	}
	COLOR = vec4(clamp(lit, 0.0, 1.0), pane, 0.0, 1.0);
}
