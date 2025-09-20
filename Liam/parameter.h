#ifndef PARAMETER_H
#define PARAMETER_H

#include "math.h"

// -------------------------- solver parameters----------------------

constexpr D3D_SHADER_MACRO kShaderDefines[]{
    "FREE_BOUNDARY", "1",  // 0: 固体边界  1：自由边界
    "ADVECT_METHOD", "4",  // 1：RK1  2:RK2  3:RK3  4: BFECC
    NULL,
    NULL};

constexpr UINT kMaxBombCount{6u};
struct BombPara {
  DirectX::XMFLOAT3 scale;
  DirectX::XMFLOAT3 rotation;
  DirectX::XMFLOAT3 tanslation;
  DirectX::XMFLOAT4 mass_vel;
  float length;
  float radius;
};

constexpr UINT kBombCount{1u};
constexpr BombPara kBombPara[kMaxBombCount]{
    {
        DirectX::XMFLOAT3{1.0f, 1.0f, 1.0f},           // scale
        DirectX::XMFLOAT3{0.0f, 0.0f, 0.0f},           // rotation
        DirectX::XMFLOAT3{0.0f, 0.0f, 0.0f},           // translation
        DirectX::XMFLOAT4{1.2f, 0.2f, 300.0f, 10.0f},  // mass and velocity
        25.0f,                                         // length
        5.0f                                           // radius
    },

    //{
    //    DirectX::XMFLOAT3{1.0f, 1.0f, 1.0f},           // scale
    //    DirectX::XMFLOAT3{0.0f, 0.0f, 0.2f},           // rotation
    //    DirectX::XMFLOAT3{-60.0f, 0.0f, 0.0f},           // translation
    //    DirectX::XMFLOAT4{1.2f, 0.2f, 250.0f, 15.0f},  // mass and velocity
    //    25.0f,                                         // length
    //    5.0f                                           // radius
    //},

    //{
    //    DirectX::XMFLOAT3{1.0f, 1.0f, 0.6f},           // scale
    //    DirectX::XMFLOAT3{0.0f, 0.8f, 0.4f},           // rotation
    //    DirectX::XMFLOAT3{-150.0f, -10.0f, 0.0f},           // translation
    //    DirectX::XMFLOAT4{2.0f, 0.2f, 280.0f, 20.0f},  // mass and velocity
    //    25.0f,                                         // length
    //    3.0f                                           // radius
    //},

    //{
    //    DirectX::XMFLOAT3{1.0f, 1.0f, 0.6f},           // scale
    //    DirectX::XMFLOAT3{1.78f, 0.0f, 0.3f},           // rotation
    //    DirectX::XMFLOAT3{25.0f, 30.0f, 0.0f},           // translation
    //    DirectX::XMFLOAT4{2.0f, 0.2f, 270.0f, 20.0f},  // mass and velocity
    //    25.0f,                                         // length
    //    3.0f                                           // radius
    //},

    //{
    //    DirectX::XMFLOAT3{1.0f, 1.0f, 0.6f},           // scale
    //    DirectX::XMFLOAT3{-0.2f, -0.6f, 0.0f},           // rotation
    //    DirectX::XMFLOAT3{170.0f, 30.0f, 0.0f},           // translation
    //    DirectX::XMFLOAT4{2.0f, 0.2f, 270.0f, 20.0f},  // mass and velocity
    //    25.0f,                                         // length
    //    3.0f                                           // radius
    //},

};
constexpr int kParticleCount{5120 * kBombCount};
constexpr int kCellCountX{200};
constexpr int kCellCountY{200};
constexpr int kCellCountZ{100};
constexpr float kCellSize{1.0f};
constexpr float kDt{0.02f};
constexpr float kAmbientTemp{300.0f};
constexpr float kAlpha{0.1f};
constexpr float kBeta{0.008f};
constexpr float kEpsilon{5.0f};
constexpr float kAtteMass{0.95f};
constexpr float kAtteVel{0.90f};
constexpr float kEmDens{240.0f};
constexpr float kEmTemp{360.0f};
constexpr DirectX::XMFLOAT3 kWindPower{2.0f, 0.0f, 0.0f};

// -------------------------- scene parameters----------------------

constexpr float kViewDistance{350.0};
constexpr DirectX::XMFLOAT3 kLightPos{-40.0f, 200.0f, -40.0f};
constexpr float gLightRadiance{100.0f};
constexpr UINT kWaveBand{2};

// ------------------------- don't change -------------------------
struct BombInfo {
  DirectX::XMFLOAT4X4 model;
  DirectX::XMFLOAT4 mass_vel;
  float length;
  float radius;
  float pad0{};
  float pad1{};
};

struct SolverConstant {
  BombInfo info[kMaxBombCount];

  UINT bomb_count{kBombCount};
  DirectX::XMUINT3 cell_count{kCellCountX, kCellCountY, kCellCountZ};

  float cell_size{kCellSize};
  DirectX::XMFLOAT3 grid_size{kCellSize * kCellCountX, kCellSize* kCellCountY,
                              kCellSize* kCellCountZ};

  float ambient_temp{kAmbientTemp};
  DirectX::XMFLOAT3 grid_lbn_pos{kCellSize * kCellCountX * -0.5f,
                                 kCellSize* kCellCountY * -0.5f,
                                 kCellSize* kCellCountZ * -0.5f};

  DirectX::XMFLOAT3 grid_rtf_pos{kCellSize * kCellCountX * 0.5f,
                                 kCellSize* kCellCountY * 0.5f,
                                 kCellSize* kCellCountZ * 0.5f};
  float alpha{kAlpha};

  float beta{kBeta};
  float epsilon{kEpsilon};
  float atte_mass{kAtteMass};
  float atte_vel{kAtteVel};

  DirectX::XMFLOAT3 wind_power{kWindPower};
  float em_dens{kEmDens};

  float em_temp{kEmTemp};
  float dt{kDt};
};

struct SceneConstant {
  DirectX::XMFLOAT4X4 mvp{Math::Identity4x4()};
  DirectX::XMFLOAT4X4 inv_model{Math::Identity4x4()};
  DirectX::XMFLOAT4 view_pos{};

  DirectX::XMFLOAT3 light_pos{kLightPos};
  float light_radiance{gLightRadiance};

  UINT para_type_index{kWaveBand};
  DirectX::XMFLOAT3 ext{3.36f, 0.29f, 0.27f};
};

#endif  // !PARAMETER_H
