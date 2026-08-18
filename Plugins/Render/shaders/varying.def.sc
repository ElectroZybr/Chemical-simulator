float v_atomCount  : TEXCOORD0 = 0.0;
float v_maxCount   : TEXCOORD1 = 0.0;
vec3  v_fragColor  : TEXCOORD2 = vec3(0.0, 0.0, 0.0);
vec2  v_uv         : TEXCOORD3 = vec2(0.0, 0.0);
float v_isSelected : TEXCOORD4 = 0.0;
vec4  v_data0      : TEXCOORD8 = vec4(0.0, 0.0, 0.0, 0.0);

vec4 a_position  : POSITION;

vec4 i_data0 : TEXCOORD7;
vec4 i_data1 : TEXCOORD6;
vec4 i_data2 : TEXCOORD5;
