#version 300 es
precision mediump float;
precision mediump usampler2D;

#define MAX_ARRAY_SIZE 4096

in vec3 color;
out uvec4 new_state;

uniform vec2 canvas_size;
uniform uint site_array_size;
uniform ivec2 step_size;
uniform usampler2D board;

struct Site {
    vec2 pos;
    vec3 color;
};

layout(std140) uniform user_data {
    Site sites[MAX_ARRAY_SIZE];
};

// the texture state is interpreted as follows:
//  r,g store coordinate, b store index and infers empty when it equals 0.
// TODO: Improve to JFA^2
void main() {
    // 1st pass, fill texture with sites
    if(ivec2(canvas_size) == step_size) {
        for(int i = 0; i < int(site_array_size); ++i) {
            if(distance(sites[i].pos * canvas_size, gl_FragCoord.xy) < .707107) {
                new_state.rg = uvec2(gl_FragCoord.xy);
                new_state.b = uint(i + 1);
                return;
            }
        }
        new_state.b = 0u;
        return;
    }
    float dist = 2. * max(canvas_size.x, canvas_size.y);
    for(int i = -1; i <= 1; ++i) {
        for(int j = -1; j <= 1; ++j) {
            ivec2 coord = ivec2(gl_FragCoord) + ivec2(i, j) * step_size;
            // out of boundary check
            if(coord.x < 0 || coord.x >= int(canvas_size.x) || coord.y < 0 || coord.y >= int(canvas_size.y)) {
                continue;
            }
            // sample
            uvec4 state = texelFetch(board, coord, 0);
            if(state.b == 0u) {
                continue;
            }
            float local_dist = distance(vec2(state.rg) + vec2(.5), gl_FragCoord.xy);
            if(local_dist < dist) {
                dist = local_dist;
                new_state = state;
            }
        }
    }
}
