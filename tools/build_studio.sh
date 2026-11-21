#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="${ROOT_DIR}/out"
BUILD_DIR="${TMPDIR:-/tmp}/quickvn_build"

prepare_studio_dir() {
    local os_name="$1"
    mkdir -p "${OUT_DIR}/${os_name}/proj"
    mkdir -p "${OUT_DIR}/${os_name}/runtimes/linux"
    mkdir -p "${OUT_DIR}/${os_name}/runtimes/windows"
    mkdir -p "${OUT_DIR}/${os_name}/runtimes/macos"
    mkdir -p "${OUT_DIR}/${os_name}/runtimes/android"
    mkdir -p "${OUT_DIR}/${os_name}/runtimes/ios"
}

distribute_runtime() {
    local plat="$1"
    local bin_path="$2"
    if [[ -f "${bin_path}" ]]; then
        for host in linux windows macos; do
            if [[ -d "${OUT_DIR}/${host}/runtimes/${plat}" ]]; then
                cp -f "${bin_path}" "${OUT_DIR}/${host}/runtimes/${plat}/"
            fi
        done
    fi
}

HOST_OS="$(uname -s)"

if [[ "${HOST_OS}" == "Linux" ]]; then
    prepare_studio_dir linux
    cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}/linux" -DCMAKE_BUILD_TYPE=Release
    cmake --build "${BUILD_DIR}/linux" -j
    cp -f "${BUILD_DIR}/linux/QuickVN_Studio" "${OUT_DIR}/linux/QuickVN_Studio"
    distribute_runtime linux "${BUILD_DIR}/linux/quickjs_vn"

    if [[ -f "${ROOT_DIR}/toolchains/mingw-w64.cmake" ]] && command -v x86_64-w64-mingw32-g++ >/dev/null 2>&1; then
        if cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}/windows" -DCMAKE_TOOLCHAIN_FILE="${ROOT_DIR}/toolchains/mingw-w64.cmake" -DCMAKE_BUILD_TYPE=Release; then
            cmake --build "${BUILD_DIR}/windows" -j
            prepare_studio_dir windows
            cp -f "${BUILD_DIR}/windows/QuickVN_Studio.exe" "${OUT_DIR}/windows/QuickVN_Studio.exe"
            distribute_runtime linux "${BUILD_DIR}/linux/quickjs_vn"
            distribute_runtime windows "${BUILD_DIR}/windows/quickjs_vn.exe"
        fi
    fi
elif [[ "${HOST_OS}" == "Darwin" ]]; then
    prepare_studio_dir macos
    cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}/macos" -DCMAKE_BUILD_TYPE=Release
    cmake --build "${BUILD_DIR}/macos" -j
    cp -f "${BUILD_DIR}/macos/QuickVN_Studio" "${OUT_DIR}/macos/QuickVN_Studio"
    distribute_runtime macos "${BUILD_DIR}/macos/quickjs_vn"

    if command -v xcodebuild >/dev/null 2>&1; then
        cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}/ios" -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos -DQVN_BUILD_STUDIO=OFF
        cmake --build "${BUILD_DIR}/ios" --config Release --target quickjs_vn
        distribute_runtime ios "${BUILD_DIR}/ios/Release-iphoneos/quickjs_vn.app/quickjs_vn"
    fi
fi

if [[ -n "${ANDROID_NDK:-}" ]] && [[ -f "${ANDROID_NDK}/build/cmake/android.toolchain.cmake" ]]; then
    if cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}/android" \
        -DCMAKE_TOOLCHAIN_FILE="${ANDROID_NDK}/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI=arm64-v8a \
        -DANDROID_PLATFORM=android-24 \
        -DQVN_BUILD_STUDIO=OFF; then
        cmake --build "${BUILD_DIR}/android" -j
        distribute_runtime android "${BUILD_DIR}/android/libquickjs_vn.so"
    fi
fi

echo "Done. Output in ${OUT_DIR}"