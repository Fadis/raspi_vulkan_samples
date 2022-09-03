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
layout (location = 2) in vec3 input_color;
layout (location = 0) out vec4 output_color;


vec3 eotf( vec3 v ) {
  return min( max( v / (v + 0.155 ) * 1.019, vec3( 0, 0, 0 ) ), vec3( 1, 1, 1 ) );
}

void main()  {
  const float pi = 3.141592653589793;
  // 現在の位置から光源の位置への向きを表す単位長のベクトルを求める
  vec3 light_dir = normalize( uniforms.light_pos.xyz - input_position );
  // ラスタライザで補間された法線は長さが単位長になっていない可能性があるので正規化する
  vec3 normal = normalize( input_normal );
  // ランバートモデルで拡散を求める
  vec3 diffuse = input_color * max( dot( light_dir, normal ), 0 ) /pi;
  // 光が届かない部分が完全な黒になると不自然なので少しだけ明るくする(環境光)
  vec3 ambient = input_color * 0.01;
  // 光のエネルギーをsRGB色空間での色に変換して出力する
  output_color = vec4( eotf( ( diffuse + ambient ) * uniforms.light_energy ), 1.0 );
}


