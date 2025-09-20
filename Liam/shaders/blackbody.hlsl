#include "common.hlsli"

struct VSInput{
  float3 gModelPos : POSITION;
};

struct VSOutput{
  float4 gClipPos : SV_POSITION;
};

VSOutput VSMain(VSInput vs_input) {
  VSOutput vs_output;
  vs_output.gClipPos = mul(float4(vs_input.gModelPos, 1.0f), gMvp);
  return vs_output;
}

float4 PSMain(VSOutput ps_input) : SV_Target {
  float lbb = 500;
  
  float r = lbb / 1000;

  float3 xyz = float3(r, r, r);
  return float4(xyz, 1.0f);
}