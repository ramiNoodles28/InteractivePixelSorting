#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D screenTexture; // This is your sorted pixel array

void main() {
    // Just look up the color from the sorted array
    FragColor = texture(screenTexture, TexCoords);
}