extends Node3D

# ── Rendering Device ──────────────────────────────────────────────────────────
var rd: RenderingDevice

# ── Compute shader resources ─────────────────────────────────────────────────
var shader: RID
var pipeline: RID

var point_cloud_uniform := RDUniform.new()
var multimesh_uniform   := RDUniform.new()

# ── MultiMesh ─────────────────────────────────────────────────────────────────
var multimesh: MultiMesh
var max_points: int

# ── Frame state ───────────────────────────────────────────────────────────────
var point_cloud_buffer_rid: RID
var num_points: int = 0
var should_draw: bool = false

func _ready() -> void:
	rd = RenderingServer.get_rendering_device()

	# ── Compute shader ─────────────────────────────────────────────────────────
	var shader_file: RDShaderFile = load("res://shaders/point_cloud.glsl")
	var spirv: RDShaderSPIRV = shader_file.get_spirv()
	shader   = rd.shader_create_from_spirv(spirv)
	pipeline = rd.compute_pipeline_create(shader)

	point_cloud_uniform.uniform_type = RenderingDevice.UNIFORM_TYPE_STORAGE_BUFFER
	point_cloud_uniform.binding = 0
	multimesh_uniform.uniform_type = RenderingDevice.UNIFORM_TYPE_STORAGE_BUFFER
	multimesh_uniform.binding = 1

	# ── MultiMesh ──────────────────────────────────────────────────────────────
	max_points = $HesaiPointCloudGPU.max_points

	multimesh = MultiMesh.new()
	multimesh.transform_format = MultiMesh.TRANSFORM_3D

	var pmesh  := PointMesh.new()
	var mat    := StandardMaterial3D.new()
	mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	mat.albedo_color = Color(1.0, 0.55, 0.0)  # orange
	mat.use_point_size = true
	mat.point_size = 2.0
	pmesh.material = mat
	multimesh.mesh = pmesh

	multimesh.instance_count = max_points

	# Initialise with identity rotation so the basis columns are correct before the
	# first frame arrives.  Only the position entries (indices 3, 7, 11 per instance)
	# are updated by the compute shader; the diagonal must already be 1.
	var buf := PackedFloat32Array()
	buf.resize(max_points * 12)
	for i in range(max_points):
		var b: int = i * 12
		buf[b]      = 1.0   # m[0][0]
		buf[b + 3]  = -666.0  # origin x – hide until first frame
		buf[b + 5]  = 1.0   # m[1][1]
		buf[b + 7]  = -666.0  # origin y
		buf[b + 10] = 1.0   # m[2][2]
		buf[b + 11] = -666.0  # origin z
	multimesh.buffer = buf

	$MultiMeshInstance3D.multimesh = multimesh

	# ── HesaiPointCloudGPU ────────────────────────────────────────────────────
	$HesaiPointCloudGPU.set_rendering_device(rd)
	$HesaiPointCloudGPU.source_type = HesaiPointCloudGPU.SOURCE_PCAP
	$HesaiPointCloudGPU.pcap_path   = ProjectSettings.globalize_path("res://HESAI_TEST_OFFICE.pcap")
	$HesaiPointCloudGPU.pcap_loop   = true
	$HesaiPointCloudGPU.point_cloud_frame.connect(_on_point_cloud_frame)
	$HesaiPointCloudGPU.start_stream()

# Called on the render thread (emitted via call_on_render_thread in the C++ binding).
func _on_point_cloud_frame(buffer: RID, _hesai_bytes: PackedByteArray) -> void:
	point_cloud_buffer_rid = buffer
	num_points = $HesaiPointCloudGPU.get_num_points()
	should_draw = true

func _process(_delta: float) -> void:
	if should_draw and point_cloud_buffer_rid.is_valid() and num_points > 0:
		should_draw = false
		_apply_compute_shader()

func _apply_compute_shader() -> void:
	var mm_rid: RID = RenderingServer.multimesh_get_buffer_rd_rid(multimesh)

	point_cloud_uniform.clear_ids()
	point_cloud_uniform.add_id(point_cloud_buffer_rid)
	multimesh_uniform.clear_ids()
	multimesh_uniform.add_id(mm_rid)

	var uniform_set: RID = rd.uniform_set_create(
		[point_cloud_uniform, multimesh_uniform], shader, 0)

	var push := PackedInt32Array([num_points, max_points, 0, 0])
	var compute_list: int = rd.compute_list_begin()
	rd.compute_list_bind_compute_pipeline(compute_list, pipeline)
	rd.compute_list_bind_uniform_set(compute_list, uniform_set, 0)
	rd.compute_list_set_push_constant(compute_list, push.to_byte_array(), 16)
	var wg := _get_workgroups(max_points)
	rd.compute_list_dispatch(compute_list, wg.x, wg.y, wg.z)
	rd.compute_list_end()

# Computes workgroup counts for a 1-D dispatch clamped to Vulkan's 65535 per dimension.
# Adapted from https://github.com/ossia/score/blob/master/src/plugins/score-plugin-gfx/Gfx/Graph/RenderedCSFNode.cpp
func _get_workgroups(required: int) -> Vector3i:
	const THREADS := 64
	const MAX_WG  := 65535
	@warning_ignore("integer_division")
	var total: int = (required + THREADS - 1) / THREADS
	if total > MAX_WG * MAX_WG:
		@warning_ignore("integer_division")
		var rem: int = (total + MAX_WG - 1) / MAX_WG
		@warning_ignore("integer_division")
		return Vector3i(MAX_WG, min(rem, MAX_WG), (rem + MAX_WG - 1) / MAX_WG)
	elif total > MAX_WG:
		@warning_ignore("integer_division")
		return Vector3i(min(total, MAX_WG), (total + MAX_WG - 1) / MAX_WG, 1)
	else:
		return Vector3i(total, 1, 1)
