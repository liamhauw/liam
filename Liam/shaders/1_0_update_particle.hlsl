#include "common.hlsli"

RWStructuredBuffer<Particle>  gParticle : register(u5);

[numthreads(128, 1, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID) {
  uint index = dtid.x;
  if (gParticle[index].gMass > 0.01) {
    gParticle[index].gPosition += gParticle[index].gVelocity * gDt;
    if (index % 350 == 0) {
      // 初始化粒子位置
      gParticle[index].gMass *= gAtteMass * 1.02;
      gParticle[index].gVelocity *= gAtteVel * 1.06;
    } else {
      // 初始化粒子质量和速度
      gParticle[index].gMass *= gAtteMass;
      gParticle[index].gVelocity *= gAtteVel; 
    }
  }
}