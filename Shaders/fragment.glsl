#version 460

out vec4 FragColor;

uniform double xmin;
uniform double xmax;
uniform double ymin;
uniform double ymax;
uniform double scalex;
uniform double scaley;
float max_n = 255.;

vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
	vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
   	return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);	
}

void main() {
	double x0 = xmin + floor(gl_FragCoord.x) * scalex;
	double y0 = ymin + floor(gl_FragCoord.y) * scaley;

	double x2 = 0.;
	double y2 = 0.;
	double x = 0.;
	double y = 0.;
	int n = 0;
	while (n < max_n && x2 + y2 < 4) {
		y = 2 * x * y + y0;
		x = x2 - y2 + x0;
		x2 = x * x;
		y2 = y * y;
		n++;
	}

	float z = n + 1 - log(log2(abs(sqrt(float(x2) + float(y2)))));	

    float hue = (1.0 * z) / max_n;
    float saturation = 1.0;
    float value = z < max_n ? 1.0 : 0.0;

	float col = n / max_n;
	FragColor = vec4(hsv2rgb(vec3(hue, saturation, value)), 1.0); 
}
