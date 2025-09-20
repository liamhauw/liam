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
  return float4(0.5f, 0.5f, 0.5f, 1.0f);
}