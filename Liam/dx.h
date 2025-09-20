#ifndef DX_H
#define DX_H

#include "helper.h"
#include "win32.h"

class DX {
 public:
  DX(UINT width, UINT height, std::wstring title);
  virtual ~DX();

  virtual void OnInit() = 0;
  virtual void OnUpdate() = 0;
  virtual void OnRender() = 0;
  virtual void OnDestroy() = 0;
  virtual void OnMouseDown(WPARAM, int, int) {}
  virtual void OnMouseUp(WPARAM, int, int) {}
  virtual void OnMouseMove(WPARAM, int, int) {}
  virtual void OnKeyDown(UINT8) {}
  virtual void OnKeyUp(UINT8) {}
  virtual void OnMouseWheel(float delta) {}

  UINT width() const { return width_; }
  UINT height() const { return height_; }
  const WCHAR *title() const { return title_.c_str(); }
  void ParseCommandLineArgs(_In_reads_(argc) WCHAR *argv[], int argc);

 protected:
  std::wstring GetAssetFullPath(LPCWSTR asset_name);
  void GetHardwareAdapter(_In_ IDXGIFactory1 *factory,
                          _Outptr_result_maybenull_ IDXGIAdapter1 **adapter,
                          bool request_high_performance_adapter = false);
  void SetCustomWindowText(double wave_length, int para_type, UINT fps);

  UINT width_;
  UINT height_;
  float aspect_ratio_;
  bool use_warp_device_;

 private:
  std::wstring assets_path_;
  std::wstring title_;
};

#endif  // !DX_H
