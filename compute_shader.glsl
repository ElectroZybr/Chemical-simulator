// compute_shader.glsl
#version 430
layout(local_size_x = 16, local_size_y = 16) in;
layout(r8, binding = 0) uniform image2D alphaMap;

void main() {
    ivec2 pixelCoords = ivec2(gl_GlobalInvocationID.xy);
    // Здесь можно обновлять прозрачность на GPU
    float alpha = ...; // Ваша логика расчета
    imageStore(alphaMap, pixelCoords, vec4(alpha));
}