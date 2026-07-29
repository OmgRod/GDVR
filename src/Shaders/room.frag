#version 320 es
precision highp float;

out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D gdTexture;

void main() {
    FragColor = texture(gdTexture, TexCoord);
}
