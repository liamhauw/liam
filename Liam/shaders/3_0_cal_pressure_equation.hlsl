#include "common.hlsli"

Texture3D<float> gPrevUGrid : register(t0);
Texture3D<float> gPrevVGrid : register(t1);
Texture3D<float> gPrevWGrid : register(t2);

RWTexture3D<float> gb : register(u3);
RWTexture3D<float4> gA : register(u4);

SamplerState gLinearClamp : register(s0);

[numthreads(8, 8, 4)]
void CSMain(uint3 dtid : SV_DispatchThreadID) {
  // 构建 Ap = b
  float3 left_u_tpos = UDtid2Tpos(dtid);
  float3 right_u_tpos = UDtid2Tpos(dtid + uint3(1, 0, 0));
  float left_u = gPrevUGrid.SampleLevel(gLinearClamp, left_u_tpos, 0.0);
  float right_u = gPrevUGrid.SampleLevel(gLinearClamp, right_u_tpos, 0.0);

  float3 bottom_v_tpos = VDtid2Tpos(dtid);
  float3 top_v_tpos = VDtid2Tpos(dtid + uint3(0, 1, 0));
  float bottom_v = gPrevVGrid.SampleLevel(gLinearClamp, bottom_v_tpos, 0.0);
  float top_v = gPrevVGrid.SampleLevel(gLinearClamp, top_v_tpos, 0.0);

  float3 near_w_tpos = WDtid2Tpos(dtid);
  float3 far_w_tpos = WDtid2Tpos(dtid + uint3(0, 0, 1));
  float near_w = gPrevWGrid.SampleLevel(gLinearClamp, near_w_tpos, 0.0);
  float far_w = gPrevWGrid.SampleLevel(gLinearClamp, far_w_tpos, 0.0);

  // 计算b
  gb[dtid] =  -gCellSize / gDt * (right_u - left_u + top_v - bottom_v + far_w - near_w);

  // 计算A
  // w分量存储p(i,j,k)的系数，x分量存储p(i-1,j,k)的系数，
  // y分量存储p(i,j-1,k)的系数，z分量存储p(i,j,k-1)的系数
  float4 A_element = float4(0.0, 0.0, 0.0, 0.0);
  uint i = dtid.x;
  uint j = dtid.y;
  uint k = dtid.z;

  if (i > 0) {
    A_element.w += 1.0;
    A_element.x -= 1.0;
  }
  if (i < gCellCount.x - 1) {
    A_element.w += 1.0;
  }
  if (j > 0) {
    A_element.w += 1.0;
    A_element.y -= 1.0;
  }
  if (j < gCellCount.y - 1) {
    A_element.w += 1.0;
  }
  if (k > 0) {
    A_element.w += 1.0;
    A_element.z -= 1.0;
  }
  if (k < gCellCount.z - 1) {
    A_element.w += 1.0;
  }

  gA[dtid] = A_element;
}