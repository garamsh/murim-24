// Value noise for what drifts over the world, the fog and the clouds' shadows: smooth from 0 to 1
// over a lattice of one unit, the same at the same point and salt, as ground.gdshader's.
uint noise_hash(int x, int y, int salt) {
	uint n = uint(x) * 374761393u + uint(y) * 668265263u + uint(salt) * 2246822519u;
	n = (n ^ (n >> 13u)) * 1274126177u;
	return n ^ (n >> 16u);
}

float noise_at(vec2 p, int salt) {
	vec2 i = floor(p);
	vec2 f = p - i;
	f = f * f * (3.0 - 2.0 * f);
	int x = int(i.x);
	int y = int(i.y);
	float a = float(noise_hash(x, y, salt) & 0xffffu) / 65536.0;
	float b = float(noise_hash(x + 1, y, salt) & 0xffffu) / 65536.0;
	float c = float(noise_hash(x, y + 1, salt) & 0xffffu) / 65536.0;
	float d = float(noise_hash(x + 1, y + 1, salt) & 0xffffu) / 65536.0;
	return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}
