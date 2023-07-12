#version 300 es
precision mediump float;

in vec3 color;
out vec4 frag_color;

uniform vec2 canvas_size;
uniform uint site_num;
uniform uint style;

// calculate color such that a smooth transition appears when dist is slightly larger than width
void render_smooth(float dist, float elem_size, vec3 color) {
    float aa_width = 1. / canvas_size.y;
    if(dist < elem_size + aa_width) {
        dist = smoothstep(elem_size, elem_size + aa_width, dist);
        frag_color = mix(vec4(color, 1.), frag_color, dist);
    }
}

void main() {
    bool draw_site = bool(style & (1u << 0u));
    float point_size = 0.01f / sqrt(sqrt(float(site_num)));

    frag_color = vec4(color, 1.);
    if(draw_site) {
        render_smooth(gl_FragCoord.z * length(canvas_size) / canvas_size.y, point_size, vec3(0.));
    }
}
