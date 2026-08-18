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
    @location(0)       color : vec4<f32>,
}

@vertex
fn vs_main(
    @location(0) localPos  : vec3<f32>,
    @location(1) origin    : vec4<f32>,
    @location(2) cellSize  : f32,
    @location(3) atomCount : f32,
) -> VertOut {
    let worldPos = origin.xyz + localPos * cellSize + uScene.renderOffset.xyz;
    let t = clamp(atomCount / uScene.maxCount.x, 0.0, 1.0);
    let color = mix(vec4<f32>(0.0, 1.0, 0.0, 0.3), vec4<f32>(1.0, 0.0, 0.0, 0.3), t);

    var out: VertOut;
    out.pos   = uScene.projection * uScene.view * vec4<f32>(worldPos, 1.0);
    out.color = color;
    return out;
}

@fragment
fn fs_main(in: VertOut) -> @location(0) vec4<f32> {
    return in.color;
} 
