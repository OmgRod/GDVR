#pragma once

const char* CUBE_VERT_SHADER = R"(
#version 450 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

out vec2 TexCoord;
out vec3 FragPos;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    gl_Position = projection * view * vec4(FragPos, 1.0);
    TexCoord = aTexCoord;
}
)";

const char* ROOM_FRAG_SHADER = R"(
#version 450 core
out vec4 FragColor;
in vec2 TexCoord;
in vec3 FragPos;

uniform sampler2D gdTexture;

void main() {
    FragColor = texture(gdTexture, TexCoord);
}
)";

const char* VOID_FRAG_SHADER = R"(
#version 450 core
out vec4 FragColor;
in vec3 FragPos;

void main() {
    float dist = abs(FragPos.y);
    float fade = smoothstep(2.0, 5.0, dist);
    FragColor = vec4(0.0, 0.0, 0.0, fade);
}
)";
