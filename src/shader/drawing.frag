#version 300 es
precision mediump float;

#define MAX_ARRAY_SIZE 4096

uniform vec2 canvas_size;
uniform uint site_array_size;
uniform uint style;
uniform float line_width;
uniform vec3 line_color;

struct Site {
    vec2 pos;
    vec3 color;
};

layout(std140) uniform user_data {
    Site sites[MAX_ARRAY_SIZE];
};

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

void main() {
    float aspect_ratio = canvas_size.x / canvas_size.y;
    vec2 uv = gl_FragCoord.xy / canvas_size * vec2(aspect_ratio, 1.);
    // uv is normalized coordinate with y from 0. to 1. and x from 0. to a, 
    // starting from lower-left corner, where a is aspect ratio.
    // (Note: this behaviour depends on the qualifier `origin_upper_left` and `pixel_center_integer` of gl_FragCoord)

    bool draw_site = bool(style & 1u);
    bool draw_frame = bool(style & 2u);
    bool draw_color = bool(style & 4u);

    vec3 background_color = color;
    frag_color = vec4(background_color, 0.);

    float aa_width = 1. / canvas_size.y;
    float point_size = 0.02f / sqrt(float(site_array_size));

    // find N nearest sites
    const uint N = 4u;
    uint indices[N];
    float dists[N];
    // init 
    for(uint i = 0u; i < N; ++i) {
        dists[i] = 1.;
    }
    // iterate over sites
    for(uint i = 0u; i < site_array_size; ++i) {
        float dist = distance(uv, sites[i].pos * vec2(aspect_ratio, 1.));
        if(dist < point_size + aa_width && draw_site) {
            frag_color = vec4(sites[i].color, 1.); // set current block color
            render_smooth(dist, point_size, vec3(0.));
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
        float site_dist = distance(sites[indices[0]].pos * vec2(aspect_ratio, 1.), sites[indices[i]].pos * vec2(aspect_ratio, 1.));
        dists[i] = (dists[i] - dists[0]) * (dists[i] + dists[0]) / (2. * site_dist);
        if(dists[i] < dists[1]) {
            dists[1] = dists[i];
            indices[1] = indices[i];
        }
    }
    float dist = dists[1];
    if(draw_frame) {
        dist = smoothstep(.5 * line_width - aa_width, .5 * line_width + aa_width, dist);
        frag_color = mix(vec4(line_color, 1.), draw_color ? vec4(sites[indices[0]].color, 1.) : frag_color, dist);
    } else {
        dist = smoothstep(-aa_width, aa_width, dist);
        frag_color = vec4(mix(sites[indices[1]].color, sites[indices[0]].color, dist), 1.);
    }
}
