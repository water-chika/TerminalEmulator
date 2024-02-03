#version 460
#extension GL_EXT_mesh_shader : enable

layout(local_size_x_id=8,local_size_y_id=1) in;
layout(max_primitives=8*2, max_vertices=8*6) out;
layout(triangles) out;
void main(){
    gl_MeshVerticesEXT[0].gl_Position = vec4(gl_LocalInvocationID*0.1+vec3(1,0,0), 1);
    gl_MeshVerticesEXT[1].gl_Position = vec4(gl_LocalInvocationID*0.1+vec3(0,0,0), 1);
    gl_MeshVerticesEXT[2].gl_Position = vec4(gl_LocalInvocationID*0.1+vec3(0,1,0), 1);
    gl_PrimitiveTriangleIndicesEXT[0] = 0;
    gl_PrimitiveTriangleIndicesEXT[1] = 0;
    gl_PrimitiveTriangleIndicesEXT[2] = 0;
    SetMeshOutputsEXT(3, 1);
}
