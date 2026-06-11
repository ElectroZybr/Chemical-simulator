struct SceneUniforms {
    view       : mat4x4<f32>,
    projection : mat4x4<f32>,
    lightDir   : vec4<f32>,
    colorMode  : vec4<f32>,
    maxSpeedSqr: vec4<f32>,
    maxCount   : vec4<f32>,
    renderOffset: vec4<f32>,
    lineColor  : vec4<f32>,
    typeColors : array<vec4<f32>, 119>,
}
@group(0) @binding(0) var<uniform> uScene: SceneUniforms;

struct VertOut {
    @builtin(position) pos   : vec4<f32>,
    @location(0)       localPos : vec2<f32>,
    @location(1)       potentials : vec4<f32>,
}

@vertex
fn vs_main(
    @location(0) localPos  : vec2<f32>,
    @location(1) origin    : vec4<f32>,
    @location(2) cellSize  : vec2<f32>,
    @location(3) potentials : vec4<f32>,
) -> VertOut {
    let worldPos = origin.xyz + vec3<f32>(localPos * cellSize, 0.0) + uScene.renderOffset.xyz;

    var out: VertOut;
    out.pos = uScene.projection * uScene.view * vec4<f32>(worldPos, 1.0);
    out.localPos = localPos;
    out.potentials = potentials;
    return out;
}

@fragment
fn fs_main(in: VertOut) -> @location(0) vec4<f32> {
    let maxValue = max(uScene.maxCount.x, 0.0001);
    let interpolation = clamp(uScene.maxCount.y, 0.0, 1.0);
    let samplePos = mix(vec2<f32>(0.5, 0.5), in.localPos, interpolation);
    let potentialBottom = mix(in.potentials.x, in.potentials.y, samplePos.x);
    let potentialTop = mix(in.potentials.z, in.potentials.w, samplePos.x);
    let potential = mix(potentialBottom, potentialTop, samplePos.y);
    let signedLinear = clamp(potential / maxValue, -1.0, 1.0);
    let strength = abs(signedLinear);

    let negativeColor = vec3<f32>(0.05, 0.25, 1.0);
    let positiveColor = vec3<f32>(1.0, 0.08, 0.02);

    var rgb = positiveColor;
    if (signedLinear < 0.0) {
        rgb = negativeColor;
    }

    return vec4<f32>(rgb, 0.78 * strength);
}
