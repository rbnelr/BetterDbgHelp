#import bevy_pbr::{
	forward_io::VertexOutput,
	mesh_view_bindings::view,
	mesh_functions,
	pbr_types::{STANDARD_MATERIAL_FLAGS_DOUBLE_SIDED_BIT, PbrInput, pbr_input_new},
	pbr_functions,
	pbr_bindings,
}
#import bevy_core_pipeline::tonemapping::tone_mapping

struct ManyCubesCubeMat {
	tint: vec4<f32>,
	emissive: vec4<f32>,
};
@group(#{MATERIAL_BIND_GROUP}) @binding(0) var<storage, read> buf: array<ManyCubesCubeMat>;

//struct Vertex {
//	@builtin(instance_index) instance_index: u32,
//	@location(0) position: vec3<f32>,
//};
//
//@vertex
//fn vertex(vertex: Vertex) -> VertexOutput {
//	var out: VertexOutput;
//	let tag = mesh_functions::get_tag(vertex.instance_index);
//	var world_from_local = mesh_functions::get_world_from_local(vertex.instance_index);
//	out.world_position = mesh_functions::mesh_position_local_to_world(world_from_local, vec4(vertex.position, 1.0));
//	out.clip_position = position_world_to_clip(out.world_position.xyz);
//	return out;
//}

@fragment
fn fragment(
	@builtin(front_facing) is_front: bool,
	mesh: VertexOutput,
) -> @location(0) vec4<f32> {
	let layer = i32(mesh.world_position.x) & 0x3;
	
	// Prepare a 'processed' StandardMaterial by sampling all textures to resolve
	// the material members
	var pbr_input: PbrInput = pbr_input_new();
	let tag = mesh_functions::get_tag(mesh.instance_index);
	
	//pbr_input.material.base_color = textureSample(my_array_texture, my_array_texture_sampler, mesh.uv, layer);
	pbr_input.material.base_color *= buf[tag].tint;
	pbr_input.material.emissive = buf[tag].emissive;
#ifdef VERTEX_COLORS
	pbr_input.material.base_color *= mesh.color;
#endif
	
	let double_sided = (pbr_input.material.flags & STANDARD_MATERIAL_FLAGS_DOUBLE_SIDED_BIT) != 0u;
	
	pbr_input.frag_coord = mesh.position;
	pbr_input.world_position = mesh.world_position;
	pbr_input.world_normal = pbr_functions::prepare_world_normal(
		mesh.world_normal,
		double_sided,
		is_front,
	);
	
	pbr_input.is_orthographic = view.clip_from_view[3].w == 1.0;
	
	pbr_input.N = normalize(pbr_input.world_normal);
	
#ifdef VERTEX_TANGENTS
	let Nt = textureSampleBias(pbr_bindings::normal_map_texture, pbr_bindings::normal_map_sampler, mesh.uv, view.mip_bias).rgb;
	let TBN = pbr_functions::calculate_tbn_mikktspace(mesh.world_normal, mesh.world_tangent);
	pbr_input.N = pbr_functions::apply_normal_mapping(
		pbr_input.material.flags,
		TBN,
		double_sided,
		is_front,
		Nt,
	);
#endif
	
	pbr_input.V = pbr_functions::calculate_view(mesh.world_position, pbr_input.is_orthographic);
	
	return tone_mapping(pbr_functions::apply_pbr_lighting(pbr_input), view.color_grading);
}
