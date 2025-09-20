
#include "pch.h"

//
#include <cmath>
#include <fstream>
#include <iostream>
#include <random>
#include <vector>

#include "smoke.h"

using namespace std;
using namespace DirectX;

Smoke::Smoke(UINT width, UINT height, std::wstring name)
    : DX(width, height, name),
      viewport_{0.0f, 0.0f, static_cast<float>(width),
                static_cast<float>(height)},
      rect_{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)} {}

// initialize
void Smoke::OnInit() {
  CreateBase();
  CreateConstant();
  CreateParticle();
  CreateGrid();
  CreateState();
  CreateScene();
  ExcuteCreateCommand();
  InitParticle();
  InitGrid();
}

void Smoke::CreateBase() {
  // create debug and factory
  ComPtr<IDXGIFactory4> factory;
  {
    UINT dxgi_factory_flags = 0;
#if defined(_DEBUG)
    {
      ComPtr<ID3D12Debug> debug;
      if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
        debug->EnableDebugLayer();
        dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
      }
    }
#endif  // defined(_DEBUG)

    ThrowIfFailed(
        CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&factory)));
  }

  // create the device
  {
    if (use_warp_device_) {
      ComPtr<IDXGIAdapter> warp_adapter;
      ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp_adapter)));
      ThrowIfFailed(D3D12CreateDevice(
          warp_adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_)));
    } else {
      ComPtr<IDXGIAdapter1> hardware_adapter;
      GetHardwareAdapter(factory.Get(), &hardware_adapter);
      ThrowIfFailed(D3D12CreateDevice(hardware_adapter.Get(),
                                      D3D_FEATURE_LEVEL_11_0,
                                      IID_PPV_ARGS(&device_)));
    }
    NAME_D3D12_OBJECT(device_);
  }

  // create the command queue
  {
    D3D12_COMMAND_QUEUE_DESC queue_desc{};
    queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ThrowIfFailed(
        device_->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&queue_)));
    NAME_D3D12_OBJECT(queue_);
  }

  // create the swap chain
  {
    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc{};
    swap_chain_desc.Width = width_;
    swap_chain_desc.Height = height_;
    swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.BufferCount = kFrameCount;
    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    ComPtr<IDXGISwapChain1> chain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(queue_.Get(), Win32::h_wnd(),
                                                  &swap_chain_desc, nullptr,
                                                  nullptr, &chain));
    ThrowIfFailed(
        factory->MakeWindowAssociation(Win32::h_wnd(), DXGI_MWA_NO_ALT_ENTER));
    ThrowIfFailed(chain.As(&chain_));
    frame_index_ = chain_->GetCurrentBackBufferIndex();
  }

  // create descriptor heaps
  {
    D3D12_DESCRIPTOR_HEAP_DESC dsv_heap_desc{};
    dsv_heap_desc.NumDescriptors = 1;
    dsv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(device_->CreateDescriptorHeap(&dsv_heap_desc,
                                                IID_PPV_ARGS(&dsv_heap_)));
    NAME_D3D12_OBJECT(dsv_heap_);
    D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc{};
    rtv_heap_desc.NumDescriptors = kFrameCount;
    rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(device_->CreateDescriptorHeap(&rtv_heap_desc,
                                                IID_PPV_ARGS(&rtv_heap_)));
    NAME_D3D12_OBJECT(rtv_heap_);
    D3D12_DESCRIPTOR_HEAP_DESC cbv_srv_uav_heap_desc{};
    cbv_srv_uav_heap_desc.NumDescriptors = kCbvSrvUavCount;
    cbv_srv_uav_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbv_srv_uav_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(device_->CreateDescriptorHeap(
        &cbv_srv_uav_heap_desc, IID_PPV_ARGS(&cbv_srv_uav_heap_)));
    NAME_D3D12_OBJECT(cbv_srv_uav_heap_);

    dsv_size_ = device_->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    rtv_size_ = device_->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    cbv_srv_uav_size_ = device_->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  }

  // create the depth stencil buffer and descriptor
  {
    D3D12_RESOURCE_DESC depth_stencil_desc{};
    depth_stencil_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depth_stencil_desc.Alignment = 0;
    depth_stencil_desc.Width = width_;
    depth_stencil_desc.Height = height_;
    depth_stencil_desc.DepthOrArraySize = 1;
    depth_stencil_desc.MipLevels = 1;
    depth_stencil_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depth_stencil_desc.SampleDesc.Count = 1;
    depth_stencil_desc.SampleDesc.Quality = 0;
    depth_stencil_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depth_stencil_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clear_value{};
    clear_value.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    clear_value.DepthStencil.Depth = 1.0f;
    clear_value.DepthStencil.Stencil = 0;

    ThrowIfFailed(device_->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES{D3D12_HEAP_TYPE_DEFAULT}, D3D12_HEAP_FLAG_NONE,
        &depth_stencil_desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear_value,
        IID_PPV_ARGS(&depth_stencil_)));
    NAME_D3D12_OBJECT(depth_stencil_);

    device_->CreateDepthStencilView(
        depth_stencil_.Get(), nullptr,
        dsv_heap_->GetCPUDescriptorHandleForHeapStart());
  }

  // create render targets and allocators
  {
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle{
        rtv_heap_->GetCPUDescriptorHandleForHeapStart()};
    for (UINT n = 0; n < kFrameCount; ++n) {
      ThrowIfFailed(chain_->GetBuffer(n, IID_PPV_ARGS(&render_targets_[n])));
      device_->CreateRenderTargetView(render_targets_[n].Get(), nullptr,
                                      rtv_handle);
      NAME_D3D12_OBJECT_INDEXED(render_targets_, n);
      rtv_handle.Offset(1, rtv_size_);

      ThrowIfFailed(device_->CreateCommandAllocator(
          D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocators_[n])));
      NAME_D3D12_OBJECT_INDEXED(allocators_, n);
    }
  }

  // create the command list.
  {
    ThrowIfFailed(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                             allocators_[frame_index_].Get(),
                                             nullptr, IID_PPV_ARGS(&list_)));
    NAME_D3D12_OBJECT(list_);
  }

  // create synchronization objects
  {
    ThrowIfFailed(
        device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)));
    NAME_D3D12_OBJECT(fence_);

    fence_event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (fence_event_ == nullptr) {
      ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }
  }
}

void Smoke::CreateConstant() {
  // solver constant
  const UINT solver_constant_buffer_size =
      CalcConstantBufferSize(sizeof(SolverConstant));

  ThrowIfFailed(device_->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(solver_constant_buffer_size),
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&solver_cb_)));

  auto solver_cpu_cbv_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
      cbv_srv_uav_heap_->GetCPUDescriptorHandleForHeapStart(), kSolverCbvIndex,
      cbv_srv_uav_size_);

  D3D12_CONSTANT_BUFFER_VIEW_DESC solver_cbv_desc{};
  solver_cbv_desc.BufferLocation = solver_cb_->GetGPUVirtualAddress();
  solver_cbv_desc.SizeInBytes = solver_constant_buffer_size;

  device_->CreateConstantBufferView(&solver_cbv_desc, solver_cpu_cbv_handle);

  CD3DX12_RANGE solver_read_range(0, 0);
  ThrowIfFailed(solver_cb_->Map(
      0, &solver_read_range, reinterpret_cast<void**>(&solver_constant_data_)));

  for (UINT i = 0; i < kBombCount; ++i) {
    const BombPara& para{kBombPara[i]};
    BombInfo& info{solver_constant_.info[i]};

    auto rotation_matrix{XMMatrixRotationX(para.rotation.x) *
                         XMMatrixRotationY(para.rotation.y) *
                         XMMatrixRotationZ(para.rotation.z)};

    auto scale_matrix{
        XMMatrixScaling(para.scale.x, para.scale.y, para.scale.z)};

    auto translation_matrix{XMMatrixTranslation(
        para.tanslation.x, para.tanslation.y, para.tanslation.z)};

    auto model{scale_matrix * rotation_matrix * translation_matrix };

    XMStoreFloat4x4(&info.model, XMMatrixTranspose(model));
    info.mass_vel = para.mass_vel;
    info.length = para.length;
    info.radius = para.radius;
  }

  memcpy(solver_constant_data_, &solver_constant_, sizeof(solver_constant_));

  // scene constant
  const UINT scene_constant_buffer_size =
      CalcConstantBufferSize(sizeof(SceneConstant));

  ThrowIfFailed(device_->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(scene_constant_buffer_size),
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&scene_cb_)));

  auto scene_cpu_cbv_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
      cbv_srv_uav_heap_->GetCPUDescriptorHandleForHeapStart(), kSceneCbvIndex,
      cbv_srv_uav_size_);

  D3D12_CONSTANT_BUFFER_VIEW_DESC scene_cbv_desc{};
  scene_cbv_desc.BufferLocation = scene_cb_->GetGPUVirtualAddress();
  scene_cbv_desc.SizeInBytes = scene_constant_buffer_size;

  device_->CreateConstantBufferView(&scene_cbv_desc, scene_cpu_cbv_handle);

  CD3DX12_RANGE scene_read_range(0, 0);
  ThrowIfFailed(scene_cb_->Map(
      0, &scene_read_range, reinterpret_cast<void**>(&scene_constant_data_)));

  memcpy(scene_constant_data_, &scene_constant_, sizeof(scene_constant_));
}

void Smoke::CreateParticle() {
  vector<Particle> data;
  data.resize(kParticleCount);
  const UINT buffer_size{kParticleCount * sizeof(Particle)};

  ThrowIfFailed(device_->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(
          buffer_size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&particle_)));
  NAME_D3D12_OBJECT(particle_);

  D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
  srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srv_desc.Format = DXGI_FORMAT_UNKNOWN;
  srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
  srv_desc.Buffer.FirstElement = 0;
  srv_desc.Buffer.NumElements = kParticleCount;
  srv_desc.Buffer.StructureByteStride = sizeof(Particle);
  srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

  CD3DX12_CPU_DESCRIPTOR_HANDLE cpu_srv_handle{
      cbv_srv_uav_heap_->GetCPUDescriptorHandleForHeapStart(),
      kParticlesSrvIndex, cbv_srv_uav_size_};
  device_->CreateShaderResourceView(particle_.Get(), &srv_desc, cpu_srv_handle);

  D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
  uav_desc.Format = DXGI_FORMAT_UNKNOWN;
  uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
  uav_desc.Buffer.FirstElement = 0;
  uav_desc.Buffer.NumElements = kParticleCount;
  uav_desc.Buffer.StructureByteStride = sizeof(Particle);
  uav_desc.Buffer.CounterOffsetInBytes = 0;
  uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

  CD3DX12_CPU_DESCRIPTOR_HANDLE cpu_uav_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
      cbv_srv_uav_heap_->GetCPUDescriptorHandleForHeapStart(),
      kParticlesUavIndex, cbv_srv_uav_size_);
  device_->CreateUnorderedAccessView(particle_.Get(), nullptr, &uav_desc,
                                     cpu_uav_handle);
}

