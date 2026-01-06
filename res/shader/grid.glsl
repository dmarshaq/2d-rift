#version 430 core

#ifdef VERTEX

layout(location = 0) in vec2 position;
layout(location = 1) in float unit_scale;
layout(location = 2) in vec4 color;
layout(location = 3) in vec2 uv0;
layout(location = 4) in float grid_scale;

layout(location = 0) uniform mat4 pr_matrix;
layout(location = 4) uniform mat4 ml_matrix;

out float v_unit_scale;
out float v_grid_scale;
out vec4 v_color;
out vec2 v_uv0;

void main() {
    v_unit_scale = unit_scale;
    v_grid_scale = grid_scale;
    v_color = color;
    v_uv0 = uv0;

    // Dummy if, because we hate glsl :).
    if (color.x < 0) {
        gl_Position = pr_matrix * ml_matrix * vec4(position, 0.0, 1.0);
    } else {
        gl_Position = vec4(position, 0.0, 1.0);
    }

}

#endif

#ifdef FRAGMENT

#define PI 3.14159265359
#define AMP_SCALE 0.4

layout(location = 0) out vec4 color;

in float v_unit_scale;
in float v_grid_scale;
in vec4 v_color;
in vec2 v_uv0;

layout(location = 8) uniform sampler2D u_textures[32];

float ground(float x) {
    return (abs(x) + x) / 2;
}

float block(float x) {
    return -ground(-x);
}

void main() {
    color.xyz = v_color.xyz * 
        (
         (block(ground(1 - abs(sin(v_uv0.x * PI * v_grid_scale)) * v_unit_scale * AMP_SCALE / v_grid_scale) * v_unit_scale * AMP_SCALE - 1) + 1) + 
         (block(ground(1 - abs(sin(v_uv0.y * PI * v_grid_scale)) * v_unit_scale * AMP_SCALE / v_grid_scale) * v_unit_scale * AMP_SCALE - 1) + 1)
        );


    color.x += block(ground(1.0 - pow(abs(v_uv0.y * v_unit_scale * AMP_SCALE * 2), 0.5)) * v_unit_scale * AMP_SCALE * 2 - 1) + 1;
    color.y += block(ground(1.0 - pow(abs(v_uv0.x * v_unit_scale * AMP_SCALE * 2), 0.5)) * v_unit_scale * AMP_SCALE * 2 - 1) + 1;
    color.w = v_color.w;

}

#endif
