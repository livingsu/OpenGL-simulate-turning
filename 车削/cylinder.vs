#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;


out vec3 Normal;
out vec3 WorldPos;
out vec2 TexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
	WorldPos=vec3(model*vec4(aPos,1.0));
	TexCoords=aTexCoords;

	//法线向量需要转换到世界坐标系中
	Normal=mat3(transpose(inverse(model)))*aNormal; 
    

}