void Smoke::CreateGrid() {
  // u0, v0, w0
  CreateGridFLoat(u0_, kCellCountX + 1, kCellCountY, kCellCountZ, kU0SrvIndex,
                  kU0UavIndex);
  CreateGridFLoat(v0_, kCellCountX, kCellCountY + 1, kCellCountZ, kV0SrvIndex,
                  kV0UavIndex);
  CreateGridFLoat(w0_, kCellCountX, kCellCountY, kCellCountZ + 1, kW0SrvIndex,
                  kW0UavIndex);
  NAME_D3D12_OBJECT(u0_);
  NAME_D3D12_OBJECT(v0_);
  NAME_D3D12_OBJECT(w0_);

  // density0, temp0
  CreateGridFLoat(density0_, kCellCountX, kCellCountY, kCellCountZ,
                  kDensity0SrvIndex, kDensity0UavIndex);
  CreateGridFLoat(temp0_, kCellCountX, kCellCountY, kCellCountZ, kTemp0SrvIndex,
                  kTemp0UavIndex);
  NAME_D3D12_OBJECT(density0_);
  NAME_D3D12_OBJECT(temp0_);

  // u1, v1, w1
  CreateGridFLoat(u1_, kCellCountX + 1, kCellCountY, kCellCountZ, kU1SrvIndex,
                  kU1UavIndex);
  CreateGridFLoat(v1_, kCellCountX, kCellCountY + 1, kCellCountZ, kV1SrvIndex,
                  kV1UavIndex);
  CreateGridFLoat(w1_, kCellCountX, kCellCountY, kCellCountZ + 1, kW1SrvIndex,
                  kW1UavIndex);
  NAME_D3D12_OBJECT(u1_);
  NAME_D3D12_OBJECT(v1_);
  NAME_D3D12_OBJECT(w1_);

  // density1, temp1
  CreateGridFLoat(density1_, kCellCountX, kCellCountY, kCellCountZ,
                  kDensity1SrvIndex, kDensity1UavIndex);
  CreateGridFLoat(temp1_, kCellCountX, kCellCountY, kCellCountZ, kTemp1SrvIndex,
                  kTemp1UavIndex);
  NAME_D3D12_OBJECT(density1_);
  NAME_D3D12_OBJECT(temp1_);

  // average velocity
  CreateGridFLoat(avg_u_, kCellCountX, kCellCountY, kCellCountZ, kAvgUSrvIndex,
                  kAvgUUavIndex);
  CreateGridFLoat(avg_v_, kCellCountX, kCellCountY, kCellCountZ, kAvgVSrvIndex,
                  kAvgVUavIndex);
  CreateGridFLoat(avg_w_, kCellCountX, kCellCountY, kCellCountZ, kAvgWSrvIndex,
                  kAvgWUavIndex);
  NAME_D3D12_OBJECT(avg_u_);
  NAME_D3D12_OBJECT(avg_v_);
  NAME_D3D12_OBJECT(avg_w_);

  // vorticity
  CreateGridFLoat(vorciticy_x_, kCellCountX, kCellCountY, kCellCountZ,
                  kVorticityXSrvIndex, kVorticityXUavIndex);
  CreateGridFLoat(vorciticy_y_, kCellCountX, kCellCountY, kCellCountZ,
                  kVorticityYSrvIndex, kVorticityYUavIndex);
  CreateGridFLoat(vorciticy_z_, kCellCountX, kCellCountY, kCellCountZ,
                  kVorticityZSrvIndex, kVorticityZUavIndex);
  NAME_D3D12_OBJECT(vorciticy_x_);
  NAME_D3D12_OBJECT(vorciticy_y_);
  NAME_D3D12_OBJECT(vorciticy_z_);

  // force
  CreateGridFLoat(force_x_, kCellCountX, kCellCountY, kCellCountZ,
                  kForceXSrvIndex, kForceXUavIndex);
  CreateGridFLoat(force_y_, kCellCountX, kCellCountY, kCellCountZ,
                  kForceYSrvIndex, kForceYUavIndex);
  CreateGridFLoat(force_z_, kCellCountX, kCellCountY, kCellCountZ,
                  kForceZSrvIndex, kForceZUavIndex);
  NAME_D3D12_OBJECT(force_x_);
  NAME_D3D12_OBJECT(force_y_);
  NAME_D3D12_OBJECT(force_z_);

  // b, A
  CreateGridFLoat(b_, kCellCountX, kCellCountY, kCellCountZ, kbSrvIndex,
                  kbUavIndex);
  CreateGridFloat4(A_, kCellCountX, kCellCountY, kCellCountZ, kASrvIndex,
                   kAUavIndex);
  NAME_D3D12_OBJECT(b_);
  NAME_D3D12_OBJECT(A_);

  // p, residual
  CreateGridFLoat(p_, kCellCountX, kCellCountY, kCellCountZ, kpSrvIndex,
                  kpUavIndex);
  CreateGridFLoat(residual_, kCellCountX, kCellCountY, kCellCountZ,
                  kResiduaSrvIndex, kResiduaUavIndex);
  NAME_D3D12_OBJECT(p_);
  NAME_D3D12_OBJECT(residual_);

  // input radiance
  CreateGridFLoat(input_radiance_, kCellCountX, kCellCountY, kCellCountZ,
                  kInputRadianceSrvIndex, kInputRadianceUavIndex);
  NAME_D3D12_OBJECT(input_radiance_);
}

void Smoke::CreateState() {
  CreateSolverState();
  CreateSceneState();
}

void Smoke::CreateScene() {
  // blackbody
  {
    constexpr float r{7.0f};
    constexpr float dis{100.0f};

    Vertex vertices[8]{XMFLOAT3{-r, -r, dis - r}, XMFLOAT3{-r, +r, dis - r},
                       XMFLOAT3{+r, +r, dis - r}, XMFLOAT3{+r, -r, dis - r},
                       XMFLOAT3{-r, -r, dis + r}, XMFLOAT3{-r, +r, dis + r},
                       XMFLOAT3{+r, +r, dis + r}, XMFLOAT3{+r, -r, dis + r}};

    uint16_t indices[] = {// front face
                          0, 1, 2, 0, 2, 3,
                          // back face
                          4, 6, 5, 4, 7, 6,
                          // left face
                          4, 5, 1, 4, 1, 0,
                          // right face
                          3, 2, 6, 3, 6, 7,
                          // top face
                          1, 5, 6, 1, 6, 2,
                          // bottom face
                          4, 0, 3, 4, 3, 7};

    const UINT vertex_buffer_size = sizeof(vertices);
    const UINT index_buffer_size = sizeof(indices);

    ThrowIfFailed(device_->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(vertex_buffer_size),
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&blackbody_vb_)));
    NAME_D3D12_OBJECT(blackbody_vb_);

    ThrowIfFailed(device_->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(vertex_buffer_size),
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&blackbody_vb_upload_)));

    D3D12_SUBRESOURCE_DATA vertex_data = {};
    vertex_data.pData = reinterpret_cast<UINT8*>(vertices);
    vertex_data.RowPitch = vertex_buffer_size;
    vertex_data.SlicePitch = vertex_data.RowPitch;

    UpdateSubresources<1>(list_.Get(), blackbody_vb_.Get(),
                          blackbody_vb_upload_.Get(), 0, 0, 1, &vertex_data);

    list_->ResourceBarrier(
        1, &CD3DX12_RESOURCE_BARRIER::Transition(
               blackbody_vb_.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
               D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

    ThrowIfFailed(device_->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(index_buffer_size),
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&blackbody_ib_)));
    NAME_D3D12_OBJECT(blackbody_ib_);

    ThrowIfFailed(device_->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(index_buffer_size),
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&blackbody_ib_upload_)));

    D3D12_SUBRESOURCE_DATA index_data = {};
    index_data.pData = reinterpret_cast<UINT8*>(indices);
    index_data.RowPitch = index_buffer_size;
    index_data.SlicePitch = index_data.RowPitch;

    UpdateSubresources<1>(list_.Get(), blackbody_ib_.Get(),
                          blackbody_ib_upload_.Get(), 0, 0, 1, &index_data);
    list_->ResourceBarrier(
        1, &CD3DX12_RESOURCE_BARRIER::Transition(
               blackbody_ib_.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
               D3D12_RESOURCE_STATE_INDEX_BUFFER));

    // vertex buffer view
    blackbody_VBV_.BufferLocation = blackbody_vb_->GetGPUVirtualAddress();
    blackbody_VBV_.SizeInBytes = vertex_buffer_size;
    blackbody_VBV_.StrideInBytes = sizeof(Vertex);

    // index buffer view
    blackbody_IBV_.BufferLocation = blackbody_ib_->GetGPUVirtualAddress();
    blackbody_IBV_.Format = DXGI_FORMAT_R16_UINT;
    blackbody_IBV_.SizeInBytes = index_buffer_size;
  }

  // wireframe
  {
    // vertex and index
    constexpr float x = kCellSize * kCellCountX * 0.5f;
    constexpr float y = kCellSize * kCellCountY * 0.5f;
    constexpr float z = kCellSize * kCellCountZ * 0.5f;

    Vertex vertices[8]{{XMFLOAT3{-x, -y, -z}}, {XMFLOAT3{-x, +y, -z}},
                       {XMFLOAT3{+x, +y, -z}}, {XMFLOAT3{+x, -y, -z}},
                       {XMFLOAT3{-x, -y, +z}}, {XMFLOAT3{-x, +y, +z}},
                       {XMFLOAT3{+x, +y, +z}}, {XMFLOAT3{+x, -y, +z}}};

    uint16_t indices[12][2]{{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                            {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};

    const UINT vertex_buffer_size = sizeof(vertices);
    const UINT index_buffer_size = sizeof(indices);

    ThrowIfFailed(device_->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(vertex_buffer_size),
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&wireframe_vb_)));
    NAME_D3D12_OBJECT(wireframe_vb_);

    ThrowIfFailed(device_->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(vertex_buffer_size),
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&wireframe_vb_upload_)));

    D3D12_SUBRESOURCE_DATA vertex_data = {};
    vertex_data.pData = reinterpret_cast<UINT8*>(vertices);
    vertex_data.RowPitch = vertex_buffer_size;
    vertex_data.SlicePitch = vertex_data.RowPitch;

    UpdateSubresources<1>(list_.Get(), wireframe_vb_.Get(),
                          wireframe_vb_upload_.Get(), 0, 0, 1, &vertex_data);

    list_->ResourceBarrier(
        1, &CD3DX12_RESOURCE_BARRIER::Transition(
               wireframe_vb_.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
               D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

    ThrowIfFailed(device_->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(index_buffer_size),
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&wireframe_ib_)));
    NAME_D3D12_OBJECT(wireframe_ib_);

    ThrowIfFailed(device_->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(index_buffer_size),
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&wireframe_ib_upload_)));

    D3D12_SUBRESOURCE_DATA index_data{};
    index_data.pData = reinterpret_cast<UINT8*>(indices);
    index_data.RowPitch = index_buffer_size;
    index_data.SlicePitch = index_data.RowPitch;

    UpdateSubresources<1>(list_.Get(), wireframe_ib_.Get(),
                          wireframe_ib_upload_.Get(), 0, 0, 1, &index_data);
    list_->ResourceBarrier(
        1, &CD3DX12_RESOURCE_BARRIER::Transition(
               wireframe_ib_.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
               D3D12_RESOURCE_STATE_INDEX_BUFFER));

    // vertex buffer view
    wireframe_VBV_.BufferLocation = wireframe_vb_->GetGPUVirtualAddress();
    wireframe_VBV_.SizeInBytes = vertex_buffer_size;
    wireframe_VBV_.StrideInBytes = sizeof(Vertex);

    // index buffer view
    wireframe_IBV_.BufferLocation = wireframe_ib_->GetGPUVirtualAddress();
    wireframe_IBV_.Format = DXGI_FORMAT_R16_UINT;
    wireframe_IBV_.SizeInBytes = index_buffer_size;
  }

  // particle
  {
    vector<ParticleVertex> vertices;
    vertices.resize(kParticleCount);
    for (int i = 0; i < kParticleCount; ++i) {
      vertices[i].color = XMFLOAT4(1.0f, 1.0f, 0.2f, 1.0f);
    }
    const UINT vertex_buffer_size = kParticleCount * sizeof(ParticleVertex);

    ThrowIfFailed(device_->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(vertex_buffer_size),
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&particle_vb_)));
    NAME_D3D12_OBJECT(particle_vb_);

    ThrowIfFailed(device_->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(vertex_buffer_size),
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&particle_vb_upload_)));

    D3D12_SUBRESOURCE_DATA vertex_data = {};
    vertex_data.pData = reinterpret_cast<UINT8*>(&vertices[0]);
    vertex_data.RowPitch = vertex_buffer_size;
    vertex_data.SlicePitch = vertex_data.RowPitch;

    UpdateSubresources<1>(list_.Get(), particle_vb_.Get(),
                          particle_vb_upload_.Get(), 0, 0, 1, &vertex_data);

    list_->ResourceBarrier(
        1, &CD3DX12_RESOURCE_BARRIER::Transition(
               particle_vb_.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
               D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

    particle_VBV_.BufferLocation = particle_vb_->GetGPUVirtualAddress();
    particle_VBV_.SizeInBytes = vertex_buffer_size;
    particle_VBV_.StrideInBytes = sizeof(ParticleVertex);
  }

  // volumn
  {
    // vertex and index
    constexpr float x = kCellSize * kCellCountX * 0.5f;
    constexpr float y = kCellSize * kCellCountY * 0.5f;
    constexpr float z = kCellSize * kCellCountZ * 0.5f;

    Vertex vertices[] = {
        {{-x, -y, -z}}, {{-x, +y, -z}}, {{+x, +y, -z}}, {{+x, -y, -z}},
        {{-x, -y, +z}}, {{-x, +y, +z}}, {{+x, +y, +z}}, {{+x, -y, +z}},
    };

    uint16_t indices[] = {// front face
                          0, 1, 2, 0, 2, 3,
                          // back face
                          4, 6, 5, 4, 7, 6,
                          // left face
                          4, 5, 1, 4, 1, 0,
                          // right face
                          3, 2, 6, 3, 6, 7,
                          // top face
                          1, 5, 6, 1, 6, 2,
                          // bottom face
                          4, 0, 3, 4, 3, 7};

    const UINT vertex_buffer_size = sizeof(vertices);
    const UINT index_buffer_size = sizeof(indices);

    ThrowIfFailed(device_->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(vertex_buffer_size),
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&volume_vb_)));
    NAME_D3D12_OBJECT(volume_vb_);

    ThrowIfFailed(device_->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(vertex_buffer_size),
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&volume_vb_upload_)));

    D3D12_SUBRESOURCE_DATA vertex_data = {};
    vertex_data.pData = reinterpret_cast<UINT8*>(vertices);
    vertex_data.RowPitch = vertex_buffer_size;
    vertex_data.SlicePitch = vertex_data.RowPitch;

    UpdateSubresources<1>(list_.Get(), volume_vb_.Get(),
                          volume_vb_upload_.Get(), 0, 0, 1, &vertex_data);

    list_->ResourceBarrier(
        1, &CD3DX12_RESOURCE_BARRIER::Transition(
               volume_vb_.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
               D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

    ThrowIfFailed(device_->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(index_buffer_size),
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&volume_ib_)));
    NAME_D3D12_OBJECT(volume_ib_);

    ThrowIfFailed(device_->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(index_buffer_size),
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&volume_ib_upload_)));

    D3D12_SUBRESOURCE_DATA index_data = {};
    index_data.pData = reinterpret_cast<UINT8*>(indices);
    index_data.RowPitch = index_buffer_size;
    index_data.SlicePitch = index_data.RowPitch;

    UpdateSubresources<1>(list_.Get(), volume_ib_.Get(),
                          volume_ib_upload_.Get(), 0, 0, 1, &index_data);
    list_->ResourceBarrier(1,
                           &CD3DX12_RESOURCE_BARRIER::Transition(
                               volume_ib_.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                               D3D12_RESOURCE_STATE_INDEX_BUFFER));
    // vertex buffer view
    volume_VBV_.BufferLocation = volume_vb_->GetGPUVirtualAddress();
    volume_VBV_.SizeInBytes = vertex_buffer_size;
    volume_VBV_.StrideInBytes = sizeof(Vertex);

    // index buffer view
    volume_IBV_.BufferLocation = volume_ib_->GetGPUVirtualAddress();
    volume_IBV_.Format = DXGI_FORMAT_R16_UINT;
    volume_IBV_.SizeInBytes = index_buffer_size;
  }
}

