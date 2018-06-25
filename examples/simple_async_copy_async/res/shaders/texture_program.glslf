#version 440 core
#extension GL_ARB_separate_shader_objects : enable 
#extension GL_NV_gpu_shader5 : enable
        
in vec2 tex_coord;
uniform sampler2D in_texture;
layout(location = 0) out vec4 out_color;
void main()
{
    out_color = texelFetch(in_texture, ivec2(gl_FragCoord.xy), 0).rgba;
}