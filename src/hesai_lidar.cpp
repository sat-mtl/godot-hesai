#include "hesai_lidar.hpp"
#include "godot_cpp/classes/rendering_server.hpp"
#include <algorithm>
#include <cstring>

void HesaiPointCloudGPU::_bind_methods() {
    ClassDB::bind_method(D_METHOD("start_stream"), &HesaiPointCloudGPU::start_stream);
    ClassDB::bind_method(D_METHOD("stop_stream"), &HesaiPointCloudGPU::stop_stream);
    ClassDB::bind_method(D_METHOD("set_rendering_device", "rd"), &HesaiPointCloudGPU::set_rendering_device);
    ClassDB::bind_method(D_METHOD("get_rendering_device"), &HesaiPointCloudGPU::get_rendering_device);
    ClassDB::bind_method(D_METHOD("get_point_buffer_rid"), &HesaiPointCloudGPU::get_point_buffer_rid);
    ClassDB::bind_method(D_METHOD("get_num_points"), &HesaiPointCloudGPU::get_num_points);
    ClassDB::bind_method(D_METHOD("update_point_cloud_buffer"), &HesaiPointCloudGPU::update_point_cloud_buffer);

    ClassDB::bind_method(D_METHOD("set_source_type", "type"), &HesaiPointCloudGPU::set_source_type);
    ClassDB::bind_method(D_METHOD("get_source_type"), &HesaiPointCloudGPU::get_source_type);
    ClassDB::bind_method(D_METHOD("set_device_ip", "ip"), &HesaiPointCloudGPU::set_device_ip);
    ClassDB::bind_method(D_METHOD("get_device_ip"), &HesaiPointCloudGPU::get_device_ip);
    ClassDB::bind_method(D_METHOD("set_host_ip", "ip"), &HesaiPointCloudGPU::set_host_ip);
    ClassDB::bind_method(D_METHOD("get_host_ip"), &HesaiPointCloudGPU::get_host_ip);
    ClassDB::bind_method(D_METHOD("set_udp_port", "port"), &HesaiPointCloudGPU::set_udp_port);
    ClassDB::bind_method(D_METHOD("get_udp_port"), &HesaiPointCloudGPU::get_udp_port);
    ClassDB::bind_method(D_METHOD("set_ptc_port", "port"), &HesaiPointCloudGPU::set_ptc_port);
    ClassDB::bind_method(D_METHOD("get_ptc_port"), &HesaiPointCloudGPU::get_ptc_port);
    ClassDB::bind_method(D_METHOD("set_pcap_path", "path"), &HesaiPointCloudGPU::set_pcap_path);
    ClassDB::bind_method(D_METHOD("get_pcap_path"), &HesaiPointCloudGPU::get_pcap_path);
    ClassDB::bind_method(D_METHOD("set_correction_file_path", "path"), &HesaiPointCloudGPU::set_correction_file_path);
    ClassDB::bind_method(D_METHOD("get_correction_file_path"), &HesaiPointCloudGPU::get_correction_file_path);
    ClassDB::bind_method(D_METHOD("set_max_points", "max"), &HesaiPointCloudGPU::set_max_points);
    ClassDB::bind_method(D_METHOD("get_max_points"), &HesaiPointCloudGPU::get_max_points);

    ADD_PROPERTY(PropertyInfo(Variant::INT, "source_type", PROPERTY_HINT_ENUM, "UDP:1,PCAP:2"),
        "set_source_type", "get_source_type");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "device_ip"), "set_device_ip", "get_device_ip");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "host_ip"), "set_host_ip", "get_host_ip");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "udp_port"), "set_udp_port", "get_udp_port");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "ptc_port"), "set_ptc_port", "get_ptc_port");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "pcap_path", PROPERTY_HINT_FILE, "*.pcap"),
        "set_pcap_path", "get_pcap_path");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "correction_file_path", PROPERTY_HINT_FILE),
        "set_correction_file_path", "get_correction_file_path");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "max_points"), "set_max_points", "get_max_points");
    ClassDB::bind_method(D_METHOD("set_pcap_loop", "loop"), &HesaiPointCloudGPU::set_pcap_loop);
    ClassDB::bind_method(D_METHOD("get_pcap_loop"), &HesaiPointCloudGPU::get_pcap_loop);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "pcap_loop"), "set_pcap_loop", "get_pcap_loop");

    ADD_SIGNAL(MethodInfo("point_cloud_frame",
        PropertyInfo(Variant::RID, "point_cloud_buffer"),
        PropertyInfo(Variant::PACKED_BYTE_ARRAY, "hesai_bytes")));

    BIND_ENUM_CONSTANT(SOURCE_UDP);
    BIND_ENUM_CONSTANT(SOURCE_PCAP);
}

