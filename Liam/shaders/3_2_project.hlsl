#include "common.hlsli"

Texture3D<float> gp : register(t5);

RWTexture3D<float> gCurUGrid : register(u0);
RWTexture3D<float> gCurVGrid : register(u1);
RWTexture3D<float> gCurWGrid : register(u2);

SamplerState gLinearClamp : register(s0);

[numthreads(8, 8, 4)]
void CSMain(uint3 dtid : SV_DispatchThreadID) {
  float3 cur_p_tpos = VoxelCenterDtid2Tpos(dtid);
  float cur_p = gp.SampleLevel(gLinearClamp, cur_p_tpos, 0.0);

  // 更新速度场，使速度的散度为零
  if (dtid.x < gCellCount.x - 1) {
    float3 right_p_tpos = VoxelCenterDtid2Tpos(dtid + uint3(1, 0, 0));
    float right_p = gp.SampleLevel(gLinearClamp, right_p_tpos, 0.0);

    gCurUGrid[dtid + uint3(1, 0, 0)] -= gDt * (right_p - cur_p) / gCellSize;
  }

  if (dtid.y < gCellCount.y - 1) {
    float3 top_p_tpos = VoxelCenterDtid2Tpos(dtid + uint3(0, 1, 0));
    float top_p = gp.SampleLevel(gLinearClamp, top_p_tpos, 0.0);

    gCurVGrid[dtid + uint3(0, 1, 0)] -= gDt * (top_p - cur_p) / gCellSize;
  }

  if (dtid.z < gCellCount.z - 1) {
    float3 far_p_tpos = VoxelCenterDtid2Tpos(dtid + uint3(0, 0, 1));
    float far_p = gp.SampleLevel(gLinearClamp, far_p_tpos, 0.0);

    gCurWGrid[dtid + uint3(0, 0, 1)] -= gDt * (far_p - cur_p) / gCellSize;
  }
}