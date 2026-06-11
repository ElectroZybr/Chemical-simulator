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
    @builtin(vertex_index) vertexIndex: u32,
    @builtin(instance_index) instanceIndex: u32,
    @location(0) fieldVector: vec2<f32>,
) -> VertOut {
    let gridX = max(u32(uScene.maxCount.x), 1u);
    let cellSize = uScene.maxCount.z;
    let z = uScene.maxCount.w + 0.02;
    let x = instanceIndex % gridX;
    let y = instanceIndex / gridX;
    let center = vec2<f32>(f32(x), f32(y)) * cellSize;

    let vectorLength = length(fieldVector);
    let visible = select(0.0, 1.0, vectorLength >= 0.0001);
    let direction = select(vec2<f32>(1.0, 0.0), normalize(fieldVector), vectorLength >= 0.0001);
    let normal = vec2<f32>(-direction.y, direction.x);

    let lineLength = cellSize * 0.65;
    let headLength = lineLength * 0.28;
    let start = center - direction * (lineLength * 0.5);
    let end = center + direction * (lineLength * 0.5);
    let headBase = end - direction * headLength;
    let headLeft = headBase + normal * (headLength * 0.45);
    let headRight = headBase - normal * (headLength * 0.45);

    var localPos = start;
    if (vertexIndex == 1u || vertexIndex == 2u || vertexIndex == 4u) {
        localPos = end;
    } else if (vertexIndex == 3u) {
        localPos = headLeft;
    } else if (vertexIndex == 5u) {
        localPos = headRight;
    }

    let worldPos = vec3<f32>(localPos, z) + uScene.renderOffset.xyz;

    var out: VertOut;
    out.pos = uScene.projection * uScene.view * vec4<f32>(worldPos, 1.0);
    out.color = vec4<f32>(uScene.lineColor.rgb, uScene.lineColor.a * visible);
    return out;
}

@fragment
fn fs_main(in: VertOut) -> @location(0) vec4<f32> {
    return in.color;
}
