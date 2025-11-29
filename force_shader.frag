uniform sampler2D field;
// uniform float maxForce;

void main()
{
    vec2 uv = gl_TexCoord[0].xy;
    float f = texture2D(field, uv).a;   // сила в диапазоне 0..1

    // float intensity = clamp(f / maxForce, 0.0, 1.0);

    // vec3 color =
    //     (intensity < 0.5)
    //     ? vec3(intensity * 2.0, 0.0, 0.0)                  // черный → красный
    //     : vec3(1.0, (intensity - 0.5) * 2.0, 0.0);         // красный → желтый
        
           
    // gl_FragColor = vec4(color, 1.0);
    gl_FragColor = vec4(1.0, 0.0, 0.0, f);
}
