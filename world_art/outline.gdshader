// Draws the outline of each body something standing in front of it hides, over everything
// standing, as docs/art-direction.md §Bodies and markers asks: each instance is a body's outline
// frame in the body atlas, at INSTANCE_CUSTOM.xy, and the body's own frame at INSTANCE_CUSTOM.zw,
// both `frame_size` in size. An outline pixel shows only where the screen does not show the body's
// own pixel there, so the outline is drawn over what hides the body and not over the body itself.
shader_type canvas_item;

uniform vec2 frame_size;
uniform sampler2D screen : hint_screen_texture, filter_nearest;

varying vec2 body_uv;

void vertex() {
	body_uv = INSTANCE_CUSTOM.zw + UV * frame_size;
	UV = INSTANCE_CUSTOM.xy + UV * frame_size;
}

void fragment() {
	vec4 outline = texture(TEXTURE, UV);
	vec4 body = texture(TEXTURE, body_uv);
	vec3 shown = texture(screen, SCREEN_UV).rgb;
	if (outline.a < 0.5 || (body.a > 0.5 && distance(body.rgb, shown) < 0.02)) {
		discard;
	}
	COLOR = outline;
}
