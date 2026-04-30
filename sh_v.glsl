#version 330

layout(location = 0) in vec3 pos;		// Model-space position
layout(location = 1) in vec3 norm;		// Model-space normal

smooth out vec3 fragNorm;	// Model-space interpolated normal
smooth out vec3 fragPos;

uniform mat4 xform;			// Model-to-clip space transform
uniform mat4 model;

void main() {
	// Calculate the pixel's position in world space
    fragPos = vec3(model * vec4(pos, 1.0));

	// Transform normals to world space (using a normal matrix to handle rotation)
	fragNorm = mat3(transpose(inverse(model))) * norm;

	// Transform vertex position
	gl_Position = xform * vec4(pos, 1.0);
}