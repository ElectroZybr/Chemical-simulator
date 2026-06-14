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
@group(0) @binding(0) var<uniform>       uScene  : SceneUniforms;
@group(0) @binding(1) var<storage, read> sPos    : array<vec4<f32>>;
@group(0) @binding(2) var<storage, read> sVel    : array<vec4<f32>>;
@group(0) @binding(3) var<storage, read> sType   : array<f32>;
@group(0) @binding(4) var<storage, read> sRadius : array<f32>;
@group(0) @binding(5) var<storage, read> sSel    : array<f32>;

struct VertOut {
    @builtin(position) pos    : vec4<f32>,
    @location(0)       uv     : vec2<f32>,
    @location(1)       color  : vec3<f32>,
    @location(2)       sel    : f32,
    @location(3)       centerView : vec3<f32>,
    @location(4)       radius : f32,
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
fn vs_main(
    @location(0)        quadPos     : vec2<f32>,
    @builtin(instance_index) iid    : u32,
) -> VertOut {
    let pos = sPos[iid].xyz + uScene.renderOffset.xyz;
    let r  = sRadius[iid];

    let right = vec3<f32>(uScene.view[0][0], uScene.view[1][0], uScene.view[2][0]);
    let up    = vec3<f32>(uScene.view[0][1], uScene.view[1][1], uScene.view[2][1]);

    let worldPos = pos
                 + right * quadPos.x * r
                 + up    * quadPos.y * r;
    let centerView = (uScene.view * vec4<f32>(pos, 1.0)).xyz;

    let mode = u32(uScene.colorMode.x);
    var color: vec3<f32>;

    if (mode == 0u) {
        let t = u32(sType[iid]);
        color = uScene.typeColors[t].rgb;
    }
    else {
        let v = sVel[iid];
        let speedSqr = v.x*v.x + v.y*v.y + v.z*v.z;
        let t = clamp(speedSqr / uScene.maxSpeedSqr.x, 0.0, 1.0);
        if (mode == 1u) {
            color = vec3<f32>(t, 0.0, 1.0 - t);
        }
        else {
            color = turboColor(t);
        }
    }

    var out: VertOut;
    out.pos = uScene.projection * uScene.view * vec4<f32>(worldPos, 1.0);
    out.uv     = quadPos; 
    out.color  = color;
    out.sel    = sSel[iid];
    out.centerView = centerView;
    out.radius = r;
    return out;
}

struct FragOut {
    @builtin(frag_depth) depth : f32,
    @location(0) color : vec4<f32>,
}

@fragment
fn fs_main(in: VertOut) -> FragOut {
    let d2 = dot(in.uv, in.uv);
    if (d2 > 1.0) { discard; }

    var color = in.color;

    // простое освещение
    let z = sqrt(1.0 - d2);
    let n = normalize(vec3<f32>(in.uv, z));
    let light = normalize(uScene.lightDir.xyz);
    let diff  = max(dot(n, light), 0.0);
    color = vec3<f32>(color.rgb * (0.3 + 0.7 * diff));

    // Write sphere surface depth instead of billboard quad depth.
    let viewPos = in.centerView + vec3<f32>(in.uv * in.radius, z * in.radius);
    let clipPos = uScene.projection * vec4<f32>(viewPos, 1.0);
    let ndcDepth = clipPos.z / clipPos.w;

    // выделение
    if (in.sel > 0.5) {
        color = mix(color, vec3<f32>(1.0, 0.85, 0.0), 0.5);
    }

    var out : FragOut;
    out.depth = ndcDepth;
    out.color = vec4<f32>(color, 1.0);
    return out;
}
