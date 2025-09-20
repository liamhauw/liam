#include "common.hlsli"

StructuredBuffer<Particle> gParticle : register(t2);

struct VSInput {
  float4 gColor : COLOR;
  uint gId : SV_VertexID;
};

struct VSOutput {
  float3 gPos : POSITION;
  float gMass : TEXCOORD;
  float4 gColor : COLOR;
};

struct GSOutput {
  float4 gColor : COLOR;
  float4 gPos : SV_Position;
};

struct PSInput {
  float4 gColor : COLOR;
};

VSOutput VSMain(VSInput vs_input) {
  VSOutput vs_output;
  vs_output.gPos = gParticle[vs_input.gId].gPosition;
  vs_output.gMass = gParticle[vs_input.gId].gMass;
  vs_output.gColor = vs_input.gColor;
  return vs_output;
}

static float3 del_pos[4] = {
  float3(-0.5, 0.5, 0),
  float3(0.5, 0.5, 0),
  float3(-0.5, -0.5, 0),
  float3(0.5, -0.5, 0),
};

[maxvertexcount(4)]
void GSMain(point VSOutput gs_input[1], inout TriangleStream<GSOutput> sprite_stream) {
  GSOutput gs_output;
  float mass = gs_input[0].gMass;
  float max_mass = 1.5;
  float coe = mass / max_mass;

  for (int i = 0; i < 4; ++i) {
    float3 pos = gs_input[0].gPos + del_pos[i] * coe;
    gs_output.gPos = mul(float4(pos, 1.0), gMvp);
    gs_output.gColor = gs_input[0].gColor;
    sprite_stream.Append(gs_output);
  }
  sprite_stream.RestartStrip();
}

float4 PSMain(PSInput ps_input) : SV_Target {
  // return ps_input.gColor;
  return float4(1.0, 0.0, 0.0, 1.0);
}
