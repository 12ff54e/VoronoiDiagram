#version 300 es
precision mediump float;
precision highp usampler2D;

in vec3 color;
// GLES do not support rendering to float point format, thus output is bit representation of a float
out uvec4 new_sites;

uniform vec2 canvas_size;
uniform uint site_num;
uniform uint patches_per_quad;
uniform uint patches_per_line;
uniform uint reduction_step;
uniform usampler2D site_info;
uniform usampler2D board;
uniform usampler2D prev_sites; // First row stores intermediate result for convergence test

void add_uint_bits_of_float(inout uint s, in uint a) {
    s = floatBitsToUint(uintBitsToFloat(s) + uintBitsToFloat(a));
}

void add_uint_bits_of_float(inout uvec2 s, in uvec2 a) {
    s = floatBitsToUint(uintBitsToFloat(s) + uintBitsToFloat(a));
}

// assumes quadrant and patches are square
void get_patch_coord(in vec2 site_pos, in uint patch_id, out ivec2 bottom_left, out ivec2 top_right) {
    uint quad_size = uint(2. * sqrt(canvas_size.x * canvas_size.y / float(site_num)));
    quad_size += (patches_per_line - quad_size % patches_per_line); // make quad_size divisible by patches_per_line
    uint quad_id = patch_id / patches_per_quad;
    bottom_left.y = int((patch_id % patches_per_quad) / patches_per_line);
    bottom_left.x = int(patch_id - quad_id * patches_per_quad) - bottom_left.y * int(patches_per_line);
    bottom_left *= ivec2(quad_size / patches_per_line);
    bottom_left += ivec2(site_pos) - ivec2((1u - quad_id % 2u), (1u - quad_id / 2u)) * int(quad_size);
    top_right = bottom_left + ivec2(quad_size / patches_per_line);
}

void main() {
    uint reduction_num = uint(ceil(log2(float(4u * patches_per_quad))));
    int step_size_base = int(ceil(pow(float(site_num), 1. / float(reduction_num))));
    uint site_id = uint(gl_FragCoord.x);

    if(reduction_step == 0u) {
        if(gl_FragCoord.y >= 1.) {
            uint patch_id = uint(gl_FragCoord.y) - 1u;
            // sum texels in current patch
            vec2 pos_acc = vec2(0);
            uint count = 0u;

            // using ivec2 since this coordinate might be negative (out of bound)
            ivec2 bottom_left;
            ivec2 top_right;
            vec2 site_pos = vec2(texelFetch(site_info, ivec2(site_id, 0), 0).zw);
            get_patch_coord(site_pos, patch_id, bottom_left, top_right);
            for(int x = max(bottom_left.x, 0); x < min(top_right.x, int(canvas_size.x)); ++x) {
                for(int y = max(bottom_left.y, 0); y < min(top_right.y, int(canvas_size.y)); ++y) {
                    uvec4 texel = texelFetch(board, ivec2(x, y), 0); // z refers to 1-based site id
                    if(texel.z == site_id + 1u) {
                        pos_acc += vec2(x, y);
                        count += 1u;
                    }

                }
            }

            new_sites.xy = floatBitsToUint(pos_acc);
            new_sites.z = count;
        } else {
            // use the 1st row to store intermediate result for convergence test
            uvec4 texel = texelFetch(site_info, ivec2(site_id, 0), 0);
            vec2 prev_pos = vec2(texel.xy);
            vec2 current_pos = vec2(texel.zw);
            new_sites.x = floatBitsToUint(pow(distance(prev_pos, current_pos), 2.));
        }
    } else if(reduction_step < reduction_num) {
        if(gl_FragCoord.y >= 1.) {
            uint patch_id = uint(gl_FragCoord.y) - 1u;

            // reduce patches to calculate centroid position
            uint align = 1u << reduction_step;
            if(patch_id % align == 0u) {
                new_sites = texelFetch(prev_sites, ivec2(site_id, patch_id + 1u), 0);
                if(patch_id + align / 2u < 4u * patches_per_quad) {
                    uvec4 texel = texelFetch(prev_sites, ivec2(site_id, patch_id + 1u + align / 2u), 0);
                    new_sites.z += texel.z;
                    add_uint_bits_of_float(new_sites.xy, texel.xy);
                }
            }
        } else {
            int step_size = int(pow(float(step_size_base), float(reduction_step - 1u)));

            new_sites.x = 0u;
            if(int(site_id) % step_size == 0) {
                uint j;
                for(int i = 0; i < step_size_base; ++i) {
                    if((j = site_id + uint(i * step_size)) >= site_num) {
                        break;
                    }
                    add_uint_bits_of_float(new_sites.x, texelFetch(prev_sites, ivec2(j, gl_FragCoord.y), 0).x);
                }
            }
        }
    } else {
        // last step
        if(int(gl_FragCoord.y) == 1) {
            // store previous and new position at 2nd row, as xy and zw component, in uint.
            new_sites.xy = texelFetch(site_info, ivec2(site_id, 0), 0).zw;
            uvec4 a1 = texelFetch(prev_sites, ivec2(site_id, 1), 0);
            uvec4 a2 = texelFetch(prev_sites, ivec2(site_id, 1 + 1 << (reduction_step - 1u)), 0);
            new_sites.zw = uvec2(roundEven((uintBitsToFloat(a1.xy) + uintBitsToFloat(a2.xy)) / float(a1.z + a2.z)));
        } else if(ivec2(gl_FragCoord.xy) == ivec2(0, 0)) {
            // rms of moving distance of each site
            int step_size_base = int(ceil(log(float(site_num)) / log(2. * float(patches_per_quad))));
            int step_size = int(pow(float(step_size_base), float(reduction_step - 1u)));
            uint j;
            for(int i = 0; i < step_size_base; ++i) {
                if((j = site_id + uint(i * step_size)) >= site_num) {
                    break;
                }
                add_uint_bits_of_float(new_sites.x, texelFetch(prev_sites, ivec2(j, gl_FragCoord.y), 0).x);
            }
            new_sites.x = uint(10. * sqrt(uintBitsToFloat(new_sites.x) / float(site_num)));
        }
    }
}
