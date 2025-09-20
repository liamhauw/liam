#include "common.hlsli"

Texture3D<float> gAvgUGrid : register(t0);
Texture3D<float> gAvgVGrid : register(t1);
Texture3D<float> gAvgWGrid : register(t2);

RWTexture3D<float> gVorticityXGrid : register(u0);
RWTexture3D<float> gVorticityYGrid : register(u1);
RWTexture3D<float> gVorticityZGrid : register(u2);

SamplerState gLinearClamp : register(s0);

[numthreads(8, 8, 4)]
void CSMain(uint3 dtid : SV_DispatchThreadID) {
  uint i = dtid.x;
  uint j = dtid.y;
  uint k = dtid.z;
  // 计算速度的旋度
  if (i > 0 && i < gCellCount.x - 1 && j > 0 && j < gCellCount.y - 1 && k > 0 && k < gCellCount.z - 1) {
    float top_w = gAvgWGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(uint3(i, j + 1, k)), 0.0);
    float bottom_w = gAvgWGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(uint3(i, j - 1, k)), 0.0);
    float far_v = gAvgVGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(uint3(i, j, k + 1)), 0.0);
    float near_v = gAvgVGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(uint3(i, j, k - 1)), 0.0);
    gVorticityXGrid[dtid] = (top_w - bottom_w - far_v + near_v) * 0.5 / gCellSize;

    float far_u = gAvgUGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(uint3(i, j, k + 1)), 0.0);
    float near_u = gAvgUGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(uint3(i, j, k - 1)), 0.0);
    float right_w = gAvgWGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(uint3(i + 1, j, k)), 0.0);
    float left_w = gAvgWGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(uint3(i - 1, j, k)), 0.0);
    gVorticityYGrid[dtid] = (far_u - near_u - right_w + left_w) * 0.5 / gCellSize;

    float right_v = gAvgVGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(uint3(i + 1, j, k)), 0.0);
    float left_v = gAvgVGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(uint3(i - 1, j, k)), 0.0);
    float top_u = gAvgUGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(uint3(i, j + 1, k)), 0.0);
    float bottom_u = gAvgUGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(uint3(i, j - 1, k)), 0.0);
    gVorticityZGrid[dtid] = (right_v - left_v - top_u + bottom_u) * 0.5 / gCellSize;
  }
}