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

layout(binding = 1) uniform sampler2D base_color;
layout(binding = 2) uniform sampler2D normal_map;
layout(binding = 3) uniform sampler2D roughness_map;

layout (location = 0) in vec3 input_position;
layout (location = 1) in vec3 input_normal;
layout (location = 2) in vec3 input_tangent;
layout (location = 3) in vec2 input_texcoord;
layout (location = 0) out vec4 output_color;

float fresnel( vec3 V, vec3 N ) {
  float c = 1 - clamp( dot( V, N ), 0, 1 );
  float c2 = c * c;
  return c2 * c2 * c;
}

float GGX_D( vec3 N, vec3 H, float roughness ) {
  const float pi = 3.141592653589793;
  float a2 = roughness * roughness;
  float NH = max( dot( N, H ), 0 );
  float t = 1 + ( a2 - 1 )* NH;
  float NH2 = NH*NH;
  float t1 = tan( acos( NH ) );
  float t2 = roughness*roughness + t1 * t1;
  return roughness*roughness/(pi*NH2*NH2*t2*t2);
}

float GGX_G1( vec3 V, vec3 N, float roughness ) {
  float VN = max( dot( V, N ), 0 );
  float t = tan( acos( VN ) );
  float l = ( sqrt(roughness * roughness + ( 1 - roughness * roughness ) * t * t )/VN - 1 )/2;
  return 1/(1 + l);
}

float GGX_G2( vec3 L, vec3 V, vec3 N, float roughness ) {
  return GGX_G1( L, N , roughness ) * GGX_G1( V, N , roughness );
}

vec3 walter( vec3 L, vec3 V, vec3 N, float roughness, vec3 fres ) {
  vec3 H = normalize(V + L);
  float D = GGX_D( N, H, roughness );
  vec3 F = fres + ( vec3( 1, 1, 1 ) - fres ) * fresnel( L, N );
  float G = GGX_G2( L, V, N, roughness );
  float scale = 4 * dot( L, N ) * dot( V, N );
  vec3 specular = F * D * G / scale;
  return specular;
}

vec3 eotf( vec3 v ) {
  return min( max( v / (v + 0.155 ) * 1.019, vec3( 0, 0, 0 ) ), vec3( 1, 1, 1 ) );
}

void main()  {
  const float normal_scale = 1.0;
  const float pi = 3.141592653589793;
  // 頂点シェーダから受け取った法線と接線を正規化し、外積求めて接平面座標系への変換行列にする
  vec3 vertex_normal = normalize( input_normal.xyz );
  vec3 tangent = normalize( input_tangent.xyz );
  vec3 binormal = cross( vertex_normal, tangent );
  mat3 texture_space = transpose( mat3( tangent, binormal, vertex_normal ) );
  // 接平面座標系での法線の向きをイメージから得る
  vec3 normal = normalize( texture( normal_map, input_texcoord ).rgb * vec3( normal_scale, normal_scale, 1 ) * 2.0 - 1.0 );
  // ワールド座標系での光源の向きを接平面座標系での向きに変換する
  vec3 light_dir = texture_space * normalize( uniforms.light_pos.xyz - input_position );
  // ワールド座標系での視点の向きを接平面座標系での向きに変換する
  vec3 eye_dir = texture_space * normalize( uniforms.eye_pos.xyz - input_position );
  // イメージをサンプリングしてこの位置における色を得る
  vec3 input_color = texture( base_color, input_texcoord ).rgb;
  // ランバートモデルで拡散を求める
  vec3 diffuse = input_color * ( max( dot( light_dir, normal ), 0 ) /pi );
  // イメージをサンプリングしてこの位置における表面の荒さを得る
  float roughness = texture( roughness_map, input_texcoord ).r;
  // イメージから得た表面の荒さを使ってGGXモデルで反射を求める
  vec3 specular = max( dot( light_dir, normal ), 0 ) * max( walter( light_dir, eye_dir, normal, roughness, vec3( 1.f, 1.f, 1.f ) ), vec3( 0.0, 0.0, 0.0 ) );
  // 光が届かない部分が完全な黒になると不自然なので少しだけ明るくする(環境光)
  vec3 ambient = input_color * 0.01;
  // 光のエネルギーをsRGB色空間での色に変換して出力する
  output_color = vec4( eotf( ( diffuse + specular + ambient ) * uniforms.light_energy ), 1.0 );
}


