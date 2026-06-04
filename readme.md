# godot-hesai

HesaiLidar_SDK_2.0 wrapper GDExtension for godot 4.5+

Exposes a `HesaiPointCloudGPU` node which connect to a hesai lidar or use a PCAP file to relay frames of depth data to a GPU SSBO with the `point_cloud_frame` signal.

## Current status

Tested on linux with the PCAP playback only.

## Building

### linux
```
git submodule update --recursive --init
./build.sh # BUILD_RELEASE=1 ./build.sh for release build
```

### windows
Untested.

### LLM Disclaimer

The initial code for this repository was generated using Claude code based on godot-orbbec. The code was then reviewed and partly rewritten by a human.
