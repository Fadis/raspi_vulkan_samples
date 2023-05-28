#!/bin/bash

function die() {
  echo ${1}
  exit 1
}
ARCH=$(uname -m)

DEST=${HOME}/vulkan
BUILD_DIR=`pwd`/build_vulkan
VULKAN_PATH=${DEST}
export PATH=${PATH}:${VULKAN_PATH}/bin
export PKG_CONFIG_PATH=${VULKAN_PATH}/lib/pkgconfig:${VULKAN_PATH}/lib/${ARCH}-linux-gnu/pkgconfig
export LD_LIBRARY_PATH=${VULKAN_PATH}/lib
export VK_ICD_FILENAMES=${VULKAN_PATH}/share/vulkan/icd.d/broadcom_icd.${ARCH}.json
export VK_LAYER_PATH=${VULKAN_PATH}/share/vulkan/explicit_layer.d/
export FXGLTF_ROOT=${VULKAN_PATH}

echo "インストール先: ${DEST}"
echo "ビルドディレクトリ: ${BUILD_DIR}"

if [ ! -e "${BUILD_DIR}" ]
then
  mkdir -p ${BUILD_DIR} || die "${BUILD_DIR}を作る事ができません。ディレクトリの権限を確認してください。"
fi
if [ -e "${DEST}" ]
then
  die "インストール先${DEST}にディレクトリが既にあります。先に削除してください。"
fi
cd ${BUILD_DIR} || die "${BUILD_DIR}に移動できません。ディレクトリの権限を確認してください。"
touch check_writable || die "${BUILD_DIR}にファイルを作れません。ディレクトリの権限を確認してください。"
if [ ! -e "${BUILD_DIR}/binary" ]
then
  mkdir -p ${BUILD_DIR}/binary || die "${BUILD_DIR}/binaryを作る事ができません。ディレクトリの権限を確認してください。"
fi
 
echo "依存するRaspberry Pi OSのパッケージをインストールします。(sudoするためにパスワードを求められます)"
sudo apt update
sudo apt install python3-mako libxshmfence-dev libxxf86vm-dev build-essential x11proto-randr-dev x11proto-present-dev libclc-dev libelf-dev libva-dev libx11-xcb-dev libxext-dev libxdamage-dev libxfixes-dev x11proto-dri3-dev libx11-dev libxcb-glx0-dev libssl-dev libgnutls28-dev x11proto-dri2-dev libffi-dev x11proto-xext-dev libxcb1-dev libxcb-*dev xutils-dev libpthread-stubs0-dev libpciaccess-dev libxcb-cursor-dev libxkbcommon-dev xutils-dev libxcb-xinerama0-dev libxinerama-dev libxcursor-dev libxcb-randr0-dev libxrandr-dev cmake meson libzstd-dev bison flex llvm llvm-dev clang libboost-all-dev nlohmann-json3-dev clang-format libxi-dev libopenimageio-dev catch2 libopenexr-dev libglm-dev google-mock wget tar xz-utils ninja-build libharfbuzz-dev libinput-dev libsystemd-dev libsdbus-c++-dev || die "依存するパッケージをインストールできません。"

echo "Vulkan-Loaderをcloneします。"
if [ ! -e "Vulkan-Loader" ]
then
  git clone -4 --depth 1 -b v1.2.203 https://github.com/KhronosGroup/Vulkan-Loader.git || die "Vulkan-Loaderのcloneが失敗しました。インターネット接続を確認してください。"
  python3 ${BUILD_DIR}/Vulkan-Loader/scripts/update_deps.py --dir ${BUILD_DIR}/Vulkan-Loader/external --arch x64 --config release || die "Vulkan-Loaderが依存するリポジトリをcheckoutできません。"
fi
if [ -e "${BUILD_DIR}/binary/Vulkan-Loader" ]
then
  rm -rf "${BUILD_DIR}/binary/Vulkan-Loader" || die "Vulkan-Loaderの古いビルドディレクトリを削除できません。"
fi
mkdir "${BUILD_DIR}/binary/Vulkan-Loader" || die "Vulkan-Loaderのビルドディレクトリを作れません。"
cd "${BUILD_DIR}/binary/Vulkan-Loader"
cmake ${BUILD_DIR}/Vulkan-Loader/ -DCMAKE_INSTALL_PREFIX=${DEST} -DCMAKE_BUILD_TYPE=Release -DBUILD_WSI_WAYLAND_SUPPORT=OFF || die "Vulkan-Loaderのビルドの準備が失敗しました。"
make || die "Vulkan-Loaderをビルドできません。"
make install || die "Vulkan-Loaderをインストールできません。"
cd ../../

