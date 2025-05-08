#version 410 core

out vec4 FragColor;

uniform float u_Time;
uniform vec3 u_Color;

void main() {
    FragColor = vec4(0.7f*u_Color+0.05f*sin(u_Time/10000.0f),1.0f);
}