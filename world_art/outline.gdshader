// Draws the outline of each body something standing in front of it hides, over everything
// standing, as docs/art-direction.md §Bodies and markers asks: each instance is a body's outline
// frame in the body atlas, at INSTANCE_CUSTOM.xy, and the body's own frame at INSTANCE_CUSTOM.zw,
// both `frame_size` in size, and the instance colour's red is the share of white the body is drawn
// with as it flashes. An outline pixel shows only where the screen does not show the body's own
// pixel there, in the colour its band drew it, flashed or not, so the outline is drawn over what
// hides the body and not over the body itself.
shader_type canvas_item;

#include "res://world_art/flash.gdshaderinc"

uniform vec2 frame_size;
uniform sampler2D screen : hint_screen_texture, filter_nearest;

varying vec2 body_uv;
varying float white;

void vertex() {
	body_uv = INSTANCE_CUSTOM.zw + UV * frame_size;
	UV = INSTANCE_CUSTOM.xy + UV * frame_size;
	white = COLOR.r;
}

void fragment() {
	vec4 outline = texture(TEXTURE, UV);
	vec4 body = texture(TEXTURE, body_uv);
	vec3 drawn = flashed(white, body_uv / TEXTURE_PIXEL_SIZE) ? IMPACT : body.rgb;
	vec3 shown = texture(screen, SCREEN_UV).rgb;
	if (outline.a < 0.5 || (body.a > 0.5 && distance(drawn, shown) < 0.02)) {
		discard;
	}
	COLOR = outline;
}
