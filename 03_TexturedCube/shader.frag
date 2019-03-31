#version 450

layout(location=0) in vec4 inColor;
layout(location=1) in vec2 inUV;
layout(location=0) out vec4 outColor;

layout(binding=1) uniform sampler2D diffuseMap;

void main()
{
  outColor = inColor * texture(diffuseMap, inUV);
}
