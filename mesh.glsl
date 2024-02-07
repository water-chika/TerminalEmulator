#version 460
#extension GL_EXT_mesh_shader : enable

const int width = 32;
const int height = 1;
const float grid_width = 2.0 / width;

const int char_num = 9;
const float tex_width = 1.0 / char_num;
const float tex_advance = tex_width;

layout(local_size_x=width/4,local_size_y=height) in;
layout(max_primitives=width*height*2, max_vertices=width*height*6) out;
layout(triangles) out;

layout(std140, binding=1) uniform char_indices {
    uvec4 indices[width/4*height];
}tex_indices;

layout(location=0) out vec2 coord[];

void set_pos(uint index, vec2 pos) {
    gl_MeshVerticesEXT[index].gl_Position = vec4(pos, 0, 1);
}
void set_coord(uint index, vec2 c) {
    coord[index] = c;
}

void draw_char(uint index, vec2 pos, uint primitive_index, uint vertex_index) {
    vec2 pos1 = pos + vec2(1,0)*grid_width;
    vec2 pos2 = pos + vec2(0,1)*grid_width;
    vec2 pos3 = pos + vec2(1,1)*grid_width;
    float tex_offset = tex_width * index;
    set_pos(vertex_index+0, pos); set_coord(vertex_index+0, vec2(tex_offset,0));
    set_pos(vertex_index+1, pos1); set_coord(vertex_index+1, vec2(tex_offset+tex_advance,0));
    set_pos(vertex_index+2, pos2); set_coord(vertex_index+2, vec2(tex_offset,1));
    gl_PrimitiveTriangleIndicesEXT[primitive_index] = uvec3(vertex_index,vertex_index+1,vertex_index+2);
    set_pos(vertex_index+3, pos1); set_coord(vertex_index+3, vec2(tex_offset+tex_advance,0));
    set_pos(vertex_index+4, pos2); set_coord(vertex_index+4, vec2(tex_offset,1));
    set_pos(vertex_index+5, pos3); set_coord(vertex_index+5, vec2(tex_offset+tex_advance,1));
    gl_PrimitiveTriangleIndicesEXT[primitive_index+1] = uvec3(vertex_index+3,vertex_index+4,vertex_index+5);

}

void main(){
    vec2 pos = (4*gl_LocalInvocationID.xy) * grid_width + vec2(-1.0, -1.0);
    uvec4 char_4_indices = tex_indices.indices[gl_LocalInvocationID.x];
    uint char_indices[4] = {char_4_indices.x,char_4_indices.y,char_4_indices.z,char_4_indices.w};
    for (int i = 0; i < 4; i++){
        draw_char(char_indices[i], pos+vec2(grid_width*i,0),   gl_LocalInvocationID.x*4*2+i*2,   gl_LocalInvocationID.x*6*4+i*6);
    }

    SetMeshOutputsEXT(width*height*2*3, width*height*2);
}
