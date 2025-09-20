#include "common.hlsli"

struct Weight {
  float w000;
  float w100;
  float w010;
  float w110;
  float w001;
  float w101;
  float w011;
  float w111;
};

StructuredBuffer<Particle>  gParticle : register(t5);

RWTexture3D<float> gCurUGrid : register(u0);
RWTexture3D<float> gCurVGrid : register(u1);
RWTexture3D<float> gCurWGrid : register(u2);
RWTexture3D<float> gCurDensityGrid : register(u3);
RWTexture3D<float> gCurTempGrid : register(u4);

SamplerState gLinearClamp : register(s0);

void CalWeight(float3 len, out Weight w) {
  float dx0 = len.x / gCellSize;
  float dy0 = len.y / gCellSize;
  float dz0 = len.z / gCellSize;

  float dx1 = 1.0 - dx0;
  float dy1 = 1.0 - dy0;
  float dz1 = 1.0 - dz0;

  w.w000 = dx1 * dy1 * dz1;
  w.w100 = dx0 * dy1 * dz1;
  w.w010 = dx1 * dy0 * dz1;
  w.w110 = dx0 * dy0 * dz1;
  w.w001 = dx1 * dy1 * dz0;
  w.w101 = dx0 * dy1 * dz0;
  w.w011 = dx1 * dy0 * dz0;
  w.w111 = dx0 * dy0 * dz0;
}

void UpdateU(float3 mpos, float produce) {
  uint3 base_dtid = uint3((mpos - gGridLbnPos - float3(0.0, 0.5, 0.5) * gCellSize) / gCellSize);
  float3 dist = mpos - UDtid2Mpos(base_dtid);
  Weight w;
  CalWeight(dist, w);

  gCurUGrid[base_dtid + uint3(0, 0, 0)] += produce * w.w000;
  gCurUGrid[base_dtid + uint3(1, 0, 0)] += produce * w.w100;
  gCurUGrid[base_dtid + uint3(0, 1, 0)] += produce * w.w010;
  gCurUGrid[base_dtid + uint3(1, 1, 0)] += produce * w.w110;
  gCurUGrid[base_dtid + uint3(0, 0, 1)] += produce * w.w001;
  gCurUGrid[base_dtid + uint3(1, 0, 1)] += produce * w.w101;
  gCurUGrid[base_dtid + uint3(0, 1, 1)] += produce * w.w011;
  gCurUGrid[base_dtid + uint3(1, 1, 1)] += produce * w.w111;
}

void UpdateV(float3 mpos, float produce) {
  uint3 base_dtid = uint3((mpos - gGridLbnPos - float3(0.5, 0.0, 0.5) * gCellSize) / gCellSize);
  float3 dist = mpos - VDtid2Mpos(base_dtid);
  Weight w;
  CalWeight(dist, w);

  gCurVGrid[base_dtid + uint3(0, 0, 0)] += produce * w.w000;
  gCurVGrid[base_dtid + uint3(1, 0, 0)] += produce * w.w100;
  gCurVGrid[base_dtid + uint3(0, 1, 0)] += produce * w.w010;
  gCurVGrid[base_dtid + uint3(1, 1, 0)] += produce * w.w110;
  gCurVGrid[base_dtid + uint3(0, 0, 1)] += produce * w.w001;
  gCurVGrid[base_dtid + uint3(1, 0, 1)] += produce * w.w101;
  gCurVGrid[base_dtid + uint3(0, 1, 1)] += produce * w.w011;
  gCurVGrid[base_dtid + uint3(1, 1, 1)] += produce * w.w111;
}

void UpdateW(float3 mpos, float produce) {
  uint3 base_dtid = uint3((mpos - gGridLbnPos - float3(0.5, 0.5, 0.0) * gCellSize) / gCellSize);
  float3 dist = mpos - WDtid2Mpos(base_dtid);
  Weight w;
  CalWeight(dist, w);

  gCurWGrid[base_dtid + uint3(0, 0, 0)] += produce * w.w000;
  gCurWGrid[base_dtid + uint3(1, 0, 0)] += produce * w.w100;
  gCurWGrid[base_dtid + uint3(0, 1, 0)] += produce * w.w010;
  gCurWGrid[base_dtid + uint3(1, 1, 0)] += produce * w.w110;
  gCurWGrid[base_dtid + uint3(0, 0, 1)] += produce * w.w001;
  gCurWGrid[base_dtid + uint3(1, 0, 1)] += produce * w.w101;
  gCurWGrid[base_dtid + uint3(0, 1, 1)] += produce * w.w011;
  gCurWGrid[base_dtid + uint3(1, 1, 1)] += produce * w.w111;
}

void UpdateDensityAndTemp(float3 mpos, float produce_density, float produce_temp) {

  uint3 base_dtid = uint3((mpos - gGridLbnPos - float3(0.5, 0.5, 0.5) * gCellSize) / gCellSize);;
  float3 dist = mpos - VoxelCenterDtid2Mpos(base_dtid);

  Weight w;
  CalWeight(dist, w);

  gCurDensityGrid[base_dtid + uint3(0, 0, 0)] += produce_density * w.w000;
  gCurDensityGrid[base_dtid + uint3(1, 0, 0)] += produce_density * w.w100;
  gCurDensityGrid[base_dtid + uint3(0, 1, 0)] += produce_density * w.w010;
  gCurDensityGrid[base_dtid + uint3(1, 1, 0)] += produce_density * w.w110;
  gCurDensityGrid[base_dtid + uint3(0, 0, 1)] += produce_density * w.w001;
  gCurDensityGrid[base_dtid + uint3(1, 0, 1)] += produce_density * w.w101;
  gCurDensityGrid[base_dtid + uint3(0, 1, 1)] += produce_density * w.w011;
  gCurDensityGrid[base_dtid + uint3(1, 1, 1)] += produce_density * w.w111;

  gCurTempGrid[base_dtid + uint3(0, 0, 0)] += produce_temp * w.w000;
  gCurTempGrid[base_dtid + uint3(1, 0, 0)] += produce_temp * w.w100;
  gCurTempGrid[base_dtid + uint3(0, 1, 0)] += produce_temp * w.w010;
  gCurTempGrid[base_dtid + uint3(1, 1, 0)] += produce_temp * w.w110;
  gCurTempGrid[base_dtid + uint3(0, 0, 1)] += produce_temp * w.w001;
  gCurTempGrid[base_dtid + uint3(1, 0, 1)] += produce_temp * w.w101;
  gCurTempGrid[base_dtid + uint3(0, 1, 1)] += produce_temp * w.w011;
  gCurTempGrid[base_dtid + uint3(1, 1, 1)] += produce_temp * w.w111;
}

[numthreads(128, 1, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID) {
  uint index = dtid.x;
  float mass = gParticle[index].gMass;
  float3 mpos = gParticle[index].gPosition;
  
  float3 min_boundary = gGridLbnPos + float3(gCellSize, gCellSize, gCellSize);
  float3 max_boundary = gGridRtfPos - float3(gCellSize, gCellSize, gCellSize);

  // 粒子质量非常小时忽略其对网格的影响
  if (mass > 0.01 && all(mpos > min_boundary) && all(mpos < max_boundary)) {
    float3 velocity = gParticle[index].gVelocity;

    // 单个粒子产生烟幕的速度、密度和温度
    float3 produce_velocity = 3.0 * velocity;
    float produce_density = gEmDens * mass;
    float produce_temp = gEmtemp * mass;
    
    // 更新网格数据
    UpdateU(mpos, produce_velocity.x);
    UpdateV(mpos, produce_velocity.y);
    UpdateW(mpos, produce_velocity.z);
    UpdateDensityAndTemp(mpos, produce_density, produce_temp);
  }

}