echo "Vulkan-Headersをcloneします。"
if [ ! -e "Vulkan-Headers" ]
then
  git clone -4 --depth 1 -b v1.2.203 https://github.com/KhronosGroup/Vulkan-Headers.git || die "Vulkan-Headersのcloneが失敗しました。インターネット接続を確認してください。"
fi
if [ -e "${BUILD_DIR}/binary/Vulkan-Headers" ]
then
  rm -rf "${BUILD_DIR}/binary/Vulkan-Headers" || die "Vulkan-Headersの古いビルドディレクトリを削除できません。"
fi
mkdir "${BUILD_DIR}/binary/Vulkan-Headers" || die "Vulkan-Headersのビルドディレクトリを作れません。"
cd "${BUILD_DIR}/binary/Vulkan-Headers"
cmake ${BUILD_DIR}/Vulkan-Headers/ -DCMAKE_INSTALL_PREFIX=${DEST} -DCMAKE_BUILD_TYPE=Release || die "Vulkan-Headersのビルドの準備が失敗しました。"
make || die "Vulkan-Headersをビルドできません。"
make install || die "Vulkan-Headersをインストールできません。"
cd ../../

echo "shadercをcloneします。"
if [ ! -e "shaderc" ]
then
  git clone -4 --depth 1 -b v2022.1 https://github.com/google/shaderc.git || die "shadercのcloneが失敗しました。インターネット接続を確認してください。"
  cd shaderc
  ./utils/git-sync-deps || die "shadercが依存するリポジトリをcheckoutできません。"
  cd ../
fi
if [ -e "${BUILD_DIR}/binary/shaderc" ]
then
  rm -rf "${BUILD_DIR}/binary/shaderc" || die "shadercの古いビルドディレクトリを削除できません。"
fi
mkdir "${BUILD_DIR}/binary/shaderc" || die "shadercのビルドディレクトリを作れません。"
cd "${BUILD_DIR}/binary/shaderc"
cmake ${BUILD_DIR}/shaderc/ -DCMAKE_INSTALL_PREFIX=${DEST} -DCMAKE_BUILD_TYPE=Release -DUSE_ROBIN_HOOD_HASHING=OFF || die "sharedcのビルドの準備が失敗しました。"
make || die "shadercをビルドできません。"
make install || die "shadercをインストールできません。"
cd ../../

echo "Vulkan-ValidationLayersをcloneします。"
if [ ! -e "Vulkan-ValidationLayers" ]
then
  git clone -4 --depth 1 -b v1.2.203 https://github.com/KhronosGroup/Vulkan-ValidationLayers.git || die "Vulkan-ValidationLayerのcloneが失敗しました。インターネット接続を確認してください。"
  python3 ${BUILD_DIR}/Vulkan-ValidationLayers/scripts/update_deps.py --dir ${BUILD_DIR}/Vulkan-ValidationLayer/external --arch x64 --config release || die "Vulkan-ValidationLayerが依存するリポジトリをcheckoutできません。"
fi
if [ -e "${BUILD_DIR}/binary/Vulkan-ValidationLayers" ]
then
  rm -rf "${BUILD_DIR}/binary/Vulkan-ValidationLayers" || die "Vulkan-ValidationLayerの古いビルドディレクトリを削除できません。"
fi
mkdir "${BUILD_DIR}/binary/Vulkan-ValidationLayers" || die "Vulkan-ValidationLayerのビルドディレクトリを作れません。"
cd "${BUILD_DIR}/binary/Vulkan-ValidationLayers"
cmake ${BUILD_DIR}/Vulkan-ValidationLayers/ -DCMAKE_INSTALL_PREFIX=${DEST} -DCMAKE_BUILD_TYPE=Release -DUSE_ROBIN_HOOD_HASHING=OFF -DBUILD_WSI_WAYLAND_SUPPORT=OFF || "Vulkan-ValidationLayerのビルドの準備が失敗しました。"
make || die "Vulkan-ValidationLayersをビルドできません。"
make install || die "Vulkan-ValidationLayersをインストールできません。"
cd ../../