HesaiPointCloudGPU::~HesaiPointCloudGPU() {
    stop_stream();
}

void HesaiPointCloudGPU::_enter_tree() {
    if (rd != nullptr) {
        allocate_point_cloud_buffer();
    }
}

void HesaiPointCloudGPU::_exit_tree() {
    stop_stream();
    // Null rd first so any in-flight render-thread calls early-return before we free.
    RenderingDevice *device = rd;
    rd = nullptr;
    if (device && point_buffer.is_valid()) {
        device->free_rid(point_buffer);
        point_buffer = RID();
    }
}

void HesaiPointCloudGPU::allocate_point_cloud_buffer() {
    if (!rd) {
        print_line("HesaiPointCloudGPU: set a rendering device before allocating the buffer.");
        return;
    }

    const uint32_t gpu_bytes = static_cast<uint32_t>(max_points) * 3u * sizeof(float);

    gpu_staging_bytes.resize(gpu_bytes);
    hesai_staging_bytes.resize(static_cast<uint32_t>(max_points) * sizeof(PointType));

    PackedByteArray empty{};
    empty.resize(gpu_bytes);

    if (!rd->has_feature(RenderingDevice::Features::SUPPORTS_BUFFER_DEVICE_ADDRESS)) {
        point_buffer = rd->storage_buffer_create(gpu_bytes, empty);
        print_line("HesaiPointCloudGPU: GPU does not support buffer device address.");
    } else {
        point_buffer = rd->storage_buffer_create(gpu_bytes, empty, 0,
            RenderingDevice::BufferCreationBits::BUFFER_CREATION_DEVICE_ADDRESS_BIT);
    }
}

void HesaiPointCloudGPU::start_stream() {
    if (sdk) {
        print_line("HesaiPointCloudGPU: stream already running, call stop_stream() first.");
        return;
    }
    if (!rd) {
        print_line("HesaiPointCloudGPU: set a rendering device before starting the stream.");
        return;
    }

    hesai::lidar::DriverParam param;
    param.input_param.source_type = static_cast<hesai::lidar::SourceType>(source_type);
    param.input_param.device_ip_address = device_ip.utf8().get_data();
    param.input_param.host_ip_address = host_ip.utf8().get_data();
    param.input_param.udp_port = static_cast<uint16_t>(udp_port);
    param.input_param.ptc_port = static_cast<uint16_t>(ptc_port);
    param.input_param.pcap_path = pcap_path.utf8().get_data();
    param.input_param.correction_file_path = correction_file_path.utf8().get_data();
    param.input_param.use_ptc_connected = false;
    param.decoder_param.pcap_play_in_loop = pcap_loop;
    param.decoder_param.pcap_play_synchronization = (source_type == SOURCE_PCAP);

    if (!point_buffer.is_valid()) {
        allocate_point_cloud_buffer();
    }

    sdk = std::make_unique<hesai::lidar::HesaiLidarSdk<PointType>>();
    sdk->Init(param);
    sdk->RegRecvCallback([this](const hesai::lidar::LidarDecodedFrame<PointType> &frame) {
        on_lidar_frame(frame);
    });
    sdk->Start();
}

