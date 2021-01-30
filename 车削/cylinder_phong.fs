#version 330 core
struct Material {
    sampler2D	diffuse;
    vec3		specular;
    float		shininess;
}; 

struct Light {
    vec3 position;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

in vec3 Normal;
in vec3 WorldPos;
in vec2 TexCoords;

out vec4 FragColor;

uniform vec3 viewPos;
uniform Material material;
uniform Light light;

void main()
{    
	//环境光
	vec3 ambient=light.ambient*vec3(texture(material.diffuse,TexCoords));

	//漫反射光
	vec3 norm=normalize(Normal);
	vec3 lightDir=normalize(light.position-WorldPos);
	float diff=max(dot(norm,lightDir),0);
	vec3 diffuse=light.diffuse*diff*vec3(texture(material.diffuse,TexCoords));

	//镜面光
	vec3 viewDir=normalize(viewPos-WorldPos);
	vec3 reflectDir=reflect(-lightDir,norm); //反射光方向
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
	vec3 specular = light.specular*(spec*material.specular) ;

    FragColor = vec4(ambient+diffuse+specular, 1.0);
}