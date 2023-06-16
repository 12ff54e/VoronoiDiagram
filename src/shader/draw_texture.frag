#version 300 es
precision mediump float;
precision mediump usampler2D;

#define MAX_ARRAY_SIZE 4096

in vec3 color;
out vec4 frag_color;

uniform vec2 canvas_size;
uniform usampler2D board;

struct Site {
    vec2 pos;
    vec3 color;
};

layout(std140) uniform user_data {
    Site sites[MAX_ARRAY_SIZE];
};

void main() {
    uint idx = texelFetch(board, ivec2(gl_FragCoord), 0).b;
    frag_color = vec4(sites[idx - 1u].color, 1.);
}
