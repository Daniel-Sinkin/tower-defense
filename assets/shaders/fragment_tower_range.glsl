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
    float time_mult = 1/500.0f;
    FragColor = vec4(color + 0.15f * sin(u_Time*2.0f*time_mult), alpha*(0.8f + 0.2f * sin(u_Time*time_mult)));
}