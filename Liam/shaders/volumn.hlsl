#include "common.hlsli"

Texture3D<float> gPrevDensityGrid : register(t0);
Texture3D<float> gPrevTempGrid : register(t1);
Texture3D<float> gInputRadianceGrid : register(t2);

SamplerState gLinearClamp : register(s0);

struct VSInput {
  float3 gModelPos : POSITION;
};

struct VSOutput {
  float4 gClipPos : SV_POSITION;
  float3 gModelPos : POSITION;
};

VSOutput VSMain(VSInput vs_input) {
  VSOutput vs_output;
  vs_output.gClipPos = mul(float4(vs_input.gModelPos, 1.0f), gMvp);
  vs_output.gModelPos = vs_input.gModelPos;
  return vs_output;
};


float4 PSMain(VSOutput ps_input) : SV_Target {
  float3 cur_mpos = ps_input.gModelPos;
  float3 view_mpos = mul(gViewPos, gInvModel).xyz;
  float3 light_mpos = mul(float4(gLightPos, 1.0), gInvModel).xyz;

  float3 view_dir = normalize(cur_mpos - view_mpos);
  float step_length = gCellSize;
  float3 step_vec = step_length * view_dir;
  int step_times = ceil(length(gCellCount));

  float trans = 1.0;
  float radiance = 0.0;

  float ext;
  if (gWaveBand == 0) {
    ext = gExt.x;
  } else if (gWaveBand == 1) {
    ext = gExt.y;
  } else {
    ext = gExt.z;
  }
  
  float x = step_length * 1e-2;
  float ds = x * x;

  for (int i = 0; i < step_times; ++i) {
    if (trans <= 0.01) {
      break;
    }
    float3 cur_tpos = VoxelCenterMpos2Tpos(cur_mpos);
    float cur_density = gPrevDensityGrid.SampleLevel(gLinearClamp, cur_tpos, 0.0);


    // 可见光
    if (gWaveBand == 0) {
      cur_density *= 0.1;
    }

    float cur_temp = gPrevTempGrid.SampleLevel(gLinearClamp, cur_tpos, 0.0);

    // 光源辐射，光源辐射等于入射辐射乘以散射率，cur_density单位为g/m^3，x单位为m，ds单位为m^2
    float costheta = dot(normalize(view_mpos - cur_mpos), normalize(light_mpos - cur_mpos));
    float scattering_ratio = cur_density * (1.0 + pow(costheta, 2)) * x / ds * 1.3e-4;
    if (gWaveBand == 0) {
      scattering_ratio *= 10.0;
    }
    float light_radiance = scattering_ratio * gInputRadianceGrid.SampleLevel(gLinearClamp, cur_tpos, 0.0);

    // 自发辐射
    float self_radiance = Lbb(cur_temp);

    // 更新辐射亮度
    radiance += (light_radiance + self_radiance) * trans;

    // 更新透过率
    float ex = exp(-ext * cur_density * x) + f0(cur_density);
    trans *= ex;

    cur_mpos += step_vec;
  }

  float w = 1.0 - trans;
  float r = radiance / 1000.0;

  if (gWaveBand == 0) {
    return float4(0.5451 * r, 0.47451 * r, 0.36863 * r, w);  // 可见光
  } else {
    float3 xyz = float3(r, r, r);
    return float4(xyz, w); // 红外
  }
}