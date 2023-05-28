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
    float aa_width = float(2) / canvas_size.y;
    if(dist < elem_size + aa_width) {
        dist = smoothstep(elem_size, elem_size + aa_width, dist);
        frag_color = vec4(mix(color, frag_color.xyz, dist), 1.);
    }
}

void main() {
    float aspect_ratio = canvas_size.x / canvas_size.y;
    vec2 uv = (gl_FragCoord.xy / canvas_size) - vec2(.5, .5);
    uv.x *= aspect_ratio;
    // uv is normalized coordinate with y from -.5 to .5 and x from -a/2 to a/2, 
    // starting from lower-left corner, where a is aspect ratio.
    // (Note: it depends on the qualifier `origin_upper_left` and `pixel_center_integer` of gl_FragCoord)

    bool draw_site = bool(style & 1u);
    bool draw_frame = bool(style & 2u);
    bool draw_color = bool(style & 4u);

    vec3 background_color = color;
    frag_color = vec4(background_color, 1.0);

    float aa_width = 1. / canvas_size.y;
    float point_size = 0.02f / sqrt(float(site_array_size));

    uint indices[] = uint[3](0u, 0u, 0u);
    float dists[] = float[3](1., 1., 1.);
    for(uint i = 0u; i < site_array_size; ++i) {
        float dist = distance(uv, sites[i].pos);
        if(dist < point_size + aa_width && draw_site) {
            frag_color = vec4(sites[i].color, 1.); // set current block color
            render_smooth(dist, point_size, vec3(0.));
            return;
        } else if(dist < dists[2]) {
            dists[2] = dist;
            indices[2] = i;
            // bubble sort
            for(int j = 1; j >= 0; --j) {
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

    float dist = (dists[1] - dists[0]) * (dists[1] + dists[0]) / (2. * distance(sites[indices[0]].pos, sites[indices[1]].pos));
    float dist2 = (dists[2] - dists[0]) * (dists[1] + dists[0]) / (2. * distance(sites[indices[0]].pos, sites[indices[2]].pos));
    if(dist2 < dist) {
        indices[1] = indices[2];
        dist = dist2;
    }
    if(draw_frame) {
        dist = smoothstep(.5 * line_width - aa_width, .5 * line_width + aa_width, dist);
        frag_color = vec4(mix(line_color, draw_color ? sites[indices[0]].color : background_color, dist), 1.);
    } else {
        dist = smoothstep(-aa_width, aa_width, dist);
        frag_color = vec4(mix(sites[indices[1]].color, sites[indices[0]].color, dist), 1.);
    }
}
