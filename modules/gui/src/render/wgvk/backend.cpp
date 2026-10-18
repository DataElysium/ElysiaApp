#include "../backend.hpp"
#define GLFW_INCLUDE_NONE
#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#endif
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <imgui_impl_glfw.h>
#include <imgui_impl_wgpu.h>
#include <stdexcept>
#include <string>

namespace elysia::gui::render::backend {
namespace {
struct State {
  GLFWwindow *window = nullptr;
  WGPUInstance instance = nullptr;
  WGPUAdapter adapter = nullptr;
  WGPUDevice device = nullptr;
  WGPUQueue queue = nullptr;
  WGPUSurface surface = nullptr;
  WGPUSurfaceConfiguration config{};
  WGPUTexture frame = nullptr;
  std::string error;
  bool imgui = false, glfw = false;
} state;
std::string message(WGPUStringView text) {
  return text.data ? std::string(text.data,
                                 text.length == WGPU_STRLEN
                                     ? std::char_traits<char>::length(text.data)
                                     : text.length)
                   : "unknown error";
}
void check() {
  if (!state.error.empty())
    throw std::runtime_error("WGVK: " + state.error);
}
void wait(WGPUFuture future) {
  WGPUFutureWaitInfo info{future, false};
  if (wgpuInstanceWaitAny(state.instance, 1, &info, 5'000'000'000ull) !=
          WGPUWaitStatus_Success ||
      !info.completed)
    throw std::runtime_error("WGVK GPU operation timed out");
  check();
}
void configure(int width, int height) {
  state.config.width = uint32_t(width);
  state.config.height = uint32_t(height);
  wgpuSurfaceConfigure(state.surface, &state.config);
  check();
}
WGPUSurface create_surface(GLFWwindow *window) {
  WGPUSurfaceDescriptor desc{};
#if defined(_WIN32)
  WGPUSurfaceSourceWindowsHWND native{};
  native.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
  native.hinstance = GetModuleHandle(nullptr);
  native.hwnd = glfwGetWin32Window(window);
  desc.nextInChain = &native.chain;
  return wgpuInstanceCreateSurface(state.instance, &desc);
#elif defined(__linux__)
  if (glfwGetPlatform() == GLFW_PLATFORM_X11) {
    WGPUSurfaceSourceXlibWindow native{};
    native.chain.sType = WGPUSType_SurfaceSourceXlibWindow;
    native.display = glfwGetX11Display();
    native.window = glfwGetX11Window(window);
    desc.nextInChain = &native.chain;
    return wgpuInstanceCreateSurface(state.instance, &desc);
  }
#endif
  throw std::runtime_error(
      "WGVK trial supports Windows and X11 windows (including Xwayland)");
}
} // namespace
const char *name() { return "WGVK / Vulkan"; }
void window_hints() { glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); }
void initialize(GLFWwindow *window, bool vsync) {
  state.window = window;
  try {
    const WGPUInstanceFeatureName features[] = {
        WGPUInstanceFeatureName_TimedWaitAny};
    WGPUInstanceDescriptor instance{};
    instance.requiredFeatures = features;
    instance.requiredFeatureCount = 1;
    state.instance = wgpuCreateInstance(&instance);
    if (!state.instance)
      throw std::runtime_error("Cannot create WGVK instance");
    state.surface = create_surface(window);
    if (!state.surface)
      throw std::runtime_error("Cannot create WGVK surface");
    WGPURequestAdapterOptions options{};
    options.featureLevel = WGPUFeatureLevel_Core;
    options.compatibleSurface = state.surface;
    WGPURequestAdapterCallbackInfo adapter_callback{};
    adapter_callback.mode = WGPUCallbackMode_WaitAnyOnly;
    adapter_callback.callback = [](WGPURequestAdapterStatus status,
                                   WGPUAdapter adapter, WGPUStringView text,
                                   void *, void *) {
      if (status == WGPURequestAdapterStatus_Success)
        state.adapter = adapter;
      else
        state.error = message(text);
    };
    wait(
        wgpuInstanceRequestAdapter(state.instance, &options, adapter_callback));
    if (!state.adapter)
      throw std::runtime_error("No compatible Vulkan adapter");
    WGPUDeviceDescriptor device{};
    device.uncapturedErrorCallbackInfo.callback =
        [](const WGPUDevice *, WGPUErrorType, WGPUStringView text, void *,
           void *) {
          state.error = message(text);
          std::fprintf(stderr, "WGVK error: %s\n", state.error.c_str());
        };
    device.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowProcessEvents;
    device.deviceLostCallbackInfo.callback =
        [](const WGPUDevice *, WGPUDeviceLostReason reason, WGPUStringView text,
           void *, void *) {
          if (reason != WGPUDeviceLostReason_Destroyed)
            state.error = message(text);
        };
    WGPURequestDeviceCallbackInfo device_callback{};
    device_callback.mode = WGPUCallbackMode_WaitAnyOnly;
    device_callback.callback = [](WGPURequestDeviceStatus status,
                                  WGPUDevice device, WGPUStringView text,
                                  void *, void *) {
      if (status == WGPURequestDeviceStatus_Success)
        state.device = device;
      else
        state.error = message(text);
    };
    wait(wgpuAdapterRequestDevice(state.adapter, &device, device_callback));
    if (!state.device)
      throw std::runtime_error("Cannot create WGVK device");
    state.queue = wgpuDeviceGetQueue(state.device);
    WGPUSurfaceCapabilities caps{};
    if (wgpuSurfaceGetCapabilities(state.surface, state.adapter, &caps) !=
        WGPUStatus_Success)
      throw std::runtime_error("Cannot query WGVK surface");
    // Linear output matches the existing OpenGL UI; do not silently change its
    // colors.
    for (size_t i = 0; i < caps.formatCount; ++i)
      if (caps.formats[i] == WGPUTextureFormat_BGRA8Unorm ||
          caps.formats[i] == WGPUTextureFormat_RGBA8Unorm) {
        state.config.format = caps.formats[i];
        break;
      }
    state.config.device = state.device;
    state.config.usage = WGPUTextureUsage_RenderAttachment |
                         (caps.usages & WGPUTextureUsage_CopySrc);
    state.config.presentMode = WGPUPresentMode_Fifo;
    if (!vsync)
      for (size_t i = 0; i < caps.presentModeCount; ++i)
        if (caps.presentModes[i] == WGPUPresentMode_Immediate)
          state.config.presentMode = WGPUPresentMode_Immediate;
    state.config.alphaMode = caps.alphaModes[0];
    wgpuSurfaceCapabilitiesFreeMembers(caps);
    if (!vsync && state.config.presentMode != WGPUPresentMode_Immediate)
      throw std::runtime_error(
          "WGVK surface cannot disable vsync for this benchmark");
    if (state.config.format == WGPUTextureFormat_Undefined)
      throw std::runtime_error("WGVK surface lacks a linear RGBA8 format");
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    configure(std::max(1, width), std::max(1, height));
    state.glfw = ImGui_ImplGlfw_InitForOther(window, true);
    ImGui_ImplWGPU_InitInfo info;
    info.Device = state.device;
    info.RenderTargetFormat = state.config.format;
    state.imgui = ImGui_ImplWGPU_Init(&info);
    if (!state.glfw || !state.imgui)
      throw std::runtime_error("Cannot initialize WGVK ImGui backend");
    check();
  } catch (...) {
    shutdown();
    throw;
  }
}
void shutdown() {
  if (state.frame)
    wgpuTextureRelease(state.frame);
  if (state.imgui)
    ImGui_ImplWGPU_Shutdown();
  if (state.glfw)
    ImGui_ImplGlfw_Shutdown();
  if (state.surface) {
    wgpuSurfaceUnconfigure(state.surface);
    wgpuSurfaceRelease(state.surface);
  }
  if (state.queue)
    wgpuQueueRelease(state.queue);
  if (state.device)
    wgpuDeviceRelease(state.device);
  if (state.adapter)
    wgpuAdapterRelease(state.adapter);
  if (state.instance)
    wgpuInstanceRelease(state.instance);
  state = {};
}
void new_frame() {
  wgpuInstanceProcessEvents(state.instance);
  check();
  ImGui_ImplWGPU_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  check();
}
bool draw(ImDrawData *data) {
  int width, height;
  glfwGetFramebufferSize(state.window, &width, &height);
  if (width <= 0 || height <= 0)
    return false;
  if (uint32_t(width) != state.config.width ||
      uint32_t(height) != state.config.height)
    configure(width, height);
  WGPUSurfaceTexture surface{};
  wgpuSurfaceGetCurrentTexture(state.surface, &surface);
  if (surface.status == WGPUSurfaceGetCurrentTextureStatus_Outdated ||
      surface.status == WGPUSurfaceGetCurrentTextureStatus_Lost) {
    if (surface.texture)
      wgpuTextureRelease(surface.texture);
    configure(width, height);
    return false;
  }
  if (surface.status == WGPUSurfaceGetCurrentTextureStatus_Timeout)
    return false;
  if (ImGui_ImplWGPU_IsSurfaceStatusError(surface.status))
    throw std::runtime_error("Cannot acquire WGVK frame");
  state.frame = surface.texture;
  auto view = wgpuTextureCreateView(state.frame, nullptr);
  auto encoder = wgpuDeviceCreateCommandEncoder(state.device, nullptr);
  WGPURenderPassColorAttachment color{};
  color.view = view;
  color.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
  color.loadOp = WGPULoadOp_Clear;
  color.storeOp = WGPUStoreOp_Store;
  color.clearValue = {.02, .05, .07, 1};
  WGPURenderPassDescriptor pass_desc{};
  pass_desc.colorAttachmentCount = 1;
  pass_desc.colorAttachments = &color;
  auto pass = wgpuCommandEncoderBeginRenderPass(encoder, &pass_desc);
  ImGui_ImplWGPU_RenderDrawData(data, pass);
  wgpuRenderPassEncoderEnd(pass);
  wgpuRenderPassEncoderRelease(pass);
  auto command = wgpuCommandEncoderFinish(encoder, nullptr);
  wgpuCommandEncoderRelease(encoder);
  wgpuQueueSubmit(state.queue, 1, &command);
  wgpuCommandBufferRelease(command);
  wgpuTextureViewRelease(view);
  check();
  return true;
}
void present() {
  if (!state.frame)
    return;
  const auto status = wgpuSurfacePresent(state.surface);
  wgpuTextureRelease(state.frame);
  state.frame = nullptr;
  check();
  if (status != WGPUStatus_Success)
    throw std::runtime_error("WGVK presentation failed");
}
Texture::~Texture() {
  if (handle_)
    wgpuTextureViewRelease(reinterpret_cast<WGPUTextureView>(handle_));
}
void Texture::upload(int width, int height, std::span<const uint8_t> rgba) {
  if (width <= 0 || height <= 0 || rgba.size() != size_t(width) * height * 4)
    throw std::invalid_argument("Invalid RGBA8 image");
  if (!state.device)
    throw std::logic_error(
        "Upload images after the GUI surface is initialized");
  WGPUTextureDescriptor desc{};
  desc.size = {uint32_t(width), uint32_t(height), 1};
  desc.format = WGPUTextureFormat_RGBA8Unorm;
  desc.dimension = WGPUTextureDimension_2D;
  desc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
  desc.mipLevelCount = desc.sampleCount = 1;
  auto texture = wgpuDeviceCreateTexture(state.device, &desc);
  check();
  if (!texture)
    throw std::runtime_error("WGVK texture allocation failed");
  WGPUTexelCopyTextureInfo copy{};
  copy.texture = texture;
  copy.aspect = WGPUTextureAspect_All;
  WGPUTexelCopyBufferLayout layout{};
  layout.bytesPerRow = uint32_t(width) * 4;
  layout.rowsPerImage = height;
  wgpuQueueWriteTexture(state.queue, &copy, rgba.data(), rgba.size(), &layout,
                        &desc.size);
  auto view = wgpuTextureCreateView(texture, nullptr);
  wgpuTextureRelease(texture);
  check();
  if (!view)
    throw std::runtime_error("WGVK texture view creation failed");
  if (handle_)
    wgpuTextureViewRelease(reinterpret_cast<WGPUTextureView>(handle_));
  handle_ = reinterpret_cast<uintptr_t>(view);
  check();
}
void capture(const std::filesystem::path &path) {
  if (!path.parent_path().empty())
    std::filesystem::create_directories(path.parent_path());
  if (!state.frame || !(state.config.usage & WGPUTextureUsage_CopySrc))
    throw std::runtime_error("WGVK surface does not support capture");
  const uint32_t pitch = (state.config.width * 4 + 255) & ~255u;
  WGPUBufferDescriptor desc{};
  desc.size = size_t(pitch) * state.config.height;
  desc.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
  auto buffer = wgpuDeviceCreateBuffer(state.device, &desc);
  auto encoder = wgpuDeviceCreateCommandEncoder(state.device, nullptr);
  WGPUTexelCopyTextureInfo source{};
  source.texture = state.frame;
  source.aspect = WGPUTextureAspect_All;
  WGPUTexelCopyBufferInfo destination{};
  destination.buffer = buffer;
  destination.layout.bytesPerRow = pitch;
  destination.layout.rowsPerImage = state.config.height;
  WGPUExtent3D extent{state.config.width, state.config.height, 1};
  wgpuCommandEncoderCopyTextureToBuffer(encoder, &source, &destination,
                                        &extent);
  auto command = wgpuCommandEncoderFinish(encoder, nullptr);
  wgpuCommandEncoderRelease(encoder);
  wgpuQueueSubmit(state.queue, 1, &command);
  wgpuCommandBufferRelease(command);
  WGPUMapAsyncStatus status = WGPUMapAsyncStatus_Error;
  WGPUBufferMapCallbackInfo callback{};
  callback.mode = WGPUCallbackMode_WaitAnyOnly;
  callback.userdata1 = &status;
  callback.callback = [](WGPUMapAsyncStatus value, WGPUStringView, void *result,
                         void *) {
    *static_cast<WGPUMapAsyncStatus *>(result) = value;
  };
  wait(wgpuBufferMapAsync(buffer, WGPUMapMode_Read, 0, desc.size, callback));
  if (status != WGPUMapAsyncStatus_Success) {
    wgpuBufferRelease(buffer);
    throw std::runtime_error("WGVK capture readback failed");
  }
  const auto *pixels = static_cast<const uint8_t *>(
      wgpuBufferGetConstMappedRange(buffer, 0, desc.size));
  std::ofstream out(path, std::ios::binary);
  out << "P6\n" << extent.width << " " << extent.height << "\n255\n";
  const bool bgra = state.config.format == WGPUTextureFormat_BGRA8Unorm;
  for (uint32_t y = 0; y < extent.height; ++y)
    for (uint32_t x = 0; x < extent.width; ++x) {
      const auto *p = pixels + size_t(y) * pitch + x * 4;
      const char rgb[3] = {char(p[bgra ? 2 : 0]), char(p[1]),
                           char(p[bgra ? 0 : 2])};
      out.write(rgb, 3);
    }
  wgpuBufferUnmap(buffer);
  wgpuBufferRelease(buffer);
  if (!out)
    throw std::runtime_error("Cannot write WGVK render capture");
}
} // namespace elysia::gui::render::backend
