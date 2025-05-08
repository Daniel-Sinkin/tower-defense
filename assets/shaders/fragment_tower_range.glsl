#version 410 core

out vec4 FragColor;

uniform float u_Time;
uniform vec3 u_Color;
uniform vec2 u_Pos;
uniform vec2 u_Resolution;
uniform float u_Radius;

void main() {
    // Get screen-space coordinates normalized to [0,1]
    vec2 uv = gl_FragCoord.xy / u_Resolution;

    // Distance from current fragment to the center
    float dist = distance(uv, u_Pos);

    // Animate radius over time (looping)
    float speed = 0.5;
    float thickness = 0.01;
    float radius = fract(u_Time * speed) * u_Radius;

    // Compute smooth ring
    float ring = smoothstep(radius - thickness, radius, dist)
               - smoothstep(radius, radius + thickness, dist);

    vec3 color = u_Color * ring;
    float alpha = ring;

    FragColor = vec4(color, alpha);
}