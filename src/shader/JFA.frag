#version 300 es
precision mediump float;
precision mediump usampler2D;

#define MAX_ARRAY_SIZE 4096

in vec3 color;
out uvec4 new_state;

uniform vec2 canvas_size;
uniform uint site_array_size;
uniform ivec2 step_size;
uniform usampler2D site_info;
uniform usampler2D board;

// the texture state is interpreted as follows:
//  r,g store coordinate, b store index and infers empty when it equals 0.
// TODO: Improve to JFA^2
void main() {
    // 1st pass, fill texture with sites
    if(ivec2(canvas_size) == step_size) {
        for(uint i = 0u; i < site_array_size; ++i) {
            vec2 site_pos = vec2(texelFetch(site_info, ivec2(i, 0), 0).pq);
            if(distance(site_pos, gl_FragCoord.xy - vec2(.5)) < .01) {
                new_state.rg = uvec2(gl_FragCoord.xy);
                new_state.b = i + 1u;
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
