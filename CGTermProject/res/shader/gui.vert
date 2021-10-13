#version 330
layout (location = 0) in vec2 aPosition;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec2 aTexCoord;

out vec4 color;
out vec2 texCoord;

uniform mat4 projection;

void main()
{
	color = aColor;
	texCoord = aTexCoord;
	gl_Position = projection * vec4(aPosition, 0.0, 1.0);
}