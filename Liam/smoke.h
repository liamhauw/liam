#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "camera.h"
#include "dx.h"
#include "math.h"
#include "parameter.h"
#include "timer.h"

using Microsoft::WRL::ComPtr;

class Smoke : public DX {
 public:
  Smoke(UINT width, UINT height, std::wstring name);

  virtual void OnInit();
  virtual void OnUpdate();
  virtual void OnRender();
  virtual void OnDestroy();

  virtual void OnMouseDown(WPARAM btn_state, int x, int y);
  virtual void OnMouseUp(WPARAM btn_state, int x, int y);
  virtual void OnMouseMove(WPARAM btn_state, int x, int y);
  virtual void OnMouseWheel(float delta);

 private:
  void CreateBase();
  void CreateConstant();
  void CreateParticle();
  void CreateGrid();
  void CreateState();
  void CreateScene();
  void ExcuteCreateCommand();
  void InitParticle();
  void InitGrid();
  void CreateSolverState();
  void CreateSceneState();

  void UpdateWindowText();
  void UpdateSolver();
  void UpdateScene();
  void Emit();
  void Force();
  void Project();
  void Advect();
  void CalInputRadiance();

  void CreateGridFLoat(ComPtr<ID3D12Resource>& texture, int width, int height,
                       int depth, int srv_index, int uav_index);
  void CreateGridFloat4(ComPtr<ID3D12Resource>& texture, int width, int height,
                        int depth, int srv_index, int uav_index);

  void WaitForGpu();

  // 1.base
  static const UINT kFrameCount = 3;
  ComPtr<ID3D12Device> device_;
  ComPtr<ID3D12CommandQueue> queue_;
  ComPtr<IDXGISwapChain3> chain_;
  ComPtr<ID3D12DescriptorHeap> dsv_heap_;
  ComPtr<ID3D12DescriptorHeap> rtv_heap_;
  ComPtr<ID3D12DescriptorHeap> cbv_srv_uav_heap_;
  ComPtr<ID3D12Resource> depth_stencil_;
  ComPtr<ID3D12Resource> render_targets_[kFrameCount];
  ComPtr<ID3D12CommandAllocator> allocators_[kFrameCount];
  ComPtr<ID3D12GraphicsCommandList> list_;
  ComPtr<ID3D12Fence> fence_;
  UINT frame_index_{};
  UINT frame_count_{};
  UINT dsv_size_{};
  UINT rtv_size_{};
  UINT cbv_srv_uav_size_{};
  HANDLE fence_event_{};
  UINT64 fence_value_{};
  POINT last_mouse_pos_{};
  CD3DX12_VIEWPORT viewport_;
  CD3DX12_RECT rect_;
  Timer timer_;
  Camera camera_;

  // 2 constant
  // 2.1 solver
  ComPtr<ID3D12Resource> solver_cb_;
  SolverConstant solver_constant_;
  UINT8* solver_constant_data_{};

  // 2.2 scene
  ComPtr<ID3D12Resource> scene_cb_;
  SceneConstant scene_constant_;
  UINT8* scene_constant_data_{};

  // 3 grid
  static const UINT kGridCount{2};
  UINT grid_group_{0};

  ComPtr<ID3D12Resource> particle_;
  ComPtr<ID3D12Resource> u0_, v0_, w0_;
  ComPtr<ID3D12Resource> density0_, temp0_;
  ComPtr<ID3D12Resource> u1_, v1_, w1_;
  ComPtr<ID3D12Resource> density1_, temp1_;
  ComPtr<ID3D12Resource> avg_u_, avg_v_, avg_w_;
  ComPtr<ID3D12Resource> vorciticy_x_, vorciticy_y_, vorciticy_z_;
  ComPtr<ID3D12Resource> force_x_, force_y_, force_z_;
  ComPtr<ID3D12Resource> b_, A_;
  ComPtr<ID3D12Resource> p_, residual_;
  ComPtr<ID3D12Resource> input_radiance_;

  // data
  ComPtr<ID3D12Resource> k_;
  ComPtr<ID3D12Resource> k_upload_;
  ComPtr<ID3D12Resource> mu_;
  ComPtr<ID3D12Resource> mu_upload_;

  // 4 state
  // 4.1 solver
  ComPtr<ID3D12RootSignature> solver_signature_;
  ComPtr<ID3D12PipelineState> init_particle_state_;
  ComPtr<ID3D12PipelineState> init_grid_state_;
  ComPtr<ID3D12PipelineState> update_particle_state_;
  ComPtr<ID3D12PipelineState> produce_smoke_state_;
  ComPtr<ID3D12PipelineState> cal_avg_velocity_state_;
  ComPtr<ID3D12PipelineState> cal_vorticity_state_;
  ComPtr<ID3D12PipelineState> cal_force_state_;
  ComPtr<ID3D12PipelineState> force_state_;
  ComPtr<ID3D12PipelineState> cal_pressure_equation_state_;
  ComPtr<ID3D12PipelineState> cal_pressure_state_;
  ComPtr<ID3D12PipelineState> project_state_;
  ComPtr<ID3D12PipelineState> advect_vector_state_;
  ComPtr<ID3D12PipelineState> advect_scalar_state_;
  ComPtr<ID3D12PipelineState> cal_input_radiance_state_;

