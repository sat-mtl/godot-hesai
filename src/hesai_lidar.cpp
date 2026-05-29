#include "hesai_lidar.hpp"
#include "godot_cpp/classes/rendering_server.hpp"
#include <algorithm>
#include <cstring>

HesaiPointCloudGPU::HesaiPointCloudGPU() {
  udp_port = base_udp_port + num_instances;
  ptc_port = base_ptc_port + num_instances;
  num_instances+=1;
}

void HesaiPointCloudGPU::_bind_methods() {
  ClassDB::bind_method(D_METHOD("start_stream", "device_ip"), &HesaiPointCloudGPU::start_stream);
  ClassDB::bind_method(D_METHOD("start_pcap_stream", "pcap_path"), &HesaiPointCloudGPU::start_pcap_stream);
  ClassDB::bind_method(D_METHOD("stop_stream"), &HesaiPointCloudGPU::stop_stream);
  ClassDB::bind_method(D_METHOD("set_rendering_device", "rd"), &HesaiPointCloudGPU::set_rendering_device);
  ClassDB::bind_method(D_METHOD("get_rendering_device"), &HesaiPointCloudGPU::get_rendering_device);
  ClassDB::bind_method(D_METHOD("get_point_buffer_rid"), &HesaiPointCloudGPU::get_point_buffer_rid);
  ClassDB::bind_method(D_METHOD("get_num_points"), &HesaiPointCloudGPU::get_num_points);
  ClassDB::bind_method(D_METHOD("update_point_cloud_buffer"), &HesaiPointCloudGPU::update_point_cloud_buffer);

  ClassDB::bind_method(D_METHOD("set_udp_port", "port"), &HesaiPointCloudGPU::set_udp_port);
  ClassDB::bind_method(D_METHOD("get_udp_port"), &HesaiPointCloudGPU::get_udp_port);
  ClassDB::bind_method(D_METHOD("set_ptc_port", "port"), &HesaiPointCloudGPU::set_ptc_port);
  ClassDB::bind_method(D_METHOD("get_ptc_port"), &HesaiPointCloudGPU::get_ptc_port);
  ClassDB::bind_method(D_METHOD("set_correction_file_path", "path"), &HesaiPointCloudGPU::set_correction_file_path);
  ClassDB::bind_method(D_METHOD("get_correction_file_path"), &HesaiPointCloudGPU::get_correction_file_path);
  ClassDB::bind_method(D_METHOD("get_max_points"), &HesaiPointCloudGPU::get_max_points);

  ADD_PROPERTY(PropertyInfo(Variant::STRING, "device_ip"), "set_device_ip", "get_device_ip");
  ADD_PROPERTY(PropertyInfo(Variant::INT, "udp_port"), "set_udp_port", "get_udp_port");
  ADD_PROPERTY(PropertyInfo(Variant::INT, "ptc_port"), "set_ptc_port", "get_ptc_port");
  ADD_PROPERTY(PropertyInfo(Variant::STRING, "correction_file_path", PROPERTY_HINT_FILE),
               "set_correction_file_path", "get_correction_file_path");
  ADD_SIGNAL(MethodInfo("point_cloud_frame",
                        PropertyInfo(Variant::RID, "point_cloud_buffer"),
                        PropertyInfo(Variant::PACKED_BYTE_ARRAY, "hesai_bytes")));

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

  point_bytes.resize(gpu_bytes);

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

void HesaiPointCloudGPU::start_stream(const String& lidar_ip) {
  if (!rd) {
    print_line("HesaiPointCloudGPU: set a rendering device before starting the stream.");
    return;
  }

  hesai::lidar::DriverParam param;
  param.input_param.source_type = hesai::lidar::DATA_FROM_LIDAR;
  param.input_param.device_ip_address = lidar_ip.utf8().get_data();
  param.input_param.udp_port = static_cast<uint16_t>(udp_port);
  param.input_param.ptc_port = static_cast<uint16_t>(ptc_port);
  param.input_param.correction_file_path = correction_file_path.utf8().get_data();
  param.input_param.use_ptc_connected = false;
  start_stream_from_params(param);
}

void HesaiPointCloudGPU::start_pcap_stream(const String& pcap_path) {
  if (!rd) {
    print_line("HesaiPointCloudGPU: set a rendering device before starting the stream.");
    return;
  }

  hesai::lidar::DriverParam param;
  param.input_param.source_type = hesai::lidar::DATA_FROM_PCAP;
  param.input_param.pcap_path = pcap_path.utf8().get_data();
  param.input_param.correction_file_path = correction_file_path.utf8().get_data();
  param.input_param.use_ptc_connected = false;
  param.decoder_param.pcap_play_in_loop = true;
  param.decoder_param.pcap_play_synchronization = true;
  start_stream_from_params(param);
}

void HesaiPointCloudGPU::start_stream_from_params(const hesai::lidar::DriverParam &param) {
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
    sdk.reset();
  }
}

void HesaiPointCloudGPU::on_lidar_frame(const hesai::lidar::LidarDecodedFrame<PointType> &frame) {
  if (!rd || frame.points_num == 0) {
    return;
  }

  const uint32_t n = std::min(static_cast<uint32_t>(max_points), frame.points_num);

  {
    // Its theoretically possible that the gpu upload is still going on when a new lidar frame comes
    // though I really doubt its likely to happen.
    std::lock_guard<std::mutex> lock(frame_mutex);

    float *points_ptr = reinterpret_cast<float *>(point_bytes.ptrw());
    // depending on PointType, the points can have much more than XYZ but we only care about those.
    for (uint32_t current_point = 0; current_point < n; current_point++) {
      points_ptr[current_point * 3 + 0] = frame.points[current_point].x;
      points_ptr[current_point * 3 + 1] = frame.points[current_point].y;
      points_ptr[current_point * 3 + 2] = frame.points[current_point].z;
    }
    current_points_num.store(n);
  }

  RenderingServer::get_singleton()->call_on_render_thread(Callable(this, "update_point_cloud_buffer"));
}

void HesaiPointCloudGPU::update_point_cloud_buffer() {
  if (!rd || !point_buffer.is_valid()) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(frame_mutex);
    rd->buffer_update(point_buffer, 0, point_bytes.size(), point_bytes);
  }
  emit_signal("point_cloud_frame", point_buffer, point_bytes);
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

void HesaiPointCloudGPU::set_udp_port(int p_port) { udp_port = p_port; }
int HesaiPointCloudGPU::get_udp_port() { return udp_port; }

void HesaiPointCloudGPU::set_ptc_port(int p_port) { ptc_port = p_port; }
int HesaiPointCloudGPU::get_ptc_port() { return ptc_port; }

void HesaiPointCloudGPU::set_correction_file_path(String p_path) { correction_file_path = p_path; }
String HesaiPointCloudGPU::get_correction_file_path() { return correction_file_path; }

int HesaiPointCloudGPU::get_max_points() { return max_points; }
