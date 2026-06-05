#version 330 core

layout (location = 0) in vec3 aPos;

out vec3 localDirection;

uniform mat4 view;
uniform mat4 projection;

void main()
{
	localDirection = aPos;
	vec4 clipPos = projection * view * vec4(aPos * 50.0, 1.0);
	gl_Position = clipPos.xyww;
}
