#include "common.hlsli"

Texture3D<float> gPrevUGrid : register(t0);
Texture3D<float> gPrevVGrid : register(t1);
Texture3D<float> gPrevWGrid : register(t2);

RWTexture3D<float> gAvgUGrid : register(u0);
RWTexture3D<float> gAvgVGrid : register(u1);
RWTexture3D<float> gAvgWGrid : register(u2);

SamplerState gLinearClamp : register(s0);

[numthreads(8, 8, 4)]
void CSMain(uint3 dtid : SV_DispatchThreadID) {
  float3 left_u_tpos = UDtid2Tpos(dtid);
  float3 right_u_tpos = UDtid2Tpos(dtid + uint3(1, 0, 0));
  float avg_u = (gPrevUGrid.SampleLevel(gLinearClamp, left_u_tpos , 0.0) 
               + gPrevUGrid.SampleLevel(gLinearClamp, right_u_tpos, 0)) * 0.5;

  float3 bottom_v_tpos = VDtid2Tpos(dtid);
  float3 top_v_tpos = VDtid2Tpos(dtid + uint3(0, 1, 0));
  float avg_v = (gPrevVGrid.SampleLevel(gLinearClamp, bottom_v_tpos, 0.0) 
               + gPrevVGrid.SampleLevel(gLinearClamp, top_v_tpos, 0.0)) * 0.5;

  float3 near_w_tpos = WDtid2Tpos(dtid);
  float3 far_w_tpos = WDtid2Tpos(dtid + uint3(0, 0, 1));
  float avg_w = (gPrevWGrid.SampleLevel(gLinearClamp, near_w_tpos, 0.0) 
               + gPrevWGrid.SampleLevel(gLinearClamp, far_w_tpos, 0.0)) * 0.5;

  gAvgUGrid[dtid] = avg_u;
  gAvgVGrid[dtid] = avg_v;
  gAvgWGrid[dtid] = avg_w;
}