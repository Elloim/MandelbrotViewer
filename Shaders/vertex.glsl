#version 460

in vec2 position;

void main() {
	gl_Position = vec4(position, 0., 1.);
}