void HesaiPointCloudGPU::stop_stream() {
    if (sdk) {
        sdk->Stop();
        sdk.reset();
    }
}

void HesaiPointCloudGPU::on_lidar_frame(const hesai::lidar::LidarDecodedFrame<PointType> &frame) {
    if (!rd || frame.points_num == 0) {
        return;
    }

    const uint32_t n = std::min(static_cast<uint32_t>(max_points), frame.points_num);

    {
        std::lock_guard<std::mutex> lock(frame_mutex);

        float *gpu_ptr = reinterpret_cast<float *>(gpu_staging_bytes.ptrw());
        for (uint32_t i = 0; i < n; ++i) {
            gpu_ptr[i * 3 + 0] = frame.points[i].x;
            gpu_ptr[i * 3 + 1] = frame.points[i].y;
            gpu_ptr[i * 3 + 2] = frame.points[i].z;
        }

        std::memcpy(hesai_staging_bytes.ptrw(), frame.points, n * sizeof(PointType));
        current_points_num.store(n);
    }

    RenderingServer::get_singleton()->call_on_render_thread(
        Callable(this, "update_point_cloud_buffer"));
}

void HesaiPointCloudGPU::update_point_cloud_buffer() {
    if (!rd || !point_buffer.is_valid()) {
        return;
    }

    PackedByteArray gpu_bytes_copy;
    PackedByteArray hesai_bytes_copy;
    uint32_t n;

    {
        std::lock_guard<std::mutex> lock(frame_mutex);
        n = current_points_num.load();
        if (n == 0) {
            return;
        }
        gpu_bytes_copy = gpu_staging_bytes.slice(0, n * 3 * sizeof(float));
        hesai_bytes_copy = hesai_staging_bytes.slice(0, n * sizeof(PointType));
    }

    rd->buffer_update(point_buffer, 0, gpu_bytes_copy.size(), gpu_bytes_copy);
    emit_signal("point_cloud_frame", point_buffer, hesai_bytes_copy);
}

RID HesaiPointCloudGPU::get_point_buffer_rid() {
    return point_buffer;
}

uint32_t HesaiPointCloudGPU::get_num_points() {
    return current_points_num.load();
}

void HesaiPointCloudGPU::set_rendering_device(RenderingDevice *p_rd) {
    rd = p_rd;
}

RenderingDevice *HesaiPointCloudGPU::get_rendering_device() {
    return rd;
}

void HesaiPointCloudGPU::set_source_type(int p_type) { source_type = p_type; }
int HesaiPointCloudGPU::get_source_type() { return source_type; }

void HesaiPointCloudGPU::set_device_ip(String p_ip) { device_ip = p_ip; }
String HesaiPointCloudGPU::get_device_ip() { return device_ip; }

void HesaiPointCloudGPU::set_host_ip(String p_ip) { host_ip = p_ip; }
String HesaiPointCloudGPU::get_host_ip() { return host_ip; }

void HesaiPointCloudGPU::set_udp_port(int p_port) { udp_port = p_port; }
int HesaiPointCloudGPU::get_udp_port() { return udp_port; }

void HesaiPointCloudGPU::set_ptc_port(int p_port) { ptc_port = p_port; }
int HesaiPointCloudGPU::get_ptc_port() { return ptc_port; }

void HesaiPointCloudGPU::set_pcap_path(String p_path) { pcap_path = p_path; }
String HesaiPointCloudGPU::get_pcap_path() { return pcap_path; }

void HesaiPointCloudGPU::set_correction_file_path(String p_path) { correction_file_path = p_path; }
String HesaiPointCloudGPU::get_correction_file_path() { return correction_file_path; }

void HesaiPointCloudGPU::set_max_points(int p_max) { max_points = p_max; }
int HesaiPointCloudGPU::get_max_points() { return max_points; }

void HesaiPointCloudGPU::set_pcap_loop(bool p_loop) { pcap_loop = p_loop; }
bool HesaiPointCloudGPU::get_pcap_loop() { return pcap_loop; }
