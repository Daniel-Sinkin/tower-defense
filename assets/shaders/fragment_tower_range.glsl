#version 410 core

out vec4 FragColor;

uniform float u_Time;
uniform vec3 u_Color;
uniform vec2 u_Pos;
uniform vec2 u_Resolution;
uniform float u_Radius;

void main() {
    vec3 color = vec3(0.1f, 0.7f, 0.1f);
    float alpha = 0.3f;
    FragColor = vec4(color, alpha);
}