#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(binding = 0) uniform Uniforms {
  mat4 projection_matrix;
  mat4 camera_matrix;
  mat4 world_matrix;
  vec4 eye_pos;
  vec4 light_pos;
  float light_energy;
} uniforms;

layout (location = 0) in vec3 input_position;
layout (location = 1) in vec3 input_normal;
layout (location = 2) in vec3 input_tangent;
layout (location = 3) in vec2 input_texcoord;
layout (location = 0) out vec3 output_position;
layout (location = 1) out vec3 output_normal;
layout (location = 2) out vec3 output_tangent;
layout (location = 3) out vec2 output_texcoord;
out gl_PerVertex
{
    vec4 gl_Position;
};

void main() {
  // ワールド座標系での頂点の位置を求めてフラグメントシェーダに送っておく
  vec4 local_pos = vec4( input_position.xyz, 1.0 );
  vec4 pos = uniforms.world_matrix * local_pos;
  output_position = pos.xyz;
  // 法線もワールド座標系での向きに変換してフラグメントシェーダに送る
  output_normal = normalize( ( mat3(uniforms.world_matrix) * input_normal ) );
  // 接線もワールド座標系での向きに変換してフラグメントシェーダに送る
  output_tangent = normalize( ( mat3(uniforms.world_matrix) * input_tangent ) );
  // 頂点に設定されたテクスチャ座標をそのままフラグメントシェーダに送る
  output_texcoord = input_texcoord;
  // ワールド座標系での頂点の座標を画面を基準とする座標系での座標に変換する
  gl_Position = uniforms.projection_matrix * uniforms.camera_matrix * pos;
}

