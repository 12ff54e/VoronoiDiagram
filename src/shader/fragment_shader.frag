#version 300 es
precision mediump float;

#define MAX_ARRAY_SIZE 4096

uniform vec2 canvas_size;
uniform uint site_array_size;

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

    vec3 background_color = color;
    frag_color = vec4(background_color, 1.0);

    float aa_width = float(2) / canvas_size.y;
    float point_size = 0.02f / sqrt(float(site_array_size));

    uint nearest_site_index = 0u;
    uint second_nearest_site_index = 0u;
    float shortest_dist = 1.;
    float second_shortest_dist = 1.;
    for(uint i = 0u; i < site_array_size; ++i) {
        float dist = distance(uv, sites[i].pos);
        if(dist < point_size + aa_width) {
            frag_color = vec4(sites[i].color,1.); // set current block color
            render_smooth(dist, point_size, vec3(0.));
            return;
        } else if(dist < shortest_dist) {
            second_shortest_dist = shortest_dist;
            second_nearest_site_index = nearest_site_index;
            shortest_dist = dist;
            nearest_site_index = i;
        } else if(dist < second_shortest_dist) {
            second_shortest_dist = dist;
            second_nearest_site_index = i;
        }
    }

    float dist = (second_shortest_dist - shortest_dist) * (second_shortest_dist + shortest_dist) / (2. * distance(sites[nearest_site_index].pos, sites[second_nearest_site_index].pos));
    dist = smoothstep(-aa_width, aa_width, dist);
    frag_color = vec4(mix(sites[second_nearest_site_index].color, sites[nearest_site_index].color, dist), 1.);
}
