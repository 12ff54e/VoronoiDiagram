#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
layout(location = 2) in mat4 transMat;

out vec3 color;

void main() {
    color = aColor;
    gl_Position = transMat * vec4(aPos, 1.0);
}
