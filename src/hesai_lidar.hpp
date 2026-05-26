#pragma once

#include "godot_cpp/classes/node.hpp"
#include "godot_cpp/classes/rendering_device.hpp"
#include "godot_cpp/variant/rid.hpp"
#include "godot_cpp/variant/packed_byte_array.hpp"
#include "godot_cpp/variant/string.hpp"

#include "hesai_lidar_sdk.hpp"

#include <memory>
#include <mutex>
#include <atomic>

using namespace godot;

class HesaiPointCloudGPU : public Node {
    GDCLASS(HesaiPointCloudGPU, Node)

public:
    enum SourceType {
        SOURCE_UDP  = hesai::lidar::DATA_FROM_LIDAR,
        SOURCE_PCAP = hesai::lidar::DATA_FROM_PCAP,
    };

protected:
    static void _bind_methods();

public:
    ~HesaiPointCloudGPU() override;

    void _enter_tree() override;
    void _exit_tree() override;

    void start_stream();
    void stop_stream();

    void set_rendering_device(RenderingDevice *p_rd);
    RenderingDevice *get_rendering_device();

    RID get_point_buffer_rid();
    uint32_t get_num_points();

    // Called on the render thread; do not call directly.
    void update_point_cloud_buffer();

    void set_source_type(int p_type);
    int get_source_type();
    void set_device_ip(String p_ip);
    String get_device_ip();
    void set_host_ip(String p_ip);
    String get_host_ip();
    void set_udp_port(int p_port);
    int get_udp_port();
    void set_ptc_port(int p_port);
    int get_ptc_port();
    void set_pcap_path(String p_path);
    String get_pcap_path();
    void set_correction_file_path(String p_path);
    String get_correction_file_path();
    void set_max_points(int p_max);
    int get_max_points();
    void set_pcap_loop(bool p_loop);
    bool get_pcap_loop();

private:
    using PointType = hesai::lidar::LidarPointXYZI;

    void allocate_point_cloud_buffer();
    void on_lidar_frame(const hesai::lidar::LidarDecodedFrame<PointType> &frame);

    RID point_buffer{};
    RenderingDevice *rd = nullptr;

    // Staging buffers written by the SDK thread, read on the render thread.
    std::mutex frame_mutex;
    PackedByteArray gpu_staging_bytes{};
    PackedByteArray hesai_staging_bytes{};
    std::atomic<uint32_t> current_points_num{0};

    std::unique_ptr<hesai::lidar::HesaiLidarSdk<PointType>> sdk;

    // Properties
    int source_type = SOURCE_UDP;
    String device_ip = "192.168.1.201";
    String host_ip = "";
    int udp_port = 2368;
    int ptc_port = 9347;
    String pcap_path = "";
    String correction_file_path = "";
    int max_points = 100000;
    bool pcap_loop = true;
};

VARIANT_ENUM_CAST(HesaiPointCloudGPU::SourceType);
