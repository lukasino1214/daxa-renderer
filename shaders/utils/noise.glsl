#pragma once

f32 rand(f32vec2 c) {
	return fract(sin(dot(c.xy, f32vec2(12.9898, 78.233))) * 43758.5453);
}

f32 noise(f32vec2 p, f32 freq) {
	f32 unit = 2560 / freq;
	f32vec2 ij = floor(p / unit);
	f32vec2 xy = mod(p, unit) / unit;
	xy = .5 * (1. - cos(3.14159265359 * xy));
	f32 a = rand((ij + f32vec2(0., 0.)));
	f32 b = rand((ij + f32vec2(1., 0.)));
	f32 c = rand((ij + f32vec2(0., 1.)));
	f32 d = rand((ij + f32vec2(1., 1.)));
	f32 x1 = mix(a, b, xy.x);
	f32 x2 = mix(c, d, xy.x);
	return mix(x1, x2, xy.y);
}

f32 pNoise(f32vec2 p, i32 res) {
	f32 persistance = .5;
	f32 n = 0.;
	f32 normK = 0.;
	f32 f = 4.;
	f32 amp = 1.;
	i32 iCount = 0;
	for (i32 i = 0; i < 50; i++) {
		n += amp * noise(p, f);
		f *= 2.;
		normK += amp;
		amp *= persistance;
		if (iCount == res) break;
		iCount++;
	}
	f32 nf = n / normK;
	return nf * nf * nf * nf;
}