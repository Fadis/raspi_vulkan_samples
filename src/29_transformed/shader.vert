#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(binding = 0) uniform Uniforms {
  mat4 projection_matrix;
  mat4 camera_matrix;
  mat4 world_matrix;
} uniforms;

layout (location = 0) in vec3 input_position;
layout (location = 2) in vec3 input_color;
layout (location = 0) out vec3 output_color;
out gl_PerVertex
{
    vec4 gl_Position;
};

void main() {
  // ワールド座標系での頂点の座標を求める
  vec4 local_pos = vec4( input_position.xyz, 1.0 );
  vec4 pos = uniforms.world_matrix * local_pos;
  // 頂点に設定された色はそのままフラグメントシェーダに送る
  output_color = input_color;
  // ワールド座標系での頂点の座標を画面を基準とする座標系での座標に変換する
  gl_Position = uniforms.projection_matrix * uniforms.camera_matrix * pos;
}