echo "Vulkan-Toolsをcloneします。"
if [ ! -e "Vulkan-Tools" ]
then
  git clone -4 --depth 1 -b v1.2.203 https://github.com/KhronosGroup/Vulkan-Tools.git || die "Vulkan-Toolsのcloneが失敗しました。インターネット接続を確認してください。"
  python3 ${BUILD_DIR}/Vulkan-Tools/scripts/update_deps.py --dir ${BUILD_DIR}/Vulkan-Tools/external --arch x64 --config release || die "Vulkan-Toolsが依存するリポジトリをcheckoutできません。"
fi
if [ -e "${BUILD_DIR}/binary/Vulkan-Tools" ]
then
  rm -rf "${BUILD_DIR}/binary/Vulkan-Tools" || die "Vulkan-Toolsの古いビルドディレクトリを削除できません。"
fi
mkdir "${BUILD_DIR}/binary/Vulkan-Tools" || die "Vulkan-Toolsのビルドディレクトリを作れません。"
cd "${BUILD_DIR}/binary/Vulkan-Tools"
cmake ${BUILD_DIR}/Vulkan-Tools/ -DCMAKE_INSTALL_PREFIX=${DEST} -DCMAKE_BUILD_TYPE=Release -DBUILD_WSI_WAYLAND_SUPPORT=OFF -DGLSLANG_INSTALL_DIR=${DEST} || die "Vulkan-Toolsのビルドの準備が失敗しました。"
make || die "Vulkan-Toolsをビルドできません。"
make install || die "Vulkan-Toolsをインストールできません。"
cd ../../

echo "libdrmをcloneします。"
if [ ! -e "${BUILD_DIR}/libdrm-2.4.112" ]
then
  wget https://dri.freedesktop.org/libdrm/libdrm-2.4.112.tar.xz || die "libdrmをダウンロードできません。インターネット接続を確認してください。"
  tar xJpf libdrm-2.4.112.tar.xz || die "libdrmを展開できません。"
fi
if [ -e "${BUILD_DIR}/binary/libdrm" ]
then
  rm -rf "${BUILD_DIR}/binary/libdrm" || die "libdrmの古いビルドディレクトリを削除できません。"
fi
mkdir "${BUILD_DIR}/binary/libdrm" || die "libdrmのビルドディレクトリを作れません。"
cd "${BUILD_DIR}/binary/libdrm"
meson --prefix=${DEST} --libdir lib -Dbuildtype=release "${BUILD_DIR}/libdrm-2.4.112" -Damdgpu=false -Dcairo-tests=false -Dfreedreno=false -Dintel=false -Dnouveau=false -Dradeon=false -Dvc4=true -Dvmwgfx=false || die "libdrmのビルドの準備が失敗しました。"
ninja || die "libdrmをビルドできません。"
ninja install || die "libdrmをインストールできません。"
cd ../../

echo "mesaをcloneします。"
if [ ! -e "${BUILD_DIR}/mesa" ]
then
  git clone -4 --depth 1 -b wip/igalia/v3dv-conformance-1.2 https://gitlab.freedesktop.org/apinheiro/mesa.git mesa || die "mesaのcloneが失敗しました。インターネット接続を確認してください。"
fi
if [ -e "${BUILD_DIR}/binary/mesa" ]
then
  rm -rf "${BUILD_DIR}/binary/mesa" || die "mesaの古いビルドディレクトリを削除できません。"
fi
mkdir "${BUILD_DIR}/binary/mesa" || die "mesaのビルドディレクトリを作れません。"
cd "${BUILD_DIR}/binary/mesa"
meson --prefix=${DEST} --libdir lib -Dbuildtype=release -Dplatforms=x11 -Dvulkan-drivers=broadcom -Dgallium-drivers=v3d,kmsro,vc4 "${BUILD_DIR}/mesa" || die "mesaのビルドの準備が失敗しました。"
ninja || die "mesaをビルドできません。"
ninja install || die "mesaをインストールできません。"
cd ../../

echo "fx-gltfをcloneします。"
if [ ! -e "${BUILD_DIR}/fx-gltf-2.0.0" ]
then
  wget https://github.com/jessey-git/fx-gltf/archive/refs/tags/v2.0.0.tar.gz -4 || die "fx-gltfをダウンロードできません。インターネット接続を確認してください。"
  tar xzpf v2.0.0.tar.gz || die "fx-gltfを展開できません。"