void Smoke::ExcuteCreateCommand() {
  ThrowIfFailed(list_->Close());
  ID3D12CommandList* Lists[]{list_.Get()};
  queue_->ExecuteCommandLists(_countof(Lists), Lists);

  WaitForGpu();
}

void Smoke::InitParticle() {
  ThrowIfFailed(allocators_[frame_index_]->Reset());
  ThrowIfFailed(list_->Reset(allocators_[frame_index_].Get(),
                             init_particle_state_.Get()));
  list_->SetComputeRootSignature(solver_signature_.Get());

  ID3D12DescriptorHeap* heaps[]{cbv_srv_uav_heap_.Get()};
  list_->SetDescriptorHeaps(_countof(heaps), heaps);

  CD3DX12_GPU_DESCRIPTOR_HANDLE solver_cbv_handle{
      cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(), kSolverCbvIndex,
      cbv_srv_uav_size_};

  CD3DX12_GPU_DESCRIPTOR_HANDLE particle_uav_handle{
      cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(),
      kParticlesUavIndex, cbv_srv_uav_size_};

  list_->ResourceBarrier(1,
                         &CD3DX12_RESOURCE_BARRIER::Transition(
                             particle_.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
                             D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

  list_->SetComputeRootDescriptorTable(0, solver_cbv_handle);
  list_->SetComputeRootDescriptorTable(6, particle_uav_handle);

  list_->Dispatch(kParticleCount / 128, 1, 1);

  list_->ResourceBarrier(
      1, &CD3DX12_RESOURCE_BARRIER::Transition(
             particle_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
             D3D12_RESOURCE_STATE_GENERIC_READ));

  ThrowIfFailed(list_->Close());
  ID3D12CommandList* Lists[] = {list_.Get()};
  queue_->ExecuteCommandLists(_countof(Lists), Lists);

  WaitForGpu();
}

void Smoke::InitGrid() {
  ThrowIfFailed(allocators_[frame_index_]->Reset());
  ThrowIfFailed(
      list_->Reset(allocators_[frame_index_].Get(), init_grid_state_.Get()));
  list_->SetComputeRootSignature(solver_signature_.Get());

  ID3D12DescriptorHeap* heaps[]{cbv_srv_uav_heap_.Get()};
  list_->SetDescriptorHeaps(_countof(heaps), heaps);

  CD3DX12_GPU_DESCRIPTOR_HANDLE solver_cbv_handle{
      cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(), kSolverCbvIndex,
      cbv_srv_uav_size_};

  CD3DX12_GPU_DESCRIPTOR_HANDLE temp0_uav_handle{
      cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(), kTemp0UavIndex,
      cbv_srv_uav_size_};

  list_->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
                                temp0_.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
                                D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

  list_->SetComputeRootDescriptorTable(0, solver_cbv_handle);
  list_->SetComputeRootDescriptorTable(6, temp0_uav_handle);

  list_->Dispatch(kCellCountX / 8, kCellCountY / 8, kCellCountZ / 4);

  list_->ResourceBarrier(
      1, &CD3DX12_RESOURCE_BARRIER::Transition(
             temp0_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
             D3D12_RESOURCE_STATE_GENERIC_READ));

  ThrowIfFailed(list_->Close());
  ID3D12CommandList* Lists[] = {list_.Get()};
  queue_->ExecuteCommandLists(_countof(Lists), Lists);

  WaitForGpu();
}

void Smoke::CreateSolverState() {
  // root signature
  CD3DX12_DESCRIPTOR_RANGE1 ranges[7]{};
  ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 2, 0);
  ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0);
  ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 3);
  ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5);
  ranges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 3, 0);
  ranges[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 3);
  ranges[6].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 5);

  CD3DX12_ROOT_PARAMETER1 parameters[7]{};
  parameters[0].InitAsDescriptorTable(1, &ranges[0]);
  parameters[1].InitAsDescriptorTable(1, &ranges[1]);
  parameters[2].InitAsDescriptorTable(1, &ranges[2]);
  parameters[3].InitAsDescriptorTable(1, &ranges[3]);
  parameters[4].InitAsDescriptorTable(1, &ranges[4]);
  parameters[5].InitAsDescriptorTable(1, &ranges[5]);
  parameters[6].InitAsDescriptorTable(1, &ranges[6]);

  D3D12_STATIC_SAMPLER_DESC linear_sampler{};
  linear_sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
  linear_sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  linear_sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  linear_sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  linear_sampler.MipLODBias = 0;
  linear_sampler.MaxAnisotropy = 0;
  linear_sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
  linear_sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
  linear_sampler.MinLOD = 0.0f;
  linear_sampler.MaxLOD = D3D12_FLOAT32_MAX;
  linear_sampler.ShaderRegister = 0;
  linear_sampler.RegisterSpace = 0;
  linear_sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

  D3D12_STATIC_SAMPLER_DESC point_sampler{};
  point_sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
  point_sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  point_sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  point_sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  point_sampler.MipLODBias = 0;
  point_sampler.MaxAnisotropy = 0;
  point_sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
  point_sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
  point_sampler.MinLOD = 0.0f;
  point_sampler.MaxLOD = D3D12_FLOAT32_MAX;
  point_sampler.ShaderRegister = 1;
  point_sampler.RegisterSpace = 0;
  point_sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

  D3D12_STATIC_SAMPLER_DESC samplers[2]{linear_sampler, point_sampler};

  CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc{};
  root_signature_desc.Init_1_1(_countof(parameters), parameters, 2, samplers);

  ComPtr<ID3DBlob> signature;
  ComPtr<ID3DBlob> error;
  ThrowIfFailed(D3DX12SerializeVersionedRootSignature(
      &root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature,
      &error));
  ThrowIfFailed(device_->CreateRootSignature(0, signature->GetBufferPointer(),
                                             signature->GetBufferSize(),
                                             IID_PPV_ARGS(&solver_signature_)));
  NAME_D3D12_OBJECT(solver_signature_);

  // pipeline state

  ComPtr<ID3DBlob> init_particle =
      CompileShader(GetAssetFullPath(L"0_0_init_particle.hlsl").c_str(),
                    nullptr, "CSMain", "cs_5_0");
  D3D12_COMPUTE_PIPELINE_STATE_DESC init_particle_desc{};
  init_particle_desc.pRootSignature = solver_signature_.Get();
  init_particle_desc.CS = CD3DX12_SHADER_BYTECODE(init_particle.Get());
  init_particle_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
  ThrowIfFailed(device_->CreateComputePipelineState(
      &init_particle_desc, IID_PPV_ARGS(&init_particle_state_)));
  NAME_D3D12_OBJECT(init_particle_state_);

  ComPtr<ID3DBlob> init_grid =
      CompileShader(GetAssetFullPath(L"0_1_init_grid.hlsl").c_str(), nullptr,
                    "CSMain", "cs_5_0");
  D3D12_COMPUTE_PIPELINE_STATE_DESC init_grid_desc{};
  init_grid_desc.pRootSignature = solver_signature_.Get();
  init_grid_desc.CS = CD3DX12_SHADER_BYTECODE(init_grid.Get());
  init_grid_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
  ThrowIfFailed(device_->CreateComputePipelineState(
      &init_grid_desc, IID_PPV_ARGS(&init_grid_state_)));
  NAME_D3D12_OBJECT(init_grid_state_);

  ComPtr<ID3DBlob> update_particle =
      CompileShader(GetAssetFullPath(L"1_0_update_particle.hlsl").c_str(),
                    nullptr, "CSMain", "cs_5_0");
  D3D12_COMPUTE_PIPELINE_STATE_DESC update_particle_desc{};
  update_particle_desc.pRootSignature = solver_signature_.Get();
  update_particle_desc.CS = CD3DX12_SHADER_BYTECODE(update_particle.Get());
  update_particle_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
  ThrowIfFailed(device_->CreateComputePipelineState(
      &update_particle_desc, IID_PPV_ARGS(&update_particle_state_)));
  NAME_D3D12_OBJECT(update_particle_state_);

  ComPtr<ID3DBlob> produce_smoke =
      CompileShader(GetAssetFullPath(L"1_1_produce_smoke.hlsl").c_str(),
                    nullptr, "CSMain", "cs_5_0");
  D3D12_COMPUTE_PIPELINE_STATE_DESC produce_smoke_desc{};
  produce_smoke_desc.pRootSignature = solver_signature_.Get();
  produce_smoke_desc.CS = CD3DX12_SHADER_BYTECODE(produce_smoke.Get());
  produce_smoke_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
  ThrowIfFailed(device_->CreateComputePipelineState(
      &produce_smoke_desc, IID_PPV_ARGS(&produce_smoke_state_)));
  NAME_D3D12_OBJECT(produce_smoke_state_);

  ComPtr<ID3DBlob> cal_avg_velocity =
      CompileShader(GetAssetFullPath(L"2_0_cal_avg_velocity.hlsl").c_str(),
                    nullptr, "CSMain", "cs_5_0");
  D3D12_COMPUTE_PIPELINE_STATE_DESC cal_avg_velocity_desc{};
  cal_avg_velocity_desc.pRootSignature = solver_signature_.Get();
  cal_avg_velocity_desc.CS = CD3DX12_SHADER_BYTECODE(cal_avg_velocity.Get());
  cal_avg_velocity_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
  ThrowIfFailed(device_->CreateComputePipelineState(
      &cal_avg_velocity_desc, IID_PPV_ARGS(&cal_avg_velocity_state_)));
  NAME_D3D12_OBJECT(cal_avg_velocity_state_);

  ComPtr<ID3DBlob> cal_vorticity =
      CompileShader(GetAssetFullPath(L"2_1_cal_vorticity.hlsl").c_str(),
                    nullptr, "CSMain", "cs_5_0");
  D3D12_COMPUTE_PIPELINE_STATE_DESC cal_vorticity_desc{};
  cal_vorticity_desc.pRootSignature = solver_signature_.Get();
  cal_vorticity_desc.CS = CD3DX12_SHADER_BYTECODE(cal_vorticity.Get());
  cal_vorticity_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
  ThrowIfFailed(device_->CreateComputePipelineState(
      &cal_vorticity_desc, IID_PPV_ARGS(&cal_vorticity_state_)));
  NAME_D3D12_OBJECT(cal_vorticity_state_);

  ComPtr<ID3DBlob> cal_force =
      CompileShader(GetAssetFullPath(L"2_2_cal_force.hlsl").c_str(), nullptr,
                    "CSMain", "cs_5_0");
  D3D12_COMPUTE_PIPELINE_STATE_DESC cal_force_desc{};
  cal_force_desc.pRootSignature = solver_signature_.Get();
  cal_force_desc.CS = CD3DX12_SHADER_BYTECODE(cal_force.Get());
  cal_force_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
  ThrowIfFailed(device_->CreateComputePipelineState(
      &cal_force_desc, IID_PPV_ARGS(&cal_force_state_)));
  NAME_D3D12_OBJECT(cal_force_state_);

  ComPtr<ID3DBlob> force =
      CompileShader(GetAssetFullPath(L"2_3_force.hlsl").c_str(), kShaderDefines,
                    "CSMain", "cs_5_0");
  D3D12_COMPUTE_PIPELINE_STATE_DESC force_desc{};
  force_desc.pRootSignature = solver_signature_.Get();
  force_desc.CS = CD3DX12_SHADER_BYTECODE(force.Get());
  force_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
  ThrowIfFailed(device_->CreateComputePipelineState(
      &force_desc, IID_PPV_ARGS(&force_state_)));
  NAME_D3D12_OBJECT(force_state_);

  ComPtr<ID3DBlob> cal_pressure_equation =
      CompileShader(GetAssetFullPath(L"3_0_cal_pressure_equation.hlsl").c_str(),
                    nullptr, "CSMain", "cs_5_0");
  D3D12_COMPUTE_PIPELINE_STATE_DESC cal_pressure_equation_desc{};
  cal_pressure_equation_desc.pRootSignature = solver_signature_.Get();
  cal_pressure_equation_desc.CS =
      CD3DX12_SHADER_BYTECODE(cal_pressure_equation.Get());
  cal_pressure_equation_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
  ThrowIfFailed(device_->CreateComputePipelineState(
      &cal_pressure_equation_desc,
      IID_PPV_ARGS(&cal_pressure_equation_state_)));
  NAME_D3D12_OBJECT(cal_pressure_equation_state_);

  ComPtr<ID3DBlob> cal_pressure =
      CompileShader(GetAssetFullPath(L"3_1_cal_pressure.hlsl").c_str(), nullptr,
                    "CSMain", "cs_5_0");
  D3D12_COMPUTE_PIPELINE_STATE_DESC cal_pressure_desc{};
  cal_pressure_desc.pRootSignature = solver_signature_.Get();
  cal_pressure_desc.CS = CD3DX12_SHADER_BYTECODE(cal_pressure.Get());
  cal_pressure_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
  ThrowIfFailed(device_->CreateComputePipelineState(
      &cal_pressure_desc, IID_PPV_ARGS(&cal_pressure_state_)));
  NAME_D3D12_OBJECT(cal_pressure_state_);

  ComPtr<ID3DBlob> project =
      CompileShader(GetAssetFullPath(L"3_2_project.hlsl").c_str(), nullptr,
                    "CSMain", "cs_5_0");
  D3D12_COMPUTE_PIPELINE_STATE_DESC project_desc{};
  project_desc.pRootSignature = solver_signature_.Get();
  project_desc.CS = CD3DX12_SHADER_BYTECODE(project.Get());
  project_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
  ThrowIfFailed(device_->CreateComputePipelineState(
      &project_desc, IID_PPV_ARGS(&project_state_)));
  NAME_D3D12_OBJECT(project_state_);

  ComPtr<ID3DBlob> advect_vector =
      CompileShader(GetAssetFullPath(L"4_0_advect_vector.hlsl").c_str(),
                    kShaderDefines, "CSMain", "cs_5_0");
  D3D12_COMPUTE_PIPELINE_STATE_DESC advect_vector_desc{};
  advect_vector_desc.pRootSignature = solver_signature_.Get();
  advect_vector_desc.CS = CD3DX12_SHADER_BYTECODE(advect_vector.Get());
  advect_vector_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
  ThrowIfFailed(device_->CreateComputePipelineState(
      &advect_vector_desc, IID_PPV_ARGS(&advect_vector_state_)));
  NAME_D3D12_OBJECT(advect_vector_state_);

  ComPtr<ID3DBlob> advect_scalar =
      CompileShader(GetAssetFullPath(L"4_1_advect_scalar.hlsl").c_str(),
                    kShaderDefines, "CSMain", "cs_5_0");
  D3D12_COMPUTE_PIPELINE_STATE_DESC advect_scalar_desc{};
  advect_scalar_desc.pRootSignature = solver_signature_.Get();
  advect_scalar_desc.CS = CD3DX12_SHADER_BYTECODE(advect_scalar.Get());
  advect_scalar_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
  ThrowIfFailed(device_->CreateComputePipelineState(
      &advect_scalar_desc, IID_PPV_ARGS(&advect_scalar_state_)));
  NAME_D3D12_OBJECT(advect_scalar_state_);

  ComPtr<ID3DBlob> cal_input_radiance =
      CompileShader(GetAssetFullPath(L"5_cal_input_radiance.hlsl").c_str(),
                    nullptr, "CSMain", "cs_5_0");
  D3D12_COMPUTE_PIPELINE_STATE_DESC cal_input_radiance_desc{};
  cal_input_radiance_desc.pRootSignature = solver_signature_.Get();
  cal_input_radiance_desc.CS =
      CD3DX12_SHADER_BYTECODE(cal_input_radiance.Get());
  cal_input_radiance_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
  ThrowIfFailed(device_->CreateComputePipelineState(
      &cal_input_radiance_desc, IID_PPV_ARGS(&cal_input_radiance_state_)));
  NAME_D3D12_OBJECT(cal_input_radiance_state_);
}

void Smoke::CreateSceneState() {
  // signature
  CD3DX12_DESCRIPTOR_RANGE1 ranges[4]{};
  ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 2, 0);
  ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);
  ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
  ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 3);

  CD3DX12_ROOT_PARAMETER1 parameters[4]{};
  parameters[0].InitAsDescriptorTable(1, &ranges[0],
                                      D3D12_SHADER_VISIBILITY_ALL);
  parameters[1].InitAsDescriptorTable(1, &ranges[1],
                                      D3D12_SHADER_VISIBILITY_ALL);
  parameters[2].InitAsDescriptorTable(1, &ranges[2],
                                      D3D12_SHADER_VISIBILITY_ALL);
  parameters[3].InitAsDescriptorTable(1, &ranges[3],
                                      D3D12_SHADER_VISIBILITY_PIXEL);

  D3D12_STATIC_SAMPLER_DESC linear_sampler{};
  linear_sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
  linear_sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  linear_sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  linear_sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  linear_sampler.MipLODBias = 0;
  linear_sampler.MaxAnisotropy = 0;
  linear_sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
  linear_sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
  linear_sampler.MinLOD = 0.0f;
  linear_sampler.MaxLOD = D3D12_FLOAT32_MAX;
  linear_sampler.ShaderRegister = 0;
  linear_sampler.RegisterSpace = 0;
  linear_sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

  D3D12_STATIC_SAMPLER_DESC point_sampler{};
  point_sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
  point_sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  point_sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  point_sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  point_sampler.MipLODBias = 0;
  point_sampler.MaxAnisotropy = 0;
  point_sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
  point_sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
  point_sampler.MinLOD = 0.0f;
  point_sampler.MaxLOD = D3D12_FLOAT32_MAX;
  point_sampler.ShaderRegister = 1;
  point_sampler.RegisterSpace = 0;
  point_sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

  D3D12_STATIC_SAMPLER_DESC samplers[2]{linear_sampler, point_sampler};

  D3D12_ROOT_SIGNATURE_FLAGS root_signature_flags =
      D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;
  CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc{};
  root_signature_desc.Init_1_1(_countof(parameters), parameters, 2, samplers,
                               root_signature_flags);

  ComPtr<ID3DBlob> root_signature;
  ComPtr<ID3DBlob> error;

  ThrowIfFailed(D3DX12SerializeVersionedRootSignature(
      &root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1_1, &root_signature,
      &error));

  ThrowIfFailed(device_->CreateRootSignature(
      0, root_signature->GetBufferPointer(), root_signature->GetBufferSize(),
      IID_PPV_ARGS(&scene_signature_)));

  NAME_D3D12_OBJECT(scene_signature_);

  // blackbody pipeline state
  {
    ComPtr<ID3DBlob> vs =
        CompileShader(GetAssetFullPath(L"blackbody.hlsl").c_str(), nullptr,
                      "VSMain", "vs_5_0");
    ComPtr<ID3DBlob> ps =
        CompileShader(GetAssetFullPath(L"blackbody.hlsl").c_str(), nullptr,
                      "PSMain", "ps_5_0");

    D3D12_INPUT_ELEMENT_DESC ie_desc[]{
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

    D3D12_GRAPHICS_PIPELINE_STATE_DESC gps_desc{};
    gps_desc.pRootSignature = scene_signature_.Get();
    gps_desc.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
    gps_desc.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
    gps_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    gps_desc.SampleMask = UINT_MAX;
    gps_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    gps_desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    gps_desc.InputLayout = {ie_desc, _countof(ie_desc)};
    gps_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    gps_desc.NumRenderTargets = 1;
    gps_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    gps_desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    gps_desc.SampleDesc.Count = 1;
    ThrowIfFailed(device_->CreateGraphicsPipelineState(
        &gps_desc, IID_PPV_ARGS(&blackbody_state_)));

    NAME_D3D12_OBJECT(blackbody_state_);
  }

  // wireframe pipeline state
  {
    ComPtr<ID3DBlob> vs =
        CompileShader(GetAssetFullPath(L"wireframe.hlsl").c_str(), nullptr,
                      "VSMain", "vs_5_0");
    ComPtr<ID3DBlob> ps =
        CompileShader(GetAssetFullPath(L"wireframe.hlsl").c_str(), nullptr,
                      "PSMain", "ps_5_0");

    D3D12_INPUT_ELEMENT_DESC ie_desc[]{
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

    CD3DX12_RASTERIZER_DESC rasterizer_desc{D3D12_DEFAULT};
    rasterizer_desc.AntialiasedLineEnable = TRUE;

    D3D12_RENDER_TARGET_BLEND_DESC blend_desc{};
    blend_desc.BlendEnable = true;
    blend_desc.LogicOpEnable = false;
    blend_desc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blend_desc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blend_desc.BlendOp = D3D12_BLEND_OP_ADD;
    blend_desc.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend_desc.DestBlendAlpha = D3D12_BLEND_ZERO;
    blend_desc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend_desc.LogicOp = D3D12_LOGIC_OP_NOOP;
    blend_desc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC gps_desc{};
    gps_desc.pRootSignature = scene_signature_.Get();
    gps_desc.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
    gps_desc.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
    gps_desc.BlendState.RenderTarget[0] = blend_desc;
    gps_desc.SampleMask = UINT_MAX;
    gps_desc.RasterizerState = rasterizer_desc;
    gps_desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    gps_desc.InputLayout = {ie_desc, _countof(ie_desc)};
    gps_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    gps_desc.NumRenderTargets = 1;
    gps_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    gps_desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    gps_desc.SampleDesc.Count = 1;
    ThrowIfFailed(device_->CreateGraphicsPipelineState(
        &gps_desc, IID_PPV_ARGS(&wireframe_state_)));

    NAME_D3D12_OBJECT(wireframe_state_);
  }

  // particle pipeline state
  {
    ComPtr<ID3DBlob> vs =
        CompileShader(GetAssetFullPath(L"particle.hlsl").c_str(), nullptr,
                      "VSMain", "vs_5_0");
    ComPtr<ID3DBlob> gs =
        CompileShader(GetAssetFullPath(L"particle.hlsl").c_str(), nullptr,
                      "GSMain", "gs_5_0");
    ComPtr<ID3DBlob> ps =
        CompileShader(GetAssetFullPath(L"particle.hlsl").c_str(), nullptr,
                      "PSMain", "ps_5_0");
    D3D12_INPUT_ELEMENT_DESC ie_desc[]{
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

    CD3DX12_RASTERIZER_DESC rasterizer_desc(D3D12_DEFAULT);
    rasterizer_desc.CullMode = D3D12_CULL_MODE_NONE;

    CD3DX12_BLEND_DESC blend_desc(D3D12_DEFAULT);
    blend_desc.RenderTarget[0].BlendEnable = TRUE;
    blend_desc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blend_desc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
    blend_desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
    blend_desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

    CD3DX12_DEPTH_STENCIL_DESC depth_stencil_desc(D3D12_DEFAULT);
    depth_stencil_desc.DepthEnable = FALSE;
    depth_stencil_desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
    pso_desc.InputLayout = {ie_desc, _countof(ie_desc)};
    pso_desc.pRootSignature = scene_signature_.Get();
    pso_desc.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
    pso_desc.GS = CD3DX12_SHADER_BYTECODE(gs.Get());
    pso_desc.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
    pso_desc.RasterizerState = rasterizer_desc;
    pso_desc.BlendState = blend_desc;
    pso_desc.DepthStencilState = depth_stencil_desc;
    pso_desc.SampleMask = UINT_MAX;
    pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
    pso_desc.NumRenderTargets = 1;
    pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso_desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    pso_desc.SampleDesc.Count = 1;

    ThrowIfFailed(device_->CreateGraphicsPipelineState(
        &pso_desc, IID_PPV_ARGS(&particle_state_)));
    NAME_D3D12_OBJECT(particle_state_);
  }

  // volumn pipeline state
  {
    ComPtr<ID3DBlob> vs = CompileShader(
        GetAssetFullPath(L"volumn.hlsl").c_str(), nullptr, "VSMain", "vs_5_0");
    ComPtr<ID3DBlob> ps = CompileShader(
        GetAssetFullPath(L"volumn.hlsl").c_str(), nullptr, "PSMain", "ps_5_0");

    D3D12_INPUT_ELEMENT_DESC ie_desc[]{
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

    D3D12_RENDER_TARGET_BLEND_DESC blend_desc{};
    blend_desc.BlendEnable = true;
    blend_desc.LogicOpEnable = false;
    blend_desc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blend_desc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blend_desc.BlendOp = D3D12_BLEND_OP_ADD;
    blend_desc.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend_desc.DestBlendAlpha = D3D12_BLEND_ZERO;
    blend_desc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend_desc.LogicOp = D3D12_LOGIC_OP_NOOP;
    blend_desc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC gps_desc{};

    gps_desc.pRootSignature = scene_signature_.Get();
    gps_desc.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
    gps_desc.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
    gps_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    gps_desc.BlendState.RenderTarget[0] = blend_desc;
    gps_desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    gps_desc.InputLayout = {ie_desc, _countof(ie_desc)};
    gps_desc.SampleMask = UINT_MAX;
    gps_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    gps_desc.NumRenderTargets = 1;
    gps_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    gps_desc.SampleDesc.Count = 1;
    gps_desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    ThrowIfFailed(device_->CreateGraphicsPipelineState(
        &gps_desc, IID_PPV_ARGS(&volumn_state_)));
    NAME_D3D12_OBJECT(volumn_state_);
  }
}

// update
void Smoke::OnUpdate() {
  UpdateWindowText();
  UpdateSolver();
  UpdateScene();
}

void Smoke::UpdateWindowText() {
  timer_.Tick();
  UINT fps{timer_.frames_per_second()};
  ++frame_count_;
  if (frame_count_ == 300) {
    wchar_t fps[64];
    swprintf_s(fps, L"%ufps", timer_.frames_per_second());
    frame_count_ = 0;
  }
  SetCustomWindowText(0.0,
                      0.0, fps);
}

void Smoke::UpdateSolver() {
  Emit();
  Force();
  Project();
  Advect();
  CalInputRadiance();
}

void Smoke::UpdateScene() {
  if (GetAsyncKeyState('0') & 0x8000) {
    scene_constant_.para_type_index = 0;
  }
  if (GetAsyncKeyState('1') & 0x8000) {
    scene_constant_.para_type_index = 1;
  }
  if (GetAsyncKeyState('2') & 0x8000) {
    scene_constant_.para_type_index = 2;
  }
  if (GetAsyncKeyState('3') & 0x8000) {
    scene_constant_.para_type_index = 3;
  }
  if (GetAsyncKeyState('4') & 0x8000) {
    scene_constant_.para_type_index = 4;
  }
  if (GetAsyncKeyState('5') & 0x8000) {
    scene_constant_.para_type_index = 5;
  }


  XMMATRIX model = XMLoadFloat4x4(&Math::Identity4x4());
  XMMATRIX inv_model = XMMatrixInverse(&XMMatrixDeterminant(model), model);
  XMStoreFloat4x4(&scene_constant_.inv_model, XMMatrixTranspose(inv_model));

  float x = camera_.radius * sinf(camera_.phi) * cosf(camera_.theta);
  float y = camera_.radius * cosf(camera_.phi);
  float z = camera_.radius * sinf(camera_.phi) * sinf(camera_.theta);
  XMVECTOR eye_position = XMVectorSet(x, y, z, 1.0f);
  XMStoreFloat4(&scene_constant_.view_pos, eye_position);

  XMVECTOR foucs_pos = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
  XMVECTOR up_dir = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
  XMMATRIX view = XMMatrixLookAtLH(eye_position, foucs_pos, up_dir);
  XMMATRIX proj = XMMatrixPerspectiveFovLH(
      0.25 * XM_PI, static_cast<float>(width_) / height_, 1.0f, 1000.0f);
  XMMATRIX mvp = model * view * proj;
  XMStoreFloat4x4(&scene_constant_.mvp, XMMatrixTranspose(mvp));

  memcpy(scene_constant_data_, &scene_constant_, sizeof(scene_constant_));
}

void Smoke::Emit() {
  // update particle
  {
    ThrowIfFailed(allocators_[frame_index_]->Reset());
    ThrowIfFailed(list_->Reset(allocators_[frame_index_].Get(),
                               update_particle_state_.Get()));

    list_->SetComputeRootSignature(solver_signature_.Get());

    ID3D12DescriptorHeap* heaps[]{cbv_srv_uav_heap_.Get()};
    list_->SetDescriptorHeaps(_countof(heaps), heaps);

    CD3DX12_GPU_DESCRIPTOR_HANDLE solver_cbv_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(),
        kSolverCbvIndex, cbv_srv_uav_size_};

    CD3DX12_GPU_DESCRIPTOR_HANDLE particle_uav_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(),
        kParticlesUavIndex, cbv_srv_uav_size_};

    list_->ResourceBarrier(
        1, &CD3DX12_RESOURCE_BARRIER::Transition(
               particle_.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
               D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

    list_->SetComputeRootDescriptorTable(0, solver_cbv_handle);
    list_->SetComputeRootDescriptorTable(6, particle_uav_handle);

    list_->Dispatch(kParticleCount / 128, 1, 1);

    ThrowIfFailed(list_->Close());
    ID3D12CommandList* Lists[] = {list_.Get()};
    queue_->ExecuteCommandLists(_countof(Lists), Lists);

    WaitForGpu();
  }

  // produce smoke
  {
    ThrowIfFailed(allocators_[frame_index_]->Reset());
    ThrowIfFailed(list_->Reset(allocators_[frame_index_].Get(),
                               produce_smoke_state_.Get()));

    list_->SetComputeRootSignature(solver_signature_.Get());

    ID3D12DescriptorHeap* heaps[]{cbv_srv_uav_heap_.Get()};
    list_->SetDescriptorHeaps(_countof(heaps), heaps);

    CD3DX12_GPU_DESCRIPTOR_HANDLE solver_cbv_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(),
        kSolverCbvIndex, cbv_srv_uav_size_};

    CD3DX12_GPU_DESCRIPTOR_HANDLE particle_srv_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(),
        kParticlesSrvIndex, cbv_srv_uav_size_};

    INT u_uav_index = grid_group_ == 0 ? kU0UavIndex : kU1UavIndex;
    CD3DX12_GPU_DESCRIPTOR_HANDLE u_uav_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(), u_uav_index,
        cbv_srv_uav_size_};

    INT density_uav_index =
        grid_group_ == 0 ? kDensity0UavIndex : kDensity1UavIndex;
    CD3DX12_GPU_DESCRIPTOR_HANDLE density_uav_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(),
        density_uav_index, cbv_srv_uav_size_};

    ID3D12Resource *u, *v, *w, *density, *temp;
    if (grid_group_ == 0) {
      u = u0_.Get();
      v = v0_.Get();
      w = w0_.Get();
      density = density0_.Get();
      temp = temp0_.Get();
    } else {
      u = u1_.Get();
      v = v1_.Get();
      w = w1_.Get();
      density = density1_.Get();
      temp = temp1_.Get();
    }
    CD3DX12_RESOURCE_BARRIER barriers[]{
        CD3DX12_RESOURCE_BARRIER::Transition(
            u, D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
        CD3DX12_RESOURCE_BARRIER::Transition(
            v, D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
        CD3DX12_RESOURCE_BARRIER::Transition(
            w, D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
        CD3DX12_RESOURCE_BARRIER::Transition(
            density, D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
        CD3DX12_RESOURCE_BARRIER::Transition(
            temp, D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
        CD3DX12_RESOURCE_BARRIER::Transition(
            particle_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_GENERIC_READ)};
    list_->ResourceBarrier(_countof(barriers), barriers);

    list_->SetComputeRootDescriptorTable(0, solver_cbv_handle);
    list_->SetComputeRootDescriptorTable(3, particle_srv_handle);
    list_->SetComputeRootDescriptorTable(4, u_uav_handle);
    list_->SetComputeRootDescriptorTable(5, density_uav_handle);

    list_->Dispatch(kParticleCount / 128, 1, 1);

    ThrowIfFailed(list_->Close());
    ID3D12CommandList* Lists[] = {list_.Get()};
    queue_->ExecuteCommandLists(_countof(Lists), Lists);

    WaitForGpu();

    //  static float cur_emit_time{0.0f};
    //  cur_emit_time += kDt;
    //  if (solver_constant_.emit == true && cur_emit_time > kEmitTime) {
    //    solver_constant_.emit = false;
    //    memcpy(solver_constant_data_, &solver_constant_,
    //           sizeof(solver_constant_));
    //  }
  }
}

void Smoke::Force() {
  // calculate average velocity
  {
    ThrowIfFailed(allocators_[frame_index_]->Reset());
    ThrowIfFailed(list_->Reset(allocators_[frame_index_].Get(),
                               cal_avg_velocity_state_.Get()));

    list_->SetComputeRootSignature(solver_signature_.Get());

    ID3D12DescriptorHeap* heaps[]{cbv_srv_uav_heap_.Get()};
    list_->SetDescriptorHeaps(_countof(heaps), heaps);

    CD3DX12_GPU_DESCRIPTOR_HANDLE cbv_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(),
        kSolverCbvIndex, cbv_srv_uav_size_};

    INT u_srv_index = grid_group_ == 0 ? kU0SrvIndex : kU1SrvIndex;
    CD3DX12_GPU_DESCRIPTOR_HANDLE u_srv_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(), u_srv_index,
        cbv_srv_uav_size_};

    CD3DX12_GPU_DESCRIPTOR_HANDLE avg_u_uav_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(), kAvgUUavIndex,
        cbv_srv_uav_size_};

    ID3D12Resource *u, *v, *w;
    if (grid_group_ == 0) {
      u = u0_.Get();
      v = v0_.Get();
      w = w0_.Get();
    } else {
      u = u1_.Get();
      v = v1_.Get();
      w = w1_.Get();
    }
    CD3DX12_RESOURCE_BARRIER barriers[]{
        CD3DX12_RESOURCE_BARRIER::Transition(
            u, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_GENERIC_READ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            v, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_GENERIC_READ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            w, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_GENERIC_READ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            avg_u_.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
        CD3DX12_RESOURCE_BARRIER::Transition(
            avg_v_.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
        CD3DX12_RESOURCE_BARRIER::Transition(
            avg_w_.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS)};
    list_->ResourceBarrier(_countof(barriers), barriers);

    list_->SetComputeRootDescriptorTable(0, cbv_handle);
    list_->SetComputeRootDescriptorTable(1, u_srv_handle);
    list_->SetComputeRootDescriptorTable(4, avg_u_uav_handle);

    list_->Dispatch(kCellCountX / 8, kCellCountY / 8, kCellCountZ / 4);

    ThrowIfFailed(list_->Close());
    ID3D12CommandList* Lists[] = {list_.Get()};
    queue_->ExecuteCommandLists(_countof(Lists), Lists);

    WaitForGpu();
  }

  // calculate vorticity
  {
    ThrowIfFailed(allocators_[frame_index_]->Reset());
    ThrowIfFailed(list_->Reset(allocators_[frame_index_].Get(),
                               cal_vorticity_state_.Get()));

    list_->SetComputeRootSignature(solver_signature_.Get());

    ID3D12DescriptorHeap* heaps[]{cbv_srv_uav_heap_.Get()};
    list_->SetDescriptorHeaps(_countof(heaps), heaps);

    CD3DX12_GPU_DESCRIPTOR_HANDLE solver_cbv_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(),
        kSolverCbvIndex, cbv_srv_uav_size_};
    CD3DX12_GPU_DESCRIPTOR_HANDLE avg_u_srv_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(), kAvgUSrvIndex,
        cbv_srv_uav_size_};
    CD3DX12_GPU_DESCRIPTOR_HANDLE vorticity_x_uav_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(),
        kVorticityXUavIndex, cbv_srv_uav_size_};

    CD3DX12_RESOURCE_BARRIER barriers[]{
        CD3DX12_RESOURCE_BARRIER::Transition(
            avg_u_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_GENERIC_READ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            avg_v_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_GENERIC_READ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            avg_w_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_GENERIC_READ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            vorciticy_x_.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
        CD3DX12_RESOURCE_BARRIER::Transition(
            vorciticy_y_.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
        CD3DX12_RESOURCE_BARRIER::Transition(
            vorciticy_z_.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS)};
    list_->ResourceBarrier(_countof(barriers), barriers);

    list_->SetComputeRootDescriptorTable(0, solver_cbv_handle);
    list_->SetComputeRootDescriptorTable(1, avg_u_srv_handle);
    list_->SetComputeRootDescriptorTable(4, vorticity_x_uav_handle);

    list_->Dispatch(kCellCountX / 8, kCellCountY / 8, kCellCountZ / 4);

    ThrowIfFailed(list_->Close());
    ID3D12CommandList* Lists[] = {list_.Get()};
    queue_->ExecuteCommandLists(_countof(Lists), Lists);

    WaitForGpu();
  }

  // calculate force
  {
    ThrowIfFailed(allocators_[frame_index_]->Reset());
    ThrowIfFailed(
        list_->Reset(allocators_[frame_index_].Get(), cal_force_state_.Get()));

    list_->SetComputeRootSignature(solver_signature_.Get());

    ID3D12DescriptorHeap* heaps[]{cbv_srv_uav_heap_.Get()};
    list_->SetDescriptorHeaps(_countof(heaps), heaps);

    CD3DX12_GPU_DESCRIPTOR_HANDLE solver_cbv_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(),
        kSolverCbvIndex, cbv_srv_uav_size_};

    CD3DX12_GPU_DESCRIPTOR_HANDLE vorticity_x_srv_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(),
        kVorticityXSrvIndex, cbv_srv_uav_size_};

    INT density_srv_index =
        grid_group_ == 0 ? kDensity0SrvIndex : kDensity1SrvIndex;
    CD3DX12_GPU_DESCRIPTOR_HANDLE density_srv_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(),
        density_srv_index, cbv_srv_uav_size_};

    CD3DX12_GPU_DESCRIPTOR_HANDLE force_x_uav_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(),
        kForceXUavIndex, cbv_srv_uav_size_};

    ID3D12Resource *density, *temp;
    if (grid_group_ == 0) {
      density = density0_.Get();
      temp = temp0_.Get();
    } else {
      density = density1_.Get();
      temp = temp1_.Get();
    }
    CD3DX12_RESOURCE_BARRIER barriers[]{
        CD3DX12_RESOURCE_BARRIER::Transition(
            vorciticy_x_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_GENERIC_READ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            vorciticy_y_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_GENERIC_READ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            vorciticy_z_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_GENERIC_READ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            density, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_GENERIC_READ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            temp, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_GENERIC_READ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            force_x_.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
        CD3DX12_RESOURCE_BARRIER::Transition(
            force_y_.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
        CD3DX12_RESOURCE_BARRIER::Transition(
            force_z_.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS)};

    list_->ResourceBarrier(_countof(barriers), barriers);

    list_->SetComputeRootDescriptorTable(0, solver_cbv_handle);
    list_->SetComputeRootDescriptorTable(1, vorticity_x_srv_handle);
    list_->SetComputeRootDescriptorTable(2, density_srv_handle);
    list_->SetComputeRootDescriptorTable(4, force_x_uav_handle);

    list_->Dispatch(kCellCountX / 8, kCellCountY / 8, kCellCountZ / 4);

    ThrowIfFailed(list_->Close());
    ID3D12CommandList* Lists[] = {list_.Get()};
    queue_->ExecuteCommandLists(_countof(Lists), Lists);

    WaitForGpu();
  }

  // force
  {
    ThrowIfFailed(allocators_[frame_index_]->Reset());
    ThrowIfFailed(
        list_->Reset(allocators_[frame_index_].Get(), force_state_.Get()));

    list_->SetComputeRootSignature(solver_signature_.Get());

    ID3D12DescriptorHeap* heaps[]{cbv_srv_uav_heap_.Get()};
    list_->SetDescriptorHeaps(_countof(heaps), heaps);

    CD3DX12_GPU_DESCRIPTOR_HANDLE solver_cbv_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(),
        kSolverCbvIndex, cbv_srv_uav_size_};

    CD3DX12_GPU_DESCRIPTOR_HANDLE force_x_srv_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(),
        kForceXSrvIndex, cbv_srv_uav_size_};

    INT u_uav_index = grid_group_ == 0 ? kU0UavIndex : kU1UavIndex;
    CD3DX12_GPU_DESCRIPTOR_HANDLE u0_uav_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(), u_uav_index,
        cbv_srv_uav_size_};

    ID3D12Resource *u, *v, *w;
    if (grid_group_ == 0) {
      u = u0_.Get();
      v = v0_.Get();
      w = w0_.Get();
    } else {
      u = u1_.Get();
      v = v1_.Get();
      w = w1_.Get();
    }
    CD3DX12_RESOURCE_BARRIER barriers[]{
        CD3DX12_RESOURCE_BARRIER::Transition(
            force_x_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_GENERIC_READ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            force_y_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_GENERIC_READ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            force_z_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_GENERIC_READ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            u, D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
        CD3DX12_RESOURCE_BARRIER::Transition(
            v, D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
        CD3DX12_RESOURCE_BARRIER::Transition(
            w, D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
    };
    list_->ResourceBarrier(_countof(barriers), barriers);

    list_->SetComputeRootDescriptorTable(0, solver_cbv_handle);
    list_->SetComputeRootDescriptorTable(1, force_x_srv_handle);
    list_->SetComputeRootDescriptorTable(4, u0_uav_handle);

    list_->Dispatch(kCellCountX / 8, kCellCountY / 8, kCellCountZ / 4);

    ThrowIfFailed(list_->Close());
    ID3D12CommandList* Lists[] = {list_.Get()};
    queue_->ExecuteCommandLists(_countof(Lists), Lists);

    WaitForGpu();
  }
}

void Smoke::Project() {
  // calculate pressure equation
  {
    ThrowIfFailed(allocators_[frame_index_]->Reset());
    ThrowIfFailed(list_->Reset(allocators_[frame_index_].Get(),
                               cal_pressure_equation_state_.Get()));

    list_->SetComputeRootSignature(solver_signature_.Get());

    ID3D12DescriptorHeap* heaps[]{cbv_srv_uav_heap_.Get()};
    list_->SetDescriptorHeaps(_countof(heaps), heaps);

    CD3DX12_GPU_DESCRIPTOR_HANDLE solver_cbv_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(),
        kSolverCbvIndex, cbv_srv_uav_size_};

    INT u_srv_index = grid_group_ == 0 ? kU0SrvIndex : kU1SrvIndex;
    CD3DX12_GPU_DESCRIPTOR_HANDLE u_srv_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(), u_srv_index,
        cbv_srv_uav_size_};

    CD3DX12_GPU_DESCRIPTOR_HANDLE b_uav_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(), kbUavIndex,
        cbv_srv_uav_size_};

    ID3D12Resource *u, *v, *w;
    if (grid_group_ == 0) {
      u = u0_.Get();
      v = v0_.Get();
      w = w0_.Get();
    } else {
      u = u1_.Get();
      v = v1_.Get();
      w = w1_.Get();
    }
    CD3DX12_RESOURCE_BARRIER barriers[]{
        CD3DX12_RESOURCE_BARRIER::Transition(
            u, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_GENERIC_READ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            v, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_GENERIC_READ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            w, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_GENERIC_READ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            b_.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
        CD3DX12_RESOURCE_BARRIER::Transition(
            A_.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS)};
    list_->ResourceBarrier(_countof(barriers), barriers);

    list_->SetComputeRootDescriptorTable(0, solver_cbv_handle);
    list_->SetComputeRootDescriptorTable(1, u_srv_handle);
    list_->SetComputeRootDescriptorTable(5, b_uav_handle);

    list_->Dispatch(kCellCountX / 8, kCellCountY / 8, kCellCountZ / 4);

    ThrowIfFailed(list_->Close());
    ID3D12CommandList* Lists[] = {list_.Get()};
    queue_->ExecuteCommandLists(_countof(Lists), Lists);

    WaitForGpu();
  }

  // calculate pressure
  {
    ThrowIfFailed(allocators_[frame_index_]->Reset());
    ThrowIfFailed(list_->Reset(allocators_[frame_index_].Get(),
                               cal_pressure_state_.Get()));

    list_->SetComputeRootSignature(solver_signature_.Get());

    ID3D12DescriptorHeap* heaps[]{cbv_srv_uav_heap_.Get()};
    list_->SetDescriptorHeaps(_countof(heaps), heaps);

    CD3DX12_GPU_DESCRIPTOR_HANDLE solver_cbv_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(),
        kSolverCbvIndex, cbv_srv_uav_size_};

    CD3DX12_GPU_DESCRIPTOR_HANDLE b_srv_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(), kbSrvIndex,
        cbv_srv_uav_size_};

    CD3DX12_GPU_DESCRIPTOR_HANDLE p_uav_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(), kpUavIndex,
        cbv_srv_uav_size_};

    CD3DX12_RESOURCE_BARRIER barriers[]{
        CD3DX12_RESOURCE_BARRIER::Transition(
            b_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_GENERIC_READ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            A_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_GENERIC_READ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            p_.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
        CD3DX12_RESOURCE_BARRIER::Transition(
            residual_.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS)};
    list_->ResourceBarrier(_countof(barriers), barriers);

    list_->SetComputeRootDescriptorTable(0, solver_cbv_handle);
    list_->SetComputeRootDescriptorTable(2, b_srv_handle);
    list_->SetComputeRootDescriptorTable(5, p_uav_handle);

    list_->Dispatch(kCellCountX / 8, kCellCountY / 8, kCellCountZ / 4);

    ThrowIfFailed(list_->Close());
    ID3D12CommandList* Lists[] = {list_.Get()};
    queue_->ExecuteCommandLists(_countof(Lists), Lists);

    WaitForGpu();
  }

  // project
  {
    ThrowIfFailed(allocators_[frame_index_]->Reset());
    ThrowIfFailed(
        list_->Reset(allocators_[frame_index_].Get(), project_state_.Get()));

    list_->SetComputeRootSignature(solver_signature_.Get());

    ID3D12DescriptorHeap* heaps[]{cbv_srv_uav_heap_.Get()};
    list_->SetDescriptorHeaps(_countof(heaps), heaps);

    CD3DX12_GPU_DESCRIPTOR_HANDLE solver_cbv_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(),
        kSolverCbvIndex, cbv_srv_uav_size_};

    CD3DX12_GPU_DESCRIPTOR_HANDLE p_srv_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(), kpSrvIndex,
        cbv_srv_uav_size_};

    INT u_uav_index = grid_group_ == 0 ? kU0UavIndex : kU1UavIndex;
    CD3DX12_GPU_DESCRIPTOR_HANDLE u0_uav_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(), u_uav_index,
        cbv_srv_uav_size_};

    ID3D12Resource *u, *v, *w;
    if (grid_group_ == 0) {
      u = u0_.Get();
      v = v0_.Get();
      w = w0_.Get();
    } else {
      u = u1_.Get();
      v = v1_.Get();
      w = w1_.Get();
    }
    CD3DX12_RESOURCE_BARRIER barriers[]{
        CD3DX12_RESOURCE_BARRIER::Transition(
            p_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_GENERIC_READ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            residual_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_GENERIC_READ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            u, D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
        CD3DX12_RESOURCE_BARRIER::Transition(
            v, D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
        CD3DX12_RESOURCE_BARRIER::Transition(
            w, D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS)};
    list_->ResourceBarrier(_countof(barriers), barriers);

    list_->SetComputeRootDescriptorTable(0, solver_cbv_handle);
    list_->SetComputeRootDescriptorTable(3, p_srv_handle);
    list_->SetComputeRootDescriptorTable(4, u0_uav_handle);

    list_->Dispatch(kCellCountX / 8, kCellCountY / 8, kCellCountZ / 4);

    ThrowIfFailed(list_->Close());
    ID3D12CommandList* Lists[] = {list_.Get()};
    queue_->ExecuteCommandLists(_countof(Lists), Lists);

    WaitForGpu();
  }
}

void Smoke::Advect() {
  // advect vector
  {
    ThrowIfFailed(allocators_[frame_index_]->Reset());
    ThrowIfFailed(list_->Reset(allocators_[frame_index_].Get(),
                               advect_vector_state_.Get()));

    list_->SetComputeRootSignature(solver_signature_.Get());

    ID3D12DescriptorHeap* heaps[]{cbv_srv_uav_heap_.Get()};
    list_->SetDescriptorHeaps(_countof(heaps), heaps);

    CD3DX12_GPU_DESCRIPTOR_HANDLE solver_cbv_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(),
        kSolverCbvIndex, cbv_srv_uav_size_};

    INT u_srv_index = grid_group_ == 0 ? kU0SrvIndex : kU1SrvIndex;
    CD3DX12_GPU_DESCRIPTOR_HANDLE u_srv_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(), u_srv_index,
        cbv_srv_uav_size_};

    INT u_uav_index = grid_group_ == 0 ? kU1UavIndex : kU0UavIndex;
    CD3DX12_GPU_DESCRIPTOR_HANDLE u1_uav_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(), u_uav_index,
        cbv_srv_uav_size_};

    ID3D12Resource *prev_u, *prev_v, *prev_w, *cur_u, *cur_v, *cur_w;
    if (grid_group_ == 0) {
      prev_u = u0_.Get();
      prev_v = v0_.Get();
      prev_w = w0_.Get();
      cur_u = u1_.Get();
      cur_v = v1_.Get();
      cur_w = w1_.Get();
    } else {
      prev_u = u1_.Get();
      prev_v = v1_.Get();
      prev_w = w1_.Get();
      cur_u = u0_.Get();
      cur_v = v0_.Get();
      cur_w = w0_.Get();
    }

    CD3DX12_RESOURCE_BARRIER barriers[]{
        CD3DX12_RESOURCE_BARRIER::Transition(
            prev_u, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_GENERIC_READ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            prev_v, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_GENERIC_READ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            prev_w, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_GENERIC_READ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            cur_u, D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
        CD3DX12_RESOURCE_BARRIER::Transition(
            cur_v, D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
        CD3DX12_RESOURCE_BARRIER::Transition(
            cur_w, D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS)};
    list_->ResourceBarrier(_countof(barriers), barriers);

    list_->SetComputeRootDescriptorTable(0, solver_cbv_handle);
    list_->SetComputeRootDescriptorTable(1, u_srv_handle);
    list_->SetComputeRootDescriptorTable(4, u1_uav_handle);

    list_->Dispatch(kCellCountX / 8, kCellCountY / 8, kCellCountZ / 4);

    ThrowIfFailed(list_->Close());
    ID3D12CommandList* Lists[] = {list_.Get()};
    queue_->ExecuteCommandLists(_countof(Lists), Lists);

    WaitForGpu();
  }

  // advect scalar
  {
    ThrowIfFailed(allocators_[frame_index_]->Reset());
    ThrowIfFailed(list_->Reset(allocators_[frame_index_].Get(),
                               advect_scalar_state_.Get()));

    list_->SetComputeRootSignature(solver_signature_.Get());

    ID3D12DescriptorHeap* heaps[]{cbv_srv_uav_heap_.Get()};
    list_->SetDescriptorHeaps(_countof(heaps), heaps);

    CD3DX12_GPU_DESCRIPTOR_HANDLE solver_cbv_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(),
        kSolverCbvIndex, cbv_srv_uav_size_};

    INT u_srv_index = grid_group_ == 0 ? kU1SrvIndex : kU0SrvIndex;
    CD3DX12_GPU_DESCRIPTOR_HANDLE u_srv_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(), u_srv_index,
        cbv_srv_uav_size_};

    INT density_srv_index =
        grid_group_ == 0 ? kDensity0SrvIndex : kDensity1SrvIndex;
    CD3DX12_GPU_DESCRIPTOR_HANDLE density_srv_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(),
        density_srv_index, cbv_srv_uav_size_};

    INT density_uav_index =
        grid_group_ == 0 ? kDensity1UavIndex : kDensity0UavIndex;
    CD3DX12_GPU_DESCRIPTOR_HANDLE density1_uav_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(),
        density_uav_index, cbv_srv_uav_size_};

    ID3D12Resource *u, *v, *w, *density, *temp;
    if (grid_group_ == 0) {
      u = u1_.Get();
      v = v1_.Get();
      w = w1_.Get();
      density = density1_.Get();
      temp = temp1_.Get();
    } else {
      u = u0_.Get();
      v = v0_.Get();
      w = w0_.Get();
      density = density0_.Get();
      temp = temp0_.Get();
    }
    CD3DX12_RESOURCE_BARRIER barriers[]{
        CD3DX12_RESOURCE_BARRIER::Transition(
            u, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_GENERIC_READ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            v, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_GENERIC_READ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            w, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_GENERIC_READ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            density, D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
        CD3DX12_RESOURCE_BARRIER::Transition(
            temp, D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS)};
    list_->ResourceBarrier(_countof(barriers), barriers);

    list_->SetComputeRootDescriptorTable(0, solver_cbv_handle);
    list_->SetComputeRootDescriptorTable(1, u_srv_handle);
    list_->SetComputeRootDescriptorTable(2, density_srv_handle);
    list_->SetComputeRootDescriptorTable(5, density1_uav_handle);

    list_->Dispatch(kCellCountX / 8, kCellCountY / 8, kCellCountZ / 4);

    ThrowIfFailed(list_->Close());
    ID3D12CommandList* Lists[] = {list_.Get()};
    queue_->ExecuteCommandLists(_countof(Lists), Lists);

    WaitForGpu();
  }
}

