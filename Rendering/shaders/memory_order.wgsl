struct SceneUniforms {
    view        : mat4x4<f32>,
    projection  : mat4x4<f32>,
    lightDir    : vec4<f32>,
    colorMode   : vec4<f32>,
    maxSpeedSqr : vec4<f32>,
    maxCount    : vec4<f32>,
    renderOffset: vec4<f32>,
    lineColor   : vec4<f32>,
    typeColors  : array<vec4<f32>, 119>,
}
@group(0) @binding(0) var<uniform> uScene: SceneUniforms;

struct VertOut {
    @builtin(position) pos   : vec4<f32>,
    @location(0)       color : vec4<f32>,
}

fn turboColor(t: f32) -> vec3<f32> {
    let x = clamp(0.1 + t * 0.75, 0.1, 0.85);

    let c0 = vec3<f32>(0.135725, 0.091412, 0.106667);
    let c1 = vec3<f32>(4.597373, 2.185608, 12.592549);
    let c2 = vec3<f32>(-42.327686, 4.805216, -60.109686);
    let c3 = vec3<f32>(130.588706, -14.019451, 109.074510);
    let c4 = vec3<f32>(-150.566627, 4.210863, -88.506588);
    let c5 = vec3<f32>(58.137451, 2.774745, 26.818275);
    var res = c5;
    res = res * x + c4;
    res = res * x + c3;
    res = res * x + c2;
    res = res * x + c1;
    res = res * x + c0;
    return res;
}

@vertex
fn vs_main(@location(0) pos: vec3<f32>, @location(1) t: f32) -> VertOut {
    var out: VertOut;
    let x = clamp(t, 0.0, 1.0);
    let rgb = turboColor(x);
    out.pos = uScene.projection * uScene.view * vec4<f32>(pos + uScene.renderOffset.xyz, 1.0);
    out.color = vec4<f32>(rgb, 1.0);
    return out;
}

@fragment
fn fs_main(in: VertOut) -> @location(0) vec4<f32> {
    return in.color;
}
