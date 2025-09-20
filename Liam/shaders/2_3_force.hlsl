#include "common.hlsli"

Texture3D<float> gForceXGrid : register(t0);
Texture3D<float> gForceYGrid : register(t1);
Texture3D<float> gForceZGrid : register(t2);

RWTexture3D<float> gCurUGrid : register(u0);
RWTexture3D<float> gCurVGrid : register(u1);
RWTexture3D<float> gCurWGrid : register(u2);

SamplerState gLinearClamp : register(s0);

[numthreads(8, 8, 4)]
void CSMain(uint3 dtid : SV_DispatchThreadID) {
  uint i = dtid.x;
  uint j = dtid.y;
  uint k = dtid.z;

  // 使用外力更新速度
  if (i < gCellCount.x - 1) {
    float left_force_x = gForceXGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(dtid), 0.0);
    float right_force_x = gForceXGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(dtid + uint3(1, 0, 0)), 0.0);
    gCurUGrid[dtid + uint3(1, 0, 0)] += (left_force_x + right_force_x) * 0.5 * gDt;
  }
  if (j < gCellCount.y - 1) {
    float bottom_force_y = gForceYGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(dtid), 0.0);
    float top_force_y = gForceYGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(dtid + uint3(0, 1, 0)), 0.0);
    gCurVGrid[dtid + uint3(0, 1, 0)] += (bottom_force_y + top_force_y) * 0.5 * gDt;
  } 
  if (k < gCellCount.z - 1) {
    float near_force_z = gForceZGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(dtid), 0.0);
    float far_force_z = gForceZGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(dtid + uint3(0, 0, 1)), 0.0);
    gCurWGrid[dtid + uint3(0, 0, 1)] += (near_force_z + far_force_z) * 0.5 * gDt;
  }

#if (FREE_BOUNDARY == 1)
  if (i == 0) {
    gCurUGrid[dtid] = gCurUGrid[dtid + uint3(1, 0, 0)];
  }
  if (j == 0) {
    gCurVGrid[dtid] = gCurVGrid[dtid + uint3(0, 1, 0)];
  } 
  if (k == 0) {
    gCurWGrid[dtid] = gCurWGrid[dtid + uint3(0, 0, 1)];
  }

  if (i == (gCellCount.x - 1)) {
    gCurUGrid[dtid + uint3(1, 0, 0)] = gCurUGrid[dtid];
  }
  if (j == (gCellCount.y - 1)) {
    gCurVGrid[dtid + uint3(0, 1, 0)] = gCurVGrid[dtid];
  } 
  if (k == (gCellCount.z - 1)) {
    gCurWGrid[dtid + uint3(0, 0, 1)] = gCurWGrid[dtid];
  }  

#endif

  
}