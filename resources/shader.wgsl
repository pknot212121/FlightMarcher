struct MyUniforms {
    projectionMatrix: mat4x4f,
	viewMatrix: mat4x4f,
	modelMatrix: mat4x4f,
	color: vec4f,
	time: f32,
};

struct VertexInput {
    @location(0) position: vec3f,
    @location(1) color: vec3f,
};

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) color: vec3f,
};

@group(0) @binding(0) var<uniform> uMyUniforms: MyUniforms;

const pi = 3.14159265359;

@vertex
fn vs(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
	out.position = uMyUniforms.projectionMatrix * uMyUniforms.viewMatrix * uMyUniforms.modelMatrix * vec4f(in.position, 1.0);
	out.color = in.color;
	return out;
}

@fragment
fn fs(in: VertexOutput) -> @location(0) vec4f {
    let linear_color = pow(in.color * uMyUniforms.color.rgb, vec3f(2.2));
    return vec4f(linear_color,1.0);
}
