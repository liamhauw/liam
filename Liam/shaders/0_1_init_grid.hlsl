#include "common.hlsli"

RWTexture3D<float> gCurTempGrid : register(u5);

[numthreads(8, 8, 4)]
void CSMain(uint3 dtid : SV_DispatchThreadID) {
  // 初始化环境温度
  gCurTempGrid[dtid] = UniformPRN0(gAmbientTemp - 10.0f, gAmbientTemp + 10.0f, dtid);
}