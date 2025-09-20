#include "common.hlsli"

Texture3D<float> gPrevDensityGrid : register(t5);

RWTexture3D<float> gInputRadianceGrid : register(u5);

SamplerState gLinearClamp : register(s0);

bool Outside(float3 mpos) {
  float xlimit = -gGridLbnPos.x;
  float ylimit = -gGridLbnPos.y;
  float zlimit = -gGridLbnPos.z;
  return (abs(mpos.x) > xlimit || abs(mpos.y) > ylimit || abs(mpos.z) > zlimit);
}

[numthreads(8, 8, 4)]
void CSMain(uint3 dtid : SV_DispatchThreadID) {
  float3 cur_mpos = VoxelCenterDtid2Mpos(dtid);
  float3 light_mpos = mul(float4(gLightPos, 1.0), gInvModel).xyz;
  float3 light_dir = normalize(light_mpos - cur_mpos);
  float step_length = gCellSize;
  float3 step_vec = step_length * light_dir;
  int step_times = ceil(length(gCellCount));

  float x = step_length * 1e-2;
  
  float ext;
  if (gWaveBand == 0) {
    ext = gExt.x;
  } else if (gWaveBand == 1) {
    ext = gExt.y;
  } else {
    ext = gExt.z;
  }

  float trans = 1.0;
  for (int i = 0; i < step_times; ++i) {
    cur_mpos += step_vec;
    if (Outside(cur_mpos) || trans <= 0.01) {
      break;
    }
    float3 cur_tpos = VoxelCenterMpos2Tpos(cur_mpos);
    float cur_density = gPrevDensityGrid.SampleLevel(gLinearClamp, cur_tpos, 0.0);

    float cur_trans = exp(-ext * cur_density * x) + f0(cur_density);
    trans *= cur_trans;
  }

  float light_radiance;
  if (gWaveBand == 0) {
    light_radiance = 200.0;
  } else {
    light_radiance = gLightRadiance;
  }

  float input_radiance = light_radiance * trans;
  gInputRadianceGrid[dtid] = input_radiance;
}