void Smoke::CalInputRadiance() {
  ThrowIfFailed(allocators_[frame_index_]->Reset());
  ThrowIfFailed(list_->Reset(allocators_[frame_index_].Get(),
                             cal_input_radiance_state_.Get()));

  list_->SetComputeRootSignature(solver_signature_.Get());

  ID3D12DescriptorHeap* heaps[] = {cbv_srv_uav_heap_.Get()};
  list_->SetDescriptorHeaps(_countof(heaps), heaps);

  CD3DX12_GPU_DESCRIPTOR_HANDLE solver_cbv_handle{
      cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(), kSolverCbvIndex,
      cbv_srv_uav_size_};

  INT density_srv_index =
      grid_group_ == 0 ? kDensity1SrvIndex : kDensity0SrvIndex;
  CD3DX12_GPU_DESCRIPTOR_HANDLE density_srv_handle{
      cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(),
      density_srv_index, cbv_srv_uav_size_};

  CD3DX12_GPU_DESCRIPTOR_HANDLE input_radiance_uav_handle{
      cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(),
      kInputRadianceUavIndex, cbv_srv_uav_size_};

  ID3D12Resource *density, *temp;
  if (grid_group_ == 0) {
    density = density1_.Get();
    temp = temp1_.Get();
  } else {
    density = density0_.Get();
    temp = temp0_.Get();
  }
  CD3DX12_RESOURCE_BARRIER barriers[]{
      CD3DX12_RESOURCE_BARRIER::Transition(
          density, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
          D3D12_RESOURCE_STATE_GENERIC_READ),
      CD3DX12_RESOURCE_BARRIER::Transition(
          temp, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
          D3D12_RESOURCE_STATE_GENERIC_READ),
      CD3DX12_RESOURCE_BARRIER::Transition(
          input_radiance_.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
          D3D12_RESOURCE_STATE_UNORDERED_ACCESS)};
  list_->ResourceBarrier(_countof(barriers), barriers);

  list_->SetComputeRootDescriptorTable(0, solver_cbv_handle);
  list_->SetComputeRootDescriptorTable(3, density_srv_handle);
  list_->SetComputeRootDescriptorTable(6, input_radiance_uav_handle);

  list_->Dispatch(kCellCountX / 8, kCellCountY / 8, kCellCountZ / 4);

  ThrowIfFailed(list_->Close());
  ID3D12CommandList* Lists[] = {list_.Get()};
  queue_->ExecuteCommandLists(_countof(Lists), Lists);

  WaitForGpu();
}

