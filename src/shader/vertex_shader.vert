#version 300 es
layout(location = 0) in vec2 a_position;
layout(location = 1) in vec3 a_color;

out vec3 color;

void main() {
    color = a_color;
    gl_Position = vec4(a_position, 0.0, 1.0);
}
