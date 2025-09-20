#include "common.hlsli"

Texture3D<float> gb : register(t3);
Texture3D<float4> gA : register(t4);

globallycoherent RWTexture3D<float> gp : register(u3);
RWTexture3D<float> gResidual : register(u4);

SamplerState gLinearClamp : register(s0);

[numthreads(8, 8, 4)]
void CSMain(uint3 dtid : SV_DispatchThreadID) {
  uint3 left_dtid = dtid - uint3(1, 0, 0);
  uint3 right_dtid = dtid + uint3(1, 0, 0);
  uint3 bottom_dtid = dtid - uint3(0, 1, 0);
  uint3 top_dtid = dtid + uint3(0, 1, 0);
  uint3 near_dtid = dtid - uint3(0, 0, 1);
  uint3 far_dtid = dtid + uint3(0, 0, 1);

  float3 center_tpos = VoxelCenterDtid2Tpos(dtid);
  float3 right_tpos = VoxelCenterDtid2Tpos(right_dtid);
  float3 top_tpos = VoxelCenterDtid2Tpos(top_dtid);
  float3 far_tpos = VoxelCenterDtid2Tpos(far_dtid);

  // b
  float b_element = gb.SampleLevel(gLinearClamp, center_tpos, 0.0);

  float4 cur_A_element = gA.SampleLevel(gLinearClamp, center_tpos, 0.0);
  float4 right_A_element = gA.SampleLevel(gLinearClamp, right_tpos, 0.0);
  float4 top_A_element = gA.SampleLevel(gLinearClamp, top_tpos, 0.0);
  float4 far_A_element = gA.SampleLevel(gLinearClamp, far_tpos, 0.0);

  // A
  float left_p_coeff = cur_A_element.x;
  float right_p_coeff = right_A_element.x;
  float bottom_p_coff = cur_A_element.y;
  float top_p_coff = top_A_element.y;
  float near_p_coff = cur_A_element.z;
  float far_p_coff = far_A_element.z;
  float cur_p_coff = cur_A_element.w;

  float omiga = 1.0; // w>1 超松弛迭代, w<1 欠松弛迭代, w=1 雅可比迭代
  for (int i = 0; i < 100; ++i) {
    float prev_gp = gp[dtid];

    float left_gp = gp[left_dtid];
    float right_gp = gp[right_dtid];
    float bottom_gp = gp[bottom_dtid];
    float top_gp = gp[top_dtid];
    float near_gp = gp[near_dtid];
    float far_gp = gp[far_dtid];
    float neighbor_pressure_sum = left_p_coeff * left_gp + right_p_coeff * right_gp + 
                                  bottom_p_coff * bottom_gp + top_p_coff * top_gp + 
                                  near_p_coff * near_gp + far_p_coff * far_gp;
                                  
    float p_element = omiga * (b_element - neighbor_pressure_sum) / cur_p_coff + (1.0 - omiga) * prev_gp;
    gp[dtid] = p_element;
    DeviceMemoryBarrierWithGroupSync();
  }
  // 计算误差
  float left = cur_p_coff * gp[dtid] + left_p_coeff * gp[left_dtid] + right_p_coeff * gp[right_dtid] + 
                                  bottom_p_coff * gp[bottom_dtid] + top_p_coff * gp[top_dtid] + 
                                  near_p_coff * gp[near_dtid] + far_p_coff * gp[far_dtid];
  float residual_element = abs(left - b_element);
  gResidual[dtid] = residual_element;
}