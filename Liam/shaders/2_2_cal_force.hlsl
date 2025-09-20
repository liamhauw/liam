#include "common.hlsli"

Texture3D<float> gVorticityXGrid : register(t0);
Texture3D<float> gVorticityYGrid : register(t1);
Texture3D<float> gVorticityZGrid : register(t2);
Texture3D<float> gPrevDensityGrid : register(t3);
Texture3D<float> gPrevTempGrid : register(t4);

RWTexture3D<float> gForceXGrid : register(u0);
RWTexture3D<float> gForceYGrid : register(u1);
RWTexture3D<float> gForceZGrid : register(u2);

SamplerState gLinearClamp : register(s0);

[numthreads(8, 8, 4)]
void CSMain(uint3 dtid : SV_DispatchThreadID) {
  float3 force = float3(10.0, 0.0, 0.0);

  // // 浮力和重力
  // force.y += gAlpha * gPrevDensityGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(dtid), 0)
  //         - gBeta * (gPrevTempGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(dtid), 0) - gAmbientTemp);

  // // 风力
  // force += gWindPower;
  
  // 涡旋力
  uint i = dtid.x;
  uint j = dtid.y;
  uint k = dtid.z;
  
  if (i > 0 && i < gCellCount.x - 1 && j > 0 && j < gCellCount.y - 1 && k > 0 && k < gCellCount.z - 1) {
    // 1. 计算 涡量大小的梯度的x分量
    float3 right_vorticity;
    right_vorticity.x = gVorticityXGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(uint3(i + 1, j, k)), 0.0);
    right_vorticity.y = gVorticityYGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(uint3(i + 1, j, k)), 0.0);
    right_vorticity.z = gVorticityZGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(uint3(i + 1, j, k)), 0.0);
    float3 left_vorticity;
    left_vorticity.x = gVorticityXGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(uint3(i - 1, j, k)), 0.0);
    left_vorticity.y = gVorticityYGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(uint3(i - 1, j, k)), 0.0);
    left_vorticity.z = gVorticityZGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(uint3(i - 1, j, k)), 0.0);
    float grad_vorticity_x = (length(right_vorticity) - length(left_vorticity)) * 0.5 / gCellSize;

    // 2. 计算 涡量大小的梯度的y分量
    float3 top_vorticity;
    top_vorticity.x = gVorticityXGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(uint3(i, j + 1, k)), 0);
    top_vorticity.y = gVorticityYGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(uint3(i, j + 1, k)), 0);
    top_vorticity.z = gVorticityZGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(uint3(i, j + 1, k)), 0);
    float3 bottom_vorticity;
    bottom_vorticity.x = gVorticityXGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(uint3(i, j - 1, k)), 0);
    bottom_vorticity.y = gVorticityYGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(uint3(i, j - 1, k)), 0);
    bottom_vorticity.z = gVorticityZGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(uint3(i, j - 1, k)), 0);
    float grad_vorticity_y = (length(top_vorticity) - length(bottom_vorticity)) * 0.5 / gCellSize;

    // 3. 计算 涡量大小的梯度的z分量
    float3 far_vorticity;
    far_vorticity.x = gVorticityXGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(uint3(i, j, k + 1)), 0);
    far_vorticity.y = gVorticityYGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(uint3(i, j, k + 1)), 0);
    far_vorticity.z = gVorticityZGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(uint3(i, j, k + 1)), 0);
    float3 near_vorticity;
    near_vorticity.x = gVorticityXGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(uint3(i, j, k - 1)), 0);
    near_vorticity.y = gVorticityYGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(uint3(i, j, k - 1)), 0);
    near_vorticity.z = gVorticityZGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(uint3(i, j, k - 1)), 0);
    float grad_vorticity_z = (length(far_vorticity) - length(near_vorticity)) * 0.5 / gCellSize;

    float3 grad_vorticity = float3(grad_vorticity_x, grad_vorticity_y, grad_vorticity_z);

    // 4.计算 N
    float3 N = float3(0.0, 0.0, 0.0);
    float grad_vorticity_length = length(grad_vorticity);
    if (grad_vorticity_length != 0.0) {
      N = grad_vorticity / grad_vorticity_length;
    }

    // 5. 当前位置处的涡量
    float3 cur_vorticity;
    cur_vorticity.x = gVorticityXGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(dtid), 0.0);
    cur_vorticity.y = gVorticityYGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(dtid), 0.0);
    cur_vorticity.z = gVorticityZGrid.SampleLevel(gLinearClamp, VoxelCenterDtid2Tpos(dtid), 0.0);

    // 6. 施加涡旋力
    force += gEpsilon * gCellSize * cross(cur_vorticity, N);
  }


  gForceXGrid[dtid] = force.x;
  gForceYGrid[dtid] = force.y;
  gForceZGrid[dtid] = force.z;
}