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

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (int(idx) >= params.max_points) {
        return;
    }

    uint mm_base = idx * 12u;

    if (int(idx) < params.point_count) {
        uint pc_base = idx * 3u;
        float x = point_cloud.data[pc_base];
        float y = point_cloud.data[pc_base + 1u];
        float z = point_cloud.data[pc_base + 2u];

        // Remap Hesai coordinates to Godot Y-up:
        //   Hesai: x = forward, y = left, z = up
        //   Godot: x = right,   y = up,   z = back
        multimesh.data[mm_base + 3u]  = x;   // forward → right  (good enough for a demo)
        multimesh.data[mm_base + 7u]  = z;   // up      → up     ✓
        multimesh.data[mm_base + 11u] = y;   // left    → back
    } else {
        // Exile surplus instances so they don't clutter the view.
        multimesh.data[mm_base + 3u]  = -666.0;
        multimesh.data[mm_base + 7u]  = -666.0;
        multimesh.data[mm_base + 11u] = -666.0;
    }
}