// render
void Smoke::OnRender() {
  // populate graphics command list
  {
    ThrowIfFailed(allocators_[frame_index_]->Reset());
    ThrowIfFailed(
        list_->Reset(allocators_[frame_index_].Get(), blackbody_state_.Get()));

    list_->SetGraphicsRootSignature(scene_signature_.Get());
    list_->RSSetViewports(1, &viewport_);
    list_->RSSetScissorRects(1, &rect_);

    list_->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
                                  render_targets_[frame_index_].Get(),
                                  D3D12_RESOURCE_STATE_PRESENT,
                                  D3D12_RESOURCE_STATE_RENDER_TARGET));

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle{
        rtv_heap_->GetCPUDescriptorHandleForHeapStart(), (INT)frame_index_,
        rtv_size_};
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsv_handle{
        dsv_heap_->GetCPUDescriptorHandleForHeapStart()};
    list_->OMSetRenderTargets(1, &rtv_handle, TRUE, &dsv_handle);

    const float clear_color[]{0.2f, 0.2f, 0.2f, 1.0f};
    list_->ClearRenderTargetView(rtv_handle, clear_color, 0, nullptr);
    list_->ClearDepthStencilView(
        dsv_handle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0,
        0, nullptr);

    ID3D12DescriptorHeap* heaps[]{cbv_srv_uav_heap_.Get()};
    list_->SetDescriptorHeaps(_countof(heaps), heaps);

    CD3DX12_GPU_DESCRIPTOR_HANDLE solver_cbv_handle = {
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(),
        kSolverCbvIndex, cbv_srv_uav_size_};

    list_->SetGraphicsRootDescriptorTable(0, solver_cbv_handle);

    // blackbody
    /*list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    list_->IASetVertexBuffers(0, 1, &blackbody_VBV_);
    list_->IASetIndexBuffer(&blackbody_IBV_);
    list_->DrawIndexedInstanced(36, 1, 0, 0, 0);*/

    // wireframe
    list_->SetPipelineState(wireframe_state_.Get());
    list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
    list_->IASetVertexBuffers(0, 1, &wireframe_VBV_);
    list_->IASetIndexBuffer(&wireframe_IBV_);
    list_->DrawIndexedInstanced(24, 1, 0, 0, 0);

    // particle
    /*list_->SetPipelineState(particle_state_.Get());

    CD3DX12_GPU_DESCRIPTOR_HANDLE particle_srv_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(),
        kParticlesSrvIndex, cbv_srv_uav_size_};

    list_->ResourceBarrier(
        1, &CD3DX12_RESOURCE_BARRIER::Transition(
               particle_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
               D3D12_RESOURCE_STATE_GENERIC_READ));

    list_->SetGraphicsRootDescriptorTable(2, particle_srv_handle);

    list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
    list_->IASetVertexBuffers(0, 1, &particle_VBV_);
    list_->DrawInstanced(kParticleCount, 1, 0, 0);*/

    // volumn
    list_->SetPipelineState(volumn_state_.Get());

    INT density_srv_index =
        grid_group_ == 0 ? kDensity1SrvIndex : kDensity0SrvIndex;
    CD3DX12_GPU_DESCRIPTOR_HANDLE density_srv_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(),
        density_srv_index, cbv_srv_uav_size_};

    CD3DX12_GPU_DESCRIPTOR_HANDLE input_radiance_srv_handle{
        cbv_srv_uav_heap_->GetGPUDescriptorHandleForHeapStart(),
        kInputRadianceSrvIndex, cbv_srv_uav_size_};

    list_->ResourceBarrier(
        1, &CD3DX12_RESOURCE_BARRIER::Transition(
               input_radiance_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
               D3D12_RESOURCE_STATE_GENERIC_READ));

    list_->SetGraphicsRootDescriptorTable(0, solver_cbv_handle);
    list_->SetGraphicsRootDescriptorTable(1, density_srv_handle);
    list_->SetGraphicsRootDescriptorTable(2, input_radiance_srv_handle);

    list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    list_->IASetVertexBuffers(0, 1, &volume_VBV_);
    list_->IASetIndexBuffer(&volume_IBV_);
    list_->DrawIndexedInstanced(36, 1, 0, 0, 0);

    list_->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
                                  render_targets_[frame_index_].Get(),
                                  D3D12_RESOURCE_STATE_RENDER_TARGET,
                                  D3D12_RESOURCE_STATE_PRESENT));
  }

  // execute the command list and present the frame
  {
    ThrowIfFailed(list_->Close());
    ID3D12CommandList* lists[] = {list_.Get()};
    queue_->ExecuteCommandLists(_countof(lists), lists);
    ThrowIfFailed(chain_->Present(1, 0));
  }

  // move to next frame
  {
    WaitForGpu();
    frame_index_ = chain_->GetCurrentBackBufferIndex();
    grid_group_ = (grid_group_ + 1) % kGridCount;
  }
}

