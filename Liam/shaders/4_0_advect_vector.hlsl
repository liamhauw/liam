#include "common.hlsli"


Texture3D<float> gPrevUGrid : register(t0);
Texture3D<float> gPrevVGrid : register(t1);
Texture3D<float> gPrevWGrid : register(t2);

RWTexture3D<float> gCurUGrid : register(u0);
RWTexture3D<float> gCurVGrid : register(u1);
RWTexture3D<float> gCurWGrid : register(u2);

SamplerState gLinearClamp : register(s0);

float3 GetVelocity(float3 mpos) {
  float3 u_tpos = UMpos2Tpos(mpos);
  float3 v_tpos = VMpos2Tpos(mpos);
  float3 w_tpos = WMpos2Tpos(mpos);
  float3 velocity;
  velocity.x = gPrevUGrid.SampleLevel(gLinearClamp, u_tpos, 0.0);
  velocity.y = gPrevVGrid.SampleLevel(gLinearClamp, v_tpos, 0.0);
  velocity.z = gPrevWGrid.SampleLevel(gLinearClamp, w_tpos, 0.0);
  return velocity;
}

void AdvectU(uint3 dtid) {
  float3 q_mpos = UDtid2Mpos(dtid);
  float3 q_velocity = GetVelocity(q_mpos);

  float3 p_mpos = float3(0.0, 0.0, 0.0);
#if (ADVECT_METHOD == 1)
  p_mpos = q_mpos - gDt * q_velocity;
#endif

#if (ADVECT_METHOD == 2)
  float3 mid_velocity = GetVelocity(q_mpos - 0.5 * gDt * q_velocity);
  p_mpos = q_mpos - gDt * mid_velocity;
#endif

#if (ADVECT_METHOD == 3)
  float3 velocity1 = GetVelocity(q_mpos - 0.5 * gDt * q_velocity);
  float3 velocity2 = GetVelocity(q_mpos - 0.75 * gDt * velocity1);
  p_mpos = q_mpos - gDt * (2.0 * q_velocity + 3.0 * velocity1 + 4.0 * velocity2) / 9.0;
#endif

#if (ADVECT_METHOD == 4)
  float3 mpos_1 = q_mpos - gDt * q_velocity;
  float3 mpos_2 = mpos_1 + gDt * GetVelocity(mpos_1);
  float3 mpos_e = 0.5 * (mpos_2 - q_mpos);
  p_mpos = mpos_1 + mpos_e;
#endif

  float3 p_tpos = UMpos2Tpos(p_mpos);
  gCurUGrid[dtid] = gPrevUGrid.SampleLevel(gLinearClamp, p_tpos, 0.0);
}

void AdvectV(uint3 dtid) {
  float3 q_mpos = VDtid2Mpos(dtid);
  float3 q_velocity = GetVelocity(q_mpos);

  float3 p_mpos = float3(0.0, 0.0, 0.0);
#if (ADVECT_METHOD == 1)
  p_mpos = q_mpos - gDt * q_velocity;
#endif

#if (ADVECT_METHOD == 2)
  float3 mid_velocity = GetVelocity(q_mpos - 0.5 * gDt * q_velocity);
  p_mpos = q_mpos - gDt * mid_velocity;
#endif

#if (ADVECT_METHOD == 3)
  float3 velocity1 = GetVelocity(q_mpos - 0.5 * gDt * q_velocity);
  float3 velocity2 = GetVelocity(q_mpos - 0.75 * gDt * velocity1);
  p_mpos = q_mpos - gDt * (2.0 * q_velocity + 3.0 * velocity1 + 4.0 * velocity2) / 9.0;
#endif

#if (ADVECT_METHOD == 4)
  float3 mpos_1 = q_mpos - gDt * q_velocity;
  float3 mpos_2 = mpos_1 + gDt * GetVelocity(mpos_1);
  float3 mpos_e = 0.5 * (mpos_2 - q_mpos);
  p_mpos = mpos_1 + mpos_e;
#endif

  float3 p_tpos = VMpos2Tpos(p_mpos);
  gCurVGrid[dtid] = gPrevVGrid.SampleLevel(gLinearClamp, p_tpos, 0.0);
}

void AdvectW(uint3 dtid) {
  float3 q_mpos = WDtid2Mpos(dtid);
  float3 q_velocity = GetVelocity(q_mpos);

  float3 p_mpos = float3(0.0, 0.0, 0.0);
#if (ADVECT_METHOD == 1)
  p_mpos = q_mpos - gDt * q_velocity;
#endif

#if (ADVECT_METHOD == 2)
  float3 mid_velocity = GetVelocity(q_mpos - 0.5 * gDt * q_velocity);
  p_mpos = q_mpos - gDt * mid_velocity;
#endif

#if (ADVECT_METHOD == 3)
  float3 velocity1 = GetVelocity(q_mpos - 0.5 * gDt * q_velocity);
  float3 velocity2 = GetVelocity(q_mpos - 0.75 * gDt * velocity1);
  p_mpos = q_mpos - gDt * (2.0 * q_velocity + 3.0 * velocity1 + 4.0 * velocity2) / 9.0;
#endif

#if (ADVECT_METHOD == 4)
  float3 mpos_1 = q_mpos - gDt * q_velocity;
  float3 mpos_2 = mpos_1 + gDt * GetVelocity(mpos_1);
  float3 mpos_e = 0.5 * (mpos_2 - q_mpos);
  p_mpos = mpos_1 + mpos_e;
#endif

  float3 p_tpos = WMpos2Tpos(p_mpos);
  gCurWGrid[dtid] = gPrevWGrid.SampleLevel(gLinearClamp, p_tpos, 0.0);
}

[numthreads(8, 8, 4)]
void CSMain(uint3 dtid : SV_DispatchThreadID) {
  AdvectU(dtid);
  AdvectV(dtid);
  AdvectW(dtid);

  if (dtid.x == gCellCount.x - 1) {
    AdvectU(dtid + uint3(1, 0, 0));
  }
  if (dtid.y == gCellCount.y - 1) {
    AdvectV(dtid + uint3(0, 1, 0));
  }
  if (dtid.z == gCellCount.z - 1) {
    AdvectW(dtid + uint3(0, 0, 1));
  }
}