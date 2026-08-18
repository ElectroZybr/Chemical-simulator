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
fn vs_main(@location(0) pos: vec3<f32>) -> VertOut {
    var out: VertOut;
    out.pos   = uScene.projection * uScene.view * vec4<f32>(pos + uScene.renderOffset.xyz, 1.0);
    out.color = uScene.lineColor;
    return out;
}

@fragment
fn fs_main(in: VertOut) -> @location(0) vec4<f32> {
    return in.color;
}
