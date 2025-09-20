#include "pch.h"
//
#include "dx.h"

using Microsoft::WRL::ComPtr;

DX::DX(UINT width, UINT height, std::wstring title)
    : width_(width), height_(height), use_warp_device_(false), title_(title) {
  aspect_ratio_ = static_cast<float>(width_) / static_cast<float>(height_);

  WCHAR assets_path[512];
  GetAssetsPath(assets_path, _countof(assets_path));
  assets_path_ = assets_path;
}

DX::~DX() {}

// Helper function for parsing any supplied command line args.
_Use_decl_annotations_ void DX::ParseCommandLineArgs(WCHAR *argv[], int argc) {
  for (int i = 1; i < argc; ++i) {
    if (_wcsnicmp(argv[i], L"-warp", wcslen(argv[i])) == 0 ||
        _wcsnicmp(argv[i], L"/warp", wcslen(argv[i])) == 0) {
      use_warp_device_ = true;
      title_ += title_ + L" (WARP)";
    }
  }
}

// Helper function for resloving the full path of asset.
std::wstring DX::GetAssetFullPath(LPCWSTR asset_name) {
  return assets_path_ + asset_name;
}

// Helper function for acquiring the first available hardware adapter that
// supports Direct3D 12. If no such adapter can be found, *ppAdapter will be set
// to nullptr.
_Use_decl_annotations_ void DX::GetHardwareAdapter(
    IDXGIFactory1 *factory, IDXGIAdapter1 **adapter,
    bool request_high_performance_adapter) {
  *adapter = nullptr;

  ComPtr<IDXGIAdapter1> adapter1;

  ComPtr<IDXGIFactory6> factory6;
  if (SUCCEEDED(factory->QueryInterface(IID_PPV_ARGS(&factory6)))) {
    for (UINT adapter_index = 0;
         DXGI_ERROR_NOT_FOUND != factory6->EnumAdapterByGpuPreference(
                                     adapter_index,
                                     request_high_performance_adapter == true
                                         ? DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE
                                         : DXGI_GPU_PREFERENCE_UNSPECIFIED,
                                     IID_PPV_ARGS(&adapter1));
         ++adapter_index) {
      DXGI_ADAPTER_DESC1 desc;
      adapter1->GetDesc1(&desc);

      if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
        // Don't select the Basic Render Driver adapter. If you want a software
        // adapter, pass in "/warp" on the command line.
        continue;
      }

      // Check to see whether the adapter supports Direct3D 12, but don't create
      // the actual device yet.
      if (SUCCEEDED(D3D12CreateDevice(adapter1.Get(), D3D_FEATURE_LEVEL_11_0,
                                      _uuidof(ID3D12Device), nullptr))) {
        break;
      }
    }
  } else {
    for (UINT adapter_index = 0;
         DXGI_ERROR_NOT_FOUND !=
         factory->EnumAdapters1(adapter_index, &adapter1);
         ++adapter_index) {
      DXGI_ADAPTER_DESC1 desc;
      adapter1->GetDesc1(&desc);

      if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
        // Don't select the Basic Render Driver adapter.
        // If you want a software adapter, pass in "/warp" on the command line.
        continue;
      }

      // Check to see whether the adapter supports Direct3D 12, but don't create
      // the actual device yet.
      if (SUCCEEDED(D3D12CreateDevice(adapter1.Get(), D3D_FEATURE_LEVEL_11_0,
                                      _uuidof(ID3D12Device), nullptr))) {
        break;
      }
    }
  }

  *adapter = adapter1.Detach();
}

// Helper function for setting the window's title text.
void DX::SetCustomWindowText(double wave_length, int para_type, UINT fps) {
  std::wstring window_text = L"wavelength: " + std::to_wstring(wave_length) + L"\tparameter type: " + std::to_wstring(para_type) + L"\tfps: " + std::to_wstring(fps);
  SetWindowText(Win32::h_wnd(), window_text.c_str());
}