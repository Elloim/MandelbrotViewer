#version 460

out vec4 FragColor;

uniform double xmin;
uniform double xmax;
uniform double ymin;
uniform double ymax;
uniform double scalex;
uniform double scaley;
float max_n = 500.;

void main() {
	double x0 = xmin + floor(gl_FragCoord.x) * scalex;
	double y0 = ymin + floor(gl_FragCoord.y) * scaley;

	double x2 = 0.;
	double y2 = 0.;
	double x = 0.;
	double y = 0.;
	float n = 0;
	while (n < max_n && x2 + y2 < 4) {
		y = 2 * x * y + y0;
		x = x2 - y2 + x0;
		x2 = x * x;
		y2 = y * y;
		n += 1;
	}
	float col = n / max_n;
	FragColor = vec4(col, col, col, 1.);

}
