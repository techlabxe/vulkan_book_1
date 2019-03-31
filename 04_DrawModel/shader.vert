#version 450

layout(location=0) in vec3 inPos;
layout(location=1) in vec3 inNormal;
layout(location=2) in vec2 inUV;
layout(location=0) out vec2 outUV;

layout(binding=0) uniform Matrices
{
  mat4 world;
  mat4 view;
  mat4 proj;
};

out gl_PerVertex
{
  vec4 gl_Position;
};

void main()
{
  mat4 pvw = proj * view * world;
  gl_Position = pvw * vec4(inPos, 1.0);
  outUV = inUV;
}
