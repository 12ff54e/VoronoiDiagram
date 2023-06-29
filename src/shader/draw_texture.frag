#version 300 es
precision mediump float;
precision mediump usampler2D;

#define MAX_ARRAY_SIZE 4096

in vec3 color;
out vec4 frag_color;

uniform vec2 canvas_size;
uniform uint site_num;
uniform uint style;
uniform uint FSAA_factor;
uniform usampler2D site_info;
uniform usampler2D board;

// calculate color such that a smooth transition appears when dist is slightly larger than width
void render_smooth(float dist, float elem_size, vec3 color) {
    float aa_width = 1.;
    if(dist < elem_size + aa_width) {
        dist = smoothstep(elem_size, elem_size + aa_width, dist);
        frag_color = mix(vec4(color, 1.), frag_color, dist);
    }
}

void main() {
    bool draw_site = bool(style & (1u << 0u));

    vec3 color = vec3(0);
    bool interior = true;
    uint idx;
    uvec3 texel;
    for(int i = 0; i < int(FSAA_factor); ++i) {
        for(int j = 0; j < int(FSAA_factor); ++j) {
            texel = texelFetch(board, ivec2(gl_FragCoord) * int(FSAA_factor) + ivec2(i, j), 0).stp;
            if(i == 0 && j == 0) {
                idx = texel.p - 1u;
            } else {
                if(texel.p - 1u != idx) {
                    interior = false;
                }
            }
            color += vec3(texelFetch(site_info, ivec2(texel.p - 1u, 1), 0)) / float(0xffff);

        }
    }

    color /= float(FSAA_factor * FSAA_factor);
    frag_color = vec4(color, 1.);

    float point_size = canvas_size.y * 0.01f / sqrt(sqrt(float(site_num)));
    if(draw_site && interior) {
        render_smooth(distance(gl_FragCoord.xy, vec2(texel.st / FSAA_factor)), point_size, vec3(0, 0, 0));
    }
}
