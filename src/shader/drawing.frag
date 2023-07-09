#version 300 es
precision mediump float;
precision mediump usampler2D;

#define MAX_ARRAY_SIZE 4096

uniform vec2 canvas_size;
uniform uint site_num;
uniform uint style;
uniform float line_width;
uniform vec3 line_color;
uniform usampler2D site_info;

in vec3 color;
out vec4 frag_color;

// calculate color such that a smooth transition appears when dist is slightly larger than width
void render_smooth(float dist, float elem_size, vec3 color) {
    float aa_width = 1. / canvas_size.y;
    if(dist < elem_size + aa_width) {
        dist = smoothstep(elem_size, elem_size + aa_width, dist);
        frag_color = mix(vec4(color, 1.), frag_color, dist);
    }
}

void dist_periodic(in vec2 p1, in vec2 p2, in bool periodic_x, in bool periodic_y, out float dist) {
    uint metric = (style >> 5u) & 3u;
    dist = max(canvas_size.x, canvas_size.y);
    for(int i = -int(periodic_x); i <= int(periodic_x); ++i) {
        for(int j = -int(periodic_y); j <= int(periodic_y); ++j) {
            float d;
            vec2 p2_dummy = p2 + canvas_size * vec2(i, j);
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

void main() {
    bool draw_site = bool(style & (1u << 0u));
    bool draw_frame = bool(style & (1u << 1u));
    bool draw_color = bool(style & (1u << 2u));
    bool periodic_x = bool(style & (1u << 3u));
    bool periodic_y = bool(style & (1u << 4u));

    vec3 background_color = color;
    frag_color = vec4(background_color, 0.);

    float aa_width = 1. / canvas_size.y;
    float point_size = 0.01f / sqrt(sqrt(float(site_num)));

    // find N nearest sites
    const uint N = 4u;
    uint indices[N];
    float dists[N];
    // init 
    for(uint i = 0u; i < N; ++i) {
        dists[i] = 1.;
    }
    // iterate over sites
    for(uint i = 0u; i < site_num; ++i) {
        vec2 site_pos = vec2(texelFetch(site_info, ivec2(i, 0), 0).pq);
        vec3 site_color = vec3(texelFetch(site_info, ivec2(i, 1), 0)) / float(0xffffu);
        float dist;
        dist_periodic(gl_FragCoord.xy, site_pos, periodic_x, periodic_y, dist);
        dist /= canvas_size.y;
        float euclidean_dist = distance(gl_FragCoord.xy, site_pos) / canvas_size.y;
        if(euclidean_dist < point_size + aa_width && draw_site) {
            frag_color = vec4(site_color, 1.); // set current block color
            render_smooth(euclidean_dist, point_size, vec3(0.));
            return;
        } else if(dist < dists[N - 1u]) {
            dists[N - 1u] = dist;
            indices[N - 1u] = i;
            // bubble sort
            for(int j = int(N) - 2; j >= 0; --j) {
                if(dists[j + 1] < dists[j]) {
                    float dd = dists[j];
                    uint ii = indices[j];
                    dists[j] = dists[j + 1];
                    dists[j + 1] = dd;
                    indices[j] = indices[j + 1];
                    indices[j + 1] = ii;
                }
            }
        }
    }

    for(uint i = 1u; i < N; ++i) {
        vec2 site0_pos = vec2(texelFetch(site_info, ivec2(indices[0], 0), 0).pq);
        vec2 site1_pos = vec2(texelFetch(site_info, ivec2(indices[i], 0), 0).pq);
        float site_dist;
        dist_periodic(site0_pos, site1_pos, periodic_x, periodic_y, site_dist);
        site_dist /= canvas_size.y;
        dists[i] = (dists[i] - dists[0]) * (dists[i] + dists[0]) / (2. * site_dist);
        if(dists[i] < dists[1]) {
            dists[1] = dists[i];
            indices[1] = indices[i];
        }
    }
    float dist = dists[1];
    vec3 site0_color = vec3(texelFetch(site_info, ivec2(indices[0], 1), 0)) / (float(0xffffu));
    vec3 site1_color = vec3(texelFetch(site_info, ivec2(indices[1], 1), 0)) / (float(0xffffu));
    if(draw_frame) {
        dist = smoothstep(.5 * line_width - aa_width, .5 * line_width + aa_width, dist);
        frag_color = mix(vec4(line_color, 1.), draw_color ? vec4(site0_color, 1.) : frag_color, dist);
    } else {
        dist = smoothstep(-aa_width, aa_width, dist);
        frag_color = vec4(mix(site1_color, site0_color, dist), 1.);
    }
}
