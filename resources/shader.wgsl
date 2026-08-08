struct GlobalUniforms 
{
	projectionMatrix: mat4x4f,
	viewMatrix: mat4x4f,
};

@group(0) @binding(0) var<uniform> globalUniforms: GlobalUniforms;

@group(0) @binding(1) var<storage, read> instanceModels: array<mat4x4f>;

struct VertexInput
{
	@builtin(instance_index) instanceIdx: u32,
	@location(0) position: vec3f,
	@location(1) normal: vec3f,
	@location(2) color: vec3f,
};

struct VertexOutput
{
	@builtin(position) position: vec4f,
	@location(0) color: vec3f,
};

@vertex
fn vs(in: VertexInput) -> VertexOutput
{
	var out: VertexOutput;
	let modelMatrix = instanceModels[in.instanceIdx];
	let worldPos = modelMatrix * vec4f(in.position, 1.0);
	out.position = globalUniforms.projectionMatrix * globalUniforms.viewMatrix * worldPos;
	out.color = in.color;
	return out;
}

@fragment
fn fs(in: VertexOutput) -> @location(0) vec4f
{
	return vec4f(in.color, 1.0);
}