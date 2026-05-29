#[compute]
#version 450

// 64 threads per workgroup — optimal for AMD and Nvidia.
layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

// xyz floats from HesaiPointCloudGPU: [x0, y0, z0, x1, y1, z1, ...]
layout(set = 0, binding = 0, std430) restrict readonly buffer PointCloud {
    float data[];
} point_cloud;

// MultiMesh transform buffer: 12 floats per instance (3 x 4 row-major matrix).
// Row 0: [ m00, m01, m02, origin.x ]   ← x position at index 3
// Row 1: [ m10, m11, m12, origin.y ]   ← y position at index 7
// Row 2: [ m20, m21, m22, origin.z ]   ← z position at index 11
layout(set = 0, binding = 1, std430) restrict buffer Multimesh {
    float data[];
} multimesh;

layout(push_constant) uniform Params {
    int point_count;  // valid points in this frame
    int max_points;   // total pre-allocated multimesh instances
    int _pad0;
    int _pad1;
} params;

const int l_size_x = 64;
const int max_workgroup_idx = 65535;
const int max_x_idx = max_workgroup_idx * l_size_x;
const int num_floats_per_multimesh_point = 12;
void main() {
    uint point_idx =
      ((gl_WorkGroupID.x + gl_LocalInvocationID.x + ((l_size_x - 1) * gl_WorkGroupID.x)) +
       gl_WorkGroupID.y * max_x_idx +
       gl_WorkGroupID.z * max_x_idx * max_workgroup_idx);

    uint multimesh_buffer_idx = point_idx * num_floats_per_multimesh_point;
    if (int(point_idx) >= params.max_points) {
        return;
    }

    if (int(point_idx) < params.point_count) {
        uint pc_base = point_idx * 3u;
        float x = point_cloud.data[pc_base];
        float y = point_cloud.data[pc_base + 1u];
        float z = point_cloud.data[pc_base + 2u];

        // Remap Hesai coordinates to Godot Y-up:
        //   Hesai: x = forward, y = left, z = up
        //   Godot: x = right,   y = up,   z = back
        multimesh.data[multimesh_buffer_idx + 3u]  = x;   // forward → right  (good enough for a demo)
        multimesh.data[multimesh_buffer_idx + 7u]  = z;   // up      → up     ✓
        multimesh.data[multimesh_buffer_idx + 11u] = y;   // left    → back
    } else {
        // Exile surplus instances so they don't clutter the view.
        multimesh.data[multimesh_buffer_idx + 3u]  = -666.0;
        multimesh.data[multimesh_buffer_idx + 7u]  = -666.0;
        multimesh.data[multimesh_buffer_idx + 11u] = -666.0;
    }
}
