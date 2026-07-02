/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/win/d3d_device.h"

#include <utility>

#include "webrtc/system_wrappers/include/logging.h"

namespace webrtc {

using Microsoft::WRL::ComPtr;

D3dDevice::D3dDevice() = default;
D3dDevice::D3dDevice(const D3dDevice& other) = default;
D3dDevice::D3dDevice(D3dDevice&& other) = default;
D3dDevice::~D3dDevice() = default;

bool D3dDevice::Initialize(const ComPtr<IDXGIAdapter>& adapter) {
  dxgi_adapter_ = adapter;
  if (!dxgi_adapter_) {
    LOG(LS_WARNING) << "An empty IDXGIAdapter instance has been received.";
    return false;
  }

// We don't have access to the D3D11CreateDevice type in gfxWindowsPlatform.h,
// since it doesn't include d3d11.h, so we use a static here. It should only
// be used within InitializeD3D11.
decltype(D3D11CreateDevice)* sD3D11CreateDeviceFn = nullptr;

  HMODULE module=LoadLibraryW(L"d3d11.dll");
  if (!module) {
    return false;
  }

  sD3D11CreateDeviceFn =
      (decltype(D3D11CreateDevice)*)GetProcAddress(module, "D3D11CreateDevice");
  if (!sD3D11CreateDeviceFn) {
    // We should just be on Windows Vista or XP in this case.
    return false;
  }

  D3D_FEATURE_LEVEL feature_level;
  // Default feature levels contain D3D 9.1 through D3D 11.0.
  _com_error error = sD3D11CreateDeviceFn(
      adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
      D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_SINGLETHREADED,
      nullptr, 0, D3D11_SDK_VERSION, d3d_device_.GetAddressOf(), &feature_level,
      context_.GetAddressOf());
  if (error.Error() != S_OK || !d3d_device_ || !context_) {
    LOG(LS_WARNING) << "D3D11CreateDeivce returns error "
                    << error.ErrorMessage() << " with code " << error.Error();
    return false;
  }

  if (feature_level < D3D_FEATURE_LEVEL_11_0) {
    LOG(LS_WARNING) << "D3D11CreateDevice returns an instance without DirectX "
                       "11 support, level " << feature_level
                    << ". Following initialization may fail.";
    // D3D_FEATURE_LEVEL_11_0 is not officially documented on MSDN to be a
    // requirement of Dxgi duplicator APIs.
  }

  error = d3d_device_.As(&dxgi_device_);
  if (error.Error() != S_OK || !dxgi_device_) {
    LOG(LS_WARNING) << "ID3D11Device is not an implementation of IDXGIDevice, "
                       "this usually means the system does not support DirectX "
                       "11. Error "
                    << error.ErrorMessage() << " with code " << error.Error();
    return false;
  }

  return true;
}

// static
std::vector<D3dDevice> D3dDevice::EnumDevices() {
typedef HRESULT (APIENTRY *PFN_CreateDXGIFactory1)(REFIID riid, void **ppFactory);
static PFN_CreateDXGIFactory1 fpCreateDXGIFactory1;
    HMODULE dxgi_module = LoadLibraryW(L"dxgi.dll");
    fpCreateDXGIFactory1 = dxgi_module == NULL ? NULL :
        (PFN_CreateDXGIFactory1)GetProcAddress(dxgi_module, "CreateDXGIFactory1");
  if (!fpCreateDXGIFactory1) {
    return std::vector<D3dDevice>();
  }

  ComPtr<IDXGIFactory1> factory;
  _com_error error = fpCreateDXGIFactory1(__uuidof(IDXGIFactory1),
      reinterpret_cast<void**>(factory.GetAddressOf()));
  if (error.Error() != S_OK || !factory) {
    return std::vector<D3dDevice>();
  }

  std::vector<D3dDevice> result;
  for (int i = 0;; i++) {
    ComPtr<IDXGIAdapter> adapter;
    error = factory->EnumAdapters(i, adapter.GetAddressOf());
    if (error.Error() == S_OK) {
      D3dDevice device;
      if (!device.Initialize(adapter)) {
        return std::vector<D3dDevice>();
      }
      result.push_back(std::move(device));
    } else if (error.Error() == DXGI_ERROR_NOT_FOUND) {
      break;
    } else {
      LOG(LS_WARNING) << "IDXGIFactory1::EnumAdapters returns an unexpected "
                         "error "
                      << error.ErrorMessage() << " with code " << error.Error();
      return std::vector<D3dDevice>();
    }
  }
  return result;
}

}  // namespace webrtc
