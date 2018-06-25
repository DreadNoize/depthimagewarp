#version 440 core
#extension GL_ARB_separate_shader_objects : enable 
#extension GL_NV_gpu_shader5 : enable

uniform mat4 mvp;
out vec2 tex_coord;
layout(location = 0) in vec3 in_position;
layout(location = 2) in vec2 in_texture_coord;
void main()
{
    gl_Position = mvp * vec4(in_position, 1.0);
    tex_coord = in_texture_coord;
}