// fragment_shader.glsl
#version 330
uniform sampler2D alphaMap;
out vec4 fragColor;

void main() {
    float alpha = texture(alphaMap, gl_TexCoord[0].xy).r;
    fragColor = vec4(1.0, 0.0, 0.0, alpha); // Красный + прозрачность
}