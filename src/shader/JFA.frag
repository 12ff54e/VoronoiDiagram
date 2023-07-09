#version 300 es
precision mediump float;
precision mediump usampler2D;

#define MAX_ARRAY_SIZE 4096

in vec3 color;
out uvec4 new_state;

uniform vec2 canvas_size;
uniform uint site_num;
uniform uint style;
uniform uint FSAA_factor;
uniform ivec2 step_size;
uniform usampler2D site_info;
uniform usampler2D board;

void dist_periodic(in vec2 p1, in vec2 p2, in bool periodic_x, in bool periodic_y, out float dist) {
    uint metric = (style >> 5u) & 3u;
    dist = max(canvas_size.x, canvas_size.y);
    vec2 MASS_canvas_size = float(FSAA_factor) * canvas_size;
    for(int i = -int(periodic_x); i <= int(periodic_x); ++i) {
        for(int j = -int(periodic_y); j <= int(periodic_y); ++j) {
            float d;
            vec2 p2_dummy = p2 + MASS_canvas_size * vec2(i, j);
            switch(metric) {
                case 0u: // Manhattan
                    d = dot(abs(p1 - p2_dummy), vec2(1));
                    break;
                case 1u: // Euclidean
                    d = distance(p1, p2_dummy);
                    break;
                case 2u: // Min
                    p2_dummy = abs(p1 - p2_dummy);
                    d = min(p2_dummy.x, p2_dummy.y);
                    break;
                case 3u: // Max
                    p2_dummy = abs(p1 - p2_dummy);
                    d = max(p2_dummy.x, p2_dummy.y);
                    break;
            }
            dist = min(dist, d);
        }
    }
}

// the texture state is interpreted as follows:
//  r,g store coordinate, b store index and infers empty when it equals 0.
// TODO: Improve to JFA^2
void main() {
    bool periodic_x = bool(style & (1u << 3u));
    bool periodic_y = bool(style & (1u << 4u));

    ivec2 MASS_canvas_size = int(FSAA_factor) * ivec2(canvas_size);

    // 1st pass, fill texture with sites
    if(MASS_canvas_size == step_size) {
        for(uint i = 0u; i < site_num; ++i) {
            vec2 site_pos = float(FSAA_factor) * vec2(texelFetch(site_info, ivec2(i, 0), 0).pq);
            if(distance(site_pos, gl_FragCoord.xy - vec2(.5)) < .01) {
                new_state.rg = uvec2(gl_FragCoord.xy);
                new_state.b = i + 1u;
                return;
            }
        }
        new_state.b = 0u;
        return;
    }
    float dist = 2. * float(max(MASS_canvas_size.x, MASS_canvas_size.y));
    for(int i = -1; i <= 1; ++i) {
        for(int j = -1; j <= 1; ++j) {
            ivec2 coord = ivec2(gl_FragCoord) + ivec2(i, j) * step_size;
            coord = (coord + MASS_canvas_size) % MASS_canvas_size;
            // sample
            uvec4 state = texelFetch(board, coord, 0);
            if(state.b == 0u) {
                continue;
            }
            float local_dist;
            dist_periodic(vec2(state), gl_FragCoord.xy - vec2(.5), periodic_x, periodic_y, local_dist);
            if(local_dist < dist) {
                dist = local_dist;
                new_state = state;
            }
        }
    }
}
