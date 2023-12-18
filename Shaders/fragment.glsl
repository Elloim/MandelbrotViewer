#version 460

out vec4 FragColor;

uniform double xmin;
uniform double xmax;
uniform double ymin;
uniform double ymax;
uniform double scalex;
uniform double scaley;
int max_n = 255;

int mandelbrotFunc(long double complex* c, int max_n) {

	int n = 0;
	long double x2 = 0.;
	long double y2 = 0.;
	long double x = 0.;
	long double y = 0.;

	long double x0 = creall(*c);
	long double y0 = cimagl(*c);

	while (n < max_n && x2 + y2 < 4) {
		y = 2 * x * y + y0;
		x = x2 - y2 + x0;
		x2 = x * x;
		y2 = y * y;
		n++;
	}

	*c = x + y * I;
	return n;
}

void main() {
	double x = xmin + floor(gl_FragCoord.x) * scalex;
	double y = xmax + floor(gl_FragCoord.y) * scaley;
	if (floor(gl_FragCoord.x) == 2) {
		FragColor = vec4(1., 1., 1., 1.);
	}
	else {
		FragColor = vec4(0., 0., 0., 1.);
	}
}
