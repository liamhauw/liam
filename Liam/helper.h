#ifndef HELPER_H
#define HELPER_H
#include "pch.h"
//

inline std::string HrToString(HRESULT hr) {
  char s_str[64] = {};
  sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<UINT>(hr));
  return std::string(s_str);
}

class HrException : public std::runtime_error {
 public:
  HrException(HRESULT hr) : std::runtime_error(HrToString(hr)), hr_(hr) {}
  HRESULT hr() const { return hr_; }

 private:
  const HRESULT hr_;
};

inline void ThrowIfFailed(HRESULT hr) {
  if (FAILED(hr)) {
    throw HrException(hr);
  }
}

// Assign a name to the object to aid with debugging.
#if defined(_DEBUG)
inline void SetName(ID3D12Object *d3d12object, LPCWSTR name) {
  d3d12object->SetName(name);
}
inline void SetNameIndexed(ID3D12Object *d3d12object, LPCWSTR name,
                           UINT index) {
  WCHAR full_name[50];
  if (swprintf_s(full_name, L"%s[%u]", name, index) > 0) {
    d3d12object->SetName(full_name);
  }
}
#else
inline void SetName(ID3D12Object *, LPCWSTR) {}
inline void SetNameIndexed(ID3D12Object *, LPCWSTR, UINT) {}
#endif

// Naming helper for ComPtr<T>.
// Assigns the name of the variable as the name of the object. The indexed
// variant will include the index in the name of the object.
#define NAME_D3D12_OBJECT(x) SetName((x).Get(), L#x)
#define NAME_D3D12_OBJECT_INDEXED(x, n) SetNameIndexed((x)[n].Get(), L#x, n)

inline Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
    const std::wstring &file_name, const D3D_SHADER_MACRO *defines,
    const std::string &entry_point, const std::string &target) {
#if defined(_DEBUG)
  // Enable better shader debugging with the graphics debugging tools
  UINT compile_flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
  UINT compile_flags = 0;
#endif
  Microsoft::WRL::ComPtr<ID3DBlob> error;
  Microsoft::WRL::ComPtr<ID3DBlob> shader;

  HRESULT hr = D3DCompileFromFile(
      file_name.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
      entry_point.c_str(), target.c_str(), compile_flags, 0, &shader, &error);
  if (error != nullptr) {
    OutputDebugStringA(reinterpret_cast<char *>(error->GetBufferPointer()));
  }
  ThrowIfFailed(hr);
  return shader;
}

inline UINT CalcConstantBufferSize(UINT byte_size) {
  return (byte_size + (D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1)) &
         ~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1);
}

inline void GetAssetsPath(_Out_writes_(path_size) WCHAR *path, UINT path_size) {
  if (path == nullptr) {
    throw std::exception();
  }

  DWORD size = GetModuleFileName(nullptr, path, path_size);
  if (size == 0 || size == path_size) {
    // Method failed or path was truncated.
    throw std::exception();
  }

  WCHAR *last_slash = wcsrchr(path, L'\\');
  if (last_slash) {
    *(last_slash + 1) = L'\0';
  }
}

//UINT Align(UINT uLocation, UINT uAlign) {
//  return ((uLocation + (uAlign - 1)) & ~(uAlign - 1));
//}

#endif  // ! HELPER_H