  // 4.2 scene
  ComPtr<ID3D12RootSignature> scene_signature_;
  ComPtr<ID3D12PipelineState> blackbody_state_;
  ComPtr<ID3D12PipelineState> wireframe_state_;
  ComPtr<ID3D12PipelineState> particle_state_;
  ComPtr<ID3D12PipelineState> volumn_state_;

  // 5. scene
  ComPtr<ID3D12Resource> blackbody_vb_;
  ComPtr<ID3D12Resource> blackbody_ib_;
  ComPtr<ID3D12Resource> blackbody_vb_upload_;
  ComPtr<ID3D12Resource> blackbody_ib_upload_;
  D3D12_VERTEX_BUFFER_VIEW blackbody_VBV_;
  D3D12_INDEX_BUFFER_VIEW blackbody_IBV_;

  ComPtr<ID3D12Resource> wireframe_vb_;
  ComPtr<ID3D12Resource> wireframe_ib_;
  ComPtr<ID3D12Resource> wireframe_vb_upload_;
  ComPtr<ID3D12Resource> wireframe_ib_upload_;
  D3D12_VERTEX_BUFFER_VIEW wireframe_VBV_;
  D3D12_INDEX_BUFFER_VIEW wireframe_IBV_;

  ComPtr<ID3D12Resource> particle_vb_;
  ComPtr<ID3D12Resource> particle_vb_upload_;
  D3D12_VERTEX_BUFFER_VIEW particle_VBV_;

  ComPtr<ID3D12Resource> volume_vb_;
  ComPtr<ID3D12Resource> volume_ib_;
  ComPtr<ID3D12Resource> volume_vb_upload_;
  ComPtr<ID3D12Resource> volume_ib_upload_;
  D3D12_VERTEX_BUFFER_VIEW volume_VBV_;
  D3D12_INDEX_BUFFER_VIEW volume_IBV_;
};

struct Particle {
  DirectX::XMFLOAT3 position;
  float mass;
  DirectX::XMFLOAT3 velocity;
};

struct ParticleVertex {
  DirectX::XMFLOAT4 color;
};

struct Vertex {
  DirectX::XMFLOAT3 position;
};

enum CbvSrvUavHeap : UINT {
  // 0 cbv
  kSolverCbvIndex,
  kSceneCbvIndex,

  // 1 srv
  // 1.1 velocity0
  kU0SrvIndex,
  kV0SrvIndex,
  kW0SrvIndex,
  // 1.2 density0 and temp0
  kDensity0SrvIndex,
  kTemp0SrvIndex,
  // 1.2 velocity1
  kU1SrvIndex,
  kV1SrvIndex,
  kW1SrvIndex,
  // 1.3 density1 and temp1
  kDensity1SrvIndex,
  kTemp1SrvIndex,
  // 1.4 others
  kAvgUSrvIndex,
  kAvgVSrvIndex,
  kAvgWSrvIndex,
  kVorticityXSrvIndex,
  kVorticityYSrvIndex,
  kVorticityZSrvIndex,
  kForceXSrvIndex,
  kForceYSrvIndex,
  kForceZSrvIndex,
  kbSrvIndex,
  kASrvIndex,
  kpSrvIndex,
  kResiduaSrvIndex,
  kInputRadianceSrvIndex,
  kParticlesSrvIndex,

  // 2 uav
  // 2.1 velocity0
  kU0UavIndex,
  kV0UavIndex,
  kW0UavIndex,
  // 2.2 density0 and temp0
  kDensity0UavIndex,
  kTemp0UavIndex,
  // 2.2 velocity1
  kU1UavIndex,
  kV1UavIndex,
  kW1UavIndex,
  // 2.3 density1 and temp1
  kDensity1UavIndex,
  kTemp1UavIndex,
  // 2.4 others
  kAvgUUavIndex,
  kAvgVUavIndex,
  kAvgWUavIndex,
  kVorticityXUavIndex,
  kVorticityYUavIndex,
  kVorticityZUavIndex,
  kForceXUavIndex,
  kForceYUavIndex,
  kForceZUavIndex,
  kbUavIndex,
  kAUavIndex,
  kpUavIndex,
  kResiduaUavIndex,
  kInputRadianceUavIndex,
  kParticlesUavIndex,

  // 4 Count
  kCbvSrvUavCount
};

#endif  // !SIMULATOR_H
