#version 330 core
layout (location = 0) in vec3 pos; // vertex position
layout (location = 1) in vec2 texCoord; // texture coordinates

out vec2 tc; // pass texture coordinates to fragment shader

uniform mat4 transformationMatrix; // transformation matrix uniform

void main() {
	tc = texCoord;
	gl_Position = transformationMatrix * vec4(pos, 1.0); // apply transformation matrix
}