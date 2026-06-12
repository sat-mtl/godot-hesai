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

 protected:
  static void _bind_methods();

 public:

  ~HesaiPointCloudGPU() override;

  void _enter_tree() override;
  void _exit_tree() override;

  void start_stream(const String& lidar_ip);

  void start_pcap_stream(const String& pcap_path);
  void stop_stream();

  void set_rendering_device(RenderingDevice *p_rd);
  RenderingDevice *get_rendering_device();

  /**
   * returns the current GPU buffer's RID.
   * Do not store the RID, call this every time. if your node exists and reenters the tree
   * the *RID* will have changed and its not like copying a RID is expensive.
   */
  RID get_point_buffer_rid();
  uint32_t get_num_points();

  // Called on the render thread; do not call directly.
  void update_point_cloud_buffer();

  /**
   * this will not restart the stream.
   */
  void set_udp_port(int p_port);
  int get_udp_port();
  /**
   * this will not restart the stream.
   */
  void set_ptc_port(int p_port);
  int get_ptc_port();
  void set_correction_file_path(String p_path);
  String get_correction_file_path();
  void set_max_points(int p_max);
  int get_max_points();

 private:
  using PointType = hesai::lidar::LidarPointXYZI;
  void start_stream_from_params(hesai::lidar::DriverParam &param);
  void allocate_point_cloud_buffer();
  void on_lidar_frame(const hesai::lidar::LidarDecodedFrame<PointType> &frame);

  RID point_buffer{};
  RenderingDevice *rd = nullptr;

  // Staging buffers written by the SDK thread, read on the render thread.
  std::mutex frame_mutex;
  PackedByteArray point_bytes{};
  std::atomic<uint32_t> current_points_num{0};

  std::unique_ptr<hesai::lidar::HesaiLidarSdk<PointType>> sdk;

  // Properties
  int udp_port = 2368;
  int ptc_port = 9347;
  // TODO someday: auto-include the correction files for at least the P40
  String correction_file_path = "";
  // this is enough for a P40. Need to validate that this value is high enough if using another model.
  const int max_points = 100000;
};