fi
if [ -e "${BUILD_DIR}/binary/fx-gltf" ]
then
  rm -rf "${BUILD_DIR}/binary/fx-gltf" || die "fx-gltfの古いビルドディレクトリを削除できません。"
fi
mkdir "${BUILD_DIR}/binary/fx-gltf" || die "fx-gltfのビルドディレクトリを作れません。"
cd "${BUILD_DIR}/binary/fx-gltf" || die "fx-gltfのビルドの準備が失敗しました。"
CXXFLAGS=-Wno-narrowing cmake ${BUILD_DIR}/fx-gltf-2.0.0/ -DCMAKE_INSTALL_PREFIX=${DEST} -DCMAKE_BUILD_TYPE=Release || die "fx-gltfのビルドの準備が失敗しました。"
make || die "fx-gltfをビルドできません。"
make install || die "fx-gltfをインストールできません。"
cd ../../

echo "vulkan2jsonをcloneします。"
if [ ! -e "vulkan2json" ]
then
  git clone -4 --depth 1 https://github.com/Fadis/vulkan2json.git || die "vulkan2jsonのcloneが失敗しました。インターネット接続を確認してください。"
fi
if [ -e "${BUILD_DIR}/binary/vulkan2json" ]
then
  rm -rf "${BUILD_DIR}/binary/vulkan2json" || die "vulkan2jsonの古いビルドディレクトリを削除できません。"
fi
mkdir "${BUILD_DIR}/binary/vulkan2json" || die "vulkan2jsonのビルドディレクトリを作れません。"
cd "${BUILD_DIR}/binary/vulkan2json"
cmake ${BUILD_DIR}/vulkan2json/ -DCMAKE_INSTALL_PREFIX=${DEST} -DCMAKE_BUILD_TYPE=Release || die "vulkan2jsonのビルドの準備が失敗しました。"
make || die "vulkan2jsonをビルドできません。"
make install || die "vulkan2jsonをインストールできません。"
cd ../../

echo "glfwをcloneします。"
if [ ! -e "${BUILD_DIR}/glfw-3.3.8" ]
then
  wget https://github.com/glfw/glfw/archive/refs/tags/3.3.8.tar.gz || die "glfwをダウンロードできません。インターネット接続を確認してください。"
  tar xzpf 3.3.8.tar.gz || die "glfwを展開できません。"
fi
if [ -e "${BUILD_DIR}/binary/glfw" ]
then
  rm -rf "${BUILD_DIR}/binary/glfw" || die "glfwの古いビルドディレクトリを削除できません。"
fi
mkdir "${BUILD_DIR}/binary/glfw" || die "glfwのビルドディレクトリを作れません。"
cd "${BUILD_DIR}/binary/glfw"
cmake ${BUILD_DIR}/glfw-3.3.8 -DCMAKE_INSTALL_PREFIX=${DEST} -DCMAKE_BUILD_TYPE=Release -DGLFW_USE_WAYLAND=OFF || die "glfwのビルドの準備が失敗しました。"
make || die "glfwをビルドできません。"
make install || die "glfwをインストールできません。"
ln -s ${DEST}/lib/libglfw3.a ${DEST}/lib/libglfw.a
cd ../../

echo "hdmapをcloneします。"
if [ ! -e "hdmap" ]
then
  git clone -4 --depth 1 https://github.com/Fadis/hdmap.git || die "hdmapのcloneが失敗しました。インターネット接続を確認してください。"
fi
if [ -e "${BUILD_DIR}/binary/hdmap" ]
then
  rm -rf "${BUILD_DIR}/binary/hdmap" || die "hdmapの古いビルドディレクトリを削除できません。"
fi
mkdir "${BUILD_DIR}/binary/hdmap" || die "hdmapのビルドディレクトリを作れません。"
cd "${BUILD_DIR}/binary/hdmap"
cmake ${BUILD_DIR}/hdmap/ -DCMAKE_INSTALL_PREFIX=${DEST} -DCMAKE_BUILD_TYPE=Release || die "hdmapのビルドの準備が失敗しました。"
make || die "hdmapをビルドできません。"
make install || die "hdmapをインストールできません。"
cd ../../