// destroy
void Smoke::OnDestroy() {
  // Cleanup
  WaitForGpu();
  CloseHandle(fence_event_);
}

// mouse
void Smoke::OnMouseDown(WPARAM btn_state, int x, int y) {
  last_mouse_pos_.x = x;
  last_mouse_pos_.y = y;
  SetCapture(Win32::h_wnd());
}

void Smoke::OnMouseUp(WPARAM btn_state, int x, int y) { ReleaseCapture(); }

void Smoke::OnMouseMove(WPARAM btn_state, int x, int y) {
  if ((btn_state & MK_LBUTTON) != 0) {
    float dx =
        XMConvertToRadians(0.25f * static_cast<float>(x - last_mouse_pos_.x));
    float dy =
        XMConvertToRadians(0.25f * static_cast<float>(y - last_mouse_pos_.y));
    camera_.theta += dx;
    camera_.phi += dy;
    camera_.phi = Math::Clamp(camera_.phi, 0.1f, XM_PI - 0.1f);
  } else if ((btn_state & MK_RBUTTON) != 0) {
    float dx = 0.05f * static_cast<float>(x - last_mouse_pos_.x);
    float dy = 0.05f * static_cast<float>(y - last_mouse_pos_.y);
    camera_.radius += dx - dy;
    camera_.radius = Math::Clamp(camera_.radius, 50.0f, 250.0f);
  }
  last_mouse_pos_.x = x;
  last_mouse_pos_.y = y;
}

void Smoke::OnMouseWheel(float delta) {
  camera_.radius += 5.0f * delta;
  camera_.radius = Math::Clamp(camera_.radius, 50.0f, 500.0f);
}

// helper
void Smoke::CreateGridFLoat(ComPtr<ID3D12Resource>& texture, int width,
                            int height, int depth, int srv_index,
                            int uav_index) {
  D3D12_RESOURCE_DESC texture_desc{};
  texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
  texture_desc.Alignment = 0;
  texture_desc.Width = width;
  texture_desc.Height = height;
  texture_desc.DepthOrArraySize = depth;
  texture_desc.MipLevels = 1;
  texture_desc.Format = DXGI_FORMAT_R32_FLOAT;
  texture_desc.SampleDesc.Count = 1;
  texture_desc.SampleDesc.Quality = 0;
  texture_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  texture_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

  D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
  srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srv_desc.Format = DXGI_FORMAT_R32_FLOAT;
  srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
  srv_desc.Texture3D.MostDetailedMip = 0;
  srv_desc.Texture3D.MipLevels = 1;
  srv_desc.Texture3D.ResourceMinLODClamp = 0.0f;

  D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
  uav_desc.Format = DXGI_FORMAT_R32_FLOAT;
  uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
  uav_desc.Texture3D.MipSlice = 0;
  uav_desc.Texture3D.FirstWSlice = 0;
  uav_desc.Texture3D.WSize = texture_desc.DepthOrArraySize;

  ThrowIfFailed(device_->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
      &texture_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
      IID_PPV_ARGS(&texture)));

  CD3DX12_CPU_DESCRIPTOR_HANDLE cpu_srv_handle{
      cbv_srv_uav_heap_->GetCPUDescriptorHandleForHeapStart(), srv_index,
      cbv_srv_uav_size_};
  device_->CreateShaderResourceView(texture.Get(), &srv_desc, cpu_srv_handle);

  CD3DX12_CPU_DESCRIPTOR_HANDLE cpu_uav_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
      cbv_srv_uav_heap_->GetCPUDescriptorHandleForHeapStart(), uav_index,
      cbv_srv_uav_size_);
  device_->CreateUnorderedAccessView(texture.Get(), nullptr, &uav_desc,
                                     cpu_uav_handle);
}

void Smoke::CreateGridFloat4(ComPtr<ID3D12Resource>& texture, int width,
                             int height, int depth, int srv_index,
                             int uav_index) {
  D3D12_RESOURCE_DESC texture_desc{};
  texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
  texture_desc.Alignment = 0;
  texture_desc.Width = width;
  texture_desc.Height = height;
  texture_desc.DepthOrArraySize = depth;
  texture_desc.MipLevels = 1;
  texture_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
  texture_desc.SampleDesc.Count = 1;
  texture_desc.SampleDesc.Quality = 0;
  texture_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  texture_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

  D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
  srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srv_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
  srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
  srv_desc.Texture3D.MostDetailedMip = 0;
  srv_desc.Texture3D.MipLevels = 1;
  srv_desc.Texture3D.ResourceMinLODClamp = 0.0f;

  D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
  uav_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
  uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
  uav_desc.Texture3D.MipSlice = 0;
  uav_desc.Texture3D.FirstWSlice = 0;
  uav_desc.Texture3D.WSize = depth;

  ThrowIfFailed(device_->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
      &texture_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
      IID_PPV_ARGS(&texture)));

  CD3DX12_CPU_DESCRIPTOR_HANDLE cpu_srv_handle{
      cbv_srv_uav_heap_->GetCPUDescriptorHandleForHeapStart(), srv_index,
      cbv_srv_uav_size_};
  device_->CreateShaderResourceView(texture.Get(), &srv_desc, cpu_srv_handle);

  CD3DX12_CPU_DESCRIPTOR_HANDLE cpu_uav_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
      cbv_srv_uav_heap_->GetCPUDescriptorHandleForHeapStart(), uav_index,
      cbv_srv_uav_size_);
  device_->CreateUnorderedAccessView(texture.Get(), nullptr, &uav_desc,
                                     cpu_uav_handle);
}

void Smoke::WaitForGpu() {
  const UINT64 cur_fence_value{++fence_value_};
  ThrowIfFailed(queue_->Signal(fence_.Get(), cur_fence_value));
  if (fence_->GetCompletedValue() < cur_fence_value) {
    ThrowIfFailed(fence_->SetEventOnCompletion(cur_fence_value, fence_event_));
    WaitForSingleObjectEx(fence_event_, INFINITE, FALSE);
  }
}