#!/usr/bin/bash

# Requires clang++ / clang. Works on Arch Linux (same toolchain as godot-orbbec).

if [[ -z "${BUILD_RELEASE}" ]]; then
    cmake -B build \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
        -DCMAKE_CXX_COMPILER="clang++" \
        -DCMAKE_C_COMPILER="clang"
else
    cmake -B build \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
        -DCMAKE_CXX_COMPILER="clang++" \
        -DCMAKE_C_COMPILER="clang" \
        -DGODOTCPP_TARGET="template_release" \
        -DCMAKE_BUILD_TYPE=Release
fi

make -sC build -j"$(nproc)"

cp build/bin/"${GODOTCPP_PLATFORM:-linux}"/godot-hesai.linux.template* ./bin/linux/ 2>/dev/null || true