if [ "${ARCH}" == "armv7l" ]
then
cat <<EOF >${BUILD_DIR}/astc-encoder.diff
diff -ubr astc-encoder-4.1.0.orig/Source/tinyexr.h astc-encoder-4.1.0/Source/tinyexr.h
--- astc-encoder-4.1.0.orig/Source/tinyexr.h    2022-08-18 05:38:36.000000000 +0900
+++ astc-encoder-4.1.0/Source/tinyexr.h 2022-08-31 20:04:41.768930805 +0900
@@ -11022,7 +11022,7 @@
     size_t total_data_len =
         size_t(data_width) * size_t(data_height) * size_t(num_channels);
     const bool total_data_len_overflown =
-        sizeof(void *) == 8 ? (total_data_len >= 0x4000000000) : false;
+        false;
     if ((total_data_len == 0) || total_data_len_overflown) {
         if (err) {
             std::stringstream ss;
EOF

fi
echo "astc-encoderをcloneします。"
if [ ! -e "${BUILD_DIR}/astc-encoder-4.1.0" ]
then
  wget https://github.com/ARM-software/astc-encoder/archive/refs/tags/4.1.0.tar.gz || die "astc-encoderをダウンロードできません。インターネット接続を確認してください。"
  tar xzpf 4.1.0.tar.gz || die "astc-encoderを展開できません。"
  if [ "${ARCH}" == "armv7l" ]
  then
    pushd "${BUILD_DIR}/astc-encoder-4.1.0"
      patch -p1 <${BUILD_DIR}/astc-encoder.diff
    popd
  fi
fi
if [ -e "${BUILD_DIR}/binary/astc-encoder" ]
then
  rm -rf "${BUILD_DIR}/binary/astc-encoder" || die "astc-encoderの古いビルドディレクトリを削除できません。"
fi
mkdir "${BUILD_DIR}/binary/astc-encoder" || die "astc-encoderのビルドディレクトリを作れません。"
cd "${BUILD_DIR}/binary/astc-encoder"
CFLAGS="-Wno-error" CXXFLAGS="-Wno-error" cmake ${BUILD_DIR}/astc-encoder-4.1.0 -DCMAKE_INSTALL_PREFIX=${DEST} -DCMAKE_BUILD_TYPE=Release || die "astc-encoderのビルドの準備が失敗しました。"
make || die "astc-encoderをビルドできません。"
make install || die "astc-encoderをインストールできません。"
cd ../../

echo "gctをcloneします。"
if [ ! -e "gct" ]
then
  git clone -4 --depth 1 --recursive https://github.com/Fadis/gct.git || die "gctのcloneが失敗しました。インターネット接続を確認してください。"
fi
if [ -e "${BUILD_DIR}/binary/gct" ]
then
  rm -rf "${BUILD_DIR}/binary/gct" || die "gctの古いビルドディレクトリを削除できません。"
fi
mkdir "${BUILD_DIR}/binary/gct" || die "gctのビルドディレクトリを作れません。"
cd "${BUILD_DIR}/binary/gct"
cmake ${BUILD_DIR}/gct/ -DCMAKE_INSTALL_PREFIX=${DEST} -DCMAKE_BUILD_TYPE=Release || die "gctのビルドの準備が失敗しました。"
make || die "gctをビルドできません。"
make install || die "gctをインストールできません。"
cd ../../

echo '#!/bin/bash' >${DEST}/bin/enable_vulkan
echo "VULKAN_PATH=${DEST}" >>${DEST}/bin/enable_vulkan
echo "ARCH=${ARCH}" >>${DEST}/bin/enable_vulkan

cat <<EOF >>${DEST}/bin/enable_vulkan

export PATH=\${PATH}:\${VULKAN_PATH}/bin
export PKG_CONFIG_PATH=\${VULKAN_PATH}/lib/pkgconfig:\${VULKAN_PATH}/lib/\${ARCH}-linux-gnu/pkgconfig
export LD_LIBRARY_PATH=\${VULKAN_PATH}/lib
export VK_ICD_FILENAMES=\${VULKAN_PATH}/share/vulkan/icd.d/broadcom_icd.\${ARCH}.json
export VK_LAYER_PATH=\${VULKAN_PATH}/share/vulkan/explicit_layer.d/
export FXGLTF_ROOT=\${VULKAN_PATH}
exec bash

EOF
chmod 700 ${DEST}/bin/enable_vulkan

