#version 300 es
precision mediump float;

in vec3 color;
out vec4 frag_color;

void main() {
    frag_color = vec4(color, 1.);
}
