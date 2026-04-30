#version 330 core
layout (location = 0) in vec2 aPos;       // Flat XY coordinates
layout (location = 1) in vec2 aTexCoords; // Where we are on the texture (0 to 1)

out vec2 TexCoords;

void main() {
    // No xform matrix! We just pass the coordinates directly.
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0); 
    TexCoords = aTexCoords;
}