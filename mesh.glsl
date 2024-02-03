#version 460
#extension GL_EXT_mesh_shader : enable

layout(local_size_x=8,local_size_y=1) in;
layout(max_primitives=8*2, max_vertices=8*6) out;
layout(triangles) out;
void main(){
    vec2 pos = gl_LocalInvocationID.xy * 0.1;
    vec2 pos1 = pos + vec2(1,0)*0.1;
    vec2 pos2 = pos + vec2(0,1)*0.1;
    gl_MeshVerticesEXT[3*gl_LocalInvocationIndex + 0].gl_Position = vec4(pos,0,1);
    gl_MeshVerticesEXT[3*gl_LocalInvocationIndex + 1].gl_Position = vec4(pos1,0,1);
    gl_MeshVerticesEXT[3*gl_LocalInvocationIndex + 2].gl_Position = vec4(pos2,0,1);
    gl_PrimitiveTriangleIndicesEXT[gl_LocalInvocationIndex + 0] = uvec3(3*gl_LocalInvocationIndex,3*gl_LocalInvocationIndex+1,3*gl_LocalInvocationIndex+2);
    uint work_group_size = gl_WorkGroupSize.x;
    SetMeshOutputsEXT(3*work_group_size, work_group_size);
}
