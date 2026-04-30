#version 330

smooth in vec3 fragNorm;	// Interpolated model-space normal
smooth in vec3 fragPos;

out vec3 fragCol;	// Final pixel color

uniform vec3 lightPos;

void main() {
	vec3 normColor = normalize(fragNorm) * 0.5f + 0.5f;
	
	float ambientStrength = 0.2;
    vec3 ambient = ambientStrength * vec3(1.0, 1.0, 1.0);

	vec3 norm = normalize(fragNorm);
    vec3 lightDir = normalize(lightPos - fragPos);

	float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * vec3(1.0, 1.0, 1.0); // White light

	// Visualize normals as colors
	fragCol = (ambientStrength + diffuse) * normColor;
}