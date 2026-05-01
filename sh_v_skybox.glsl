#version 330

layout (location = 0) in vec3 aPos;
out vec3 TexCoords;
uniform mat4 projection;
uniform mat4 view;

void main() {
    TexCoords = aPos;
    // Remove translation from the view matrix
    mat4 staticView = mat4(mat3(view)); 
    vec4 pos = projection * staticView * vec4(aPos, 1.0);
    // Force depth to 1.0 (the very back) by setting z = w
    gl_Position = pos.xyww;
}