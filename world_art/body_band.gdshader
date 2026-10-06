// Draws each instance of a body band's MultiMesh from its own rectangle of the body atlas:
// INSTANCE_CUSTOM holds the rectangle's position and size, as fractions of the atlas.
shader_type canvas_item;

void vertex() {
	UV = INSTANCE_CUSTOM.xy + UV * INSTANCE_CUSTOM.zw;
}
