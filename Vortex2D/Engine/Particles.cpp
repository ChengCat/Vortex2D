//
//  Particles.cpp
//  Vortex
//

#include "Particles.h"

#include <Vortex2D/Engine/LevelSet.h>

#include <random>
#include "vortex2d_generated_spirv.h"

namespace Vortex2D { namespace Fluid {

ParticleCount::ParticleCount(const Renderer::Device& device,
                     const glm::ivec2& size,
                     Renderer::GenericBuffer& particles,
                     const Renderer::DispatchParams& params)
    : Renderer::RenderTexture(device, size.x, size.y, vk::Format::eR32Sint)
    , mDevice(device)
    , mParticles(particles)
    , mNewParticles(device, 8*size.x*size.y)
    , mCount(device, size.x*size.y)
    , mIndex(device, size.x*size.y)
    , mSeeds(device, 4)
    , mDispatchParams(device)
    , mLocalDispatchParams(device, 1, true)
    , mNewDispatchParams(device)
    , mParticleCountWork(device, Renderer::ComputeSize::Default1D(), ParticleCount_comp)
    , mParticleCountBound(mParticleCountWork.Bind({particles, mDispatchParams, *this}))
    , mPrefixScan(device, size)
    , mPrefixScanBound(mPrefixScan.Bind(mCount, mIndex, mNewDispatchParams))
    , mParticleBucketWork(device, Renderer::ComputeSize::Default1D(), ParticleBucket_comp)
    , mParticleBucketBound(mParticleBucketWork.Bind(size, {particles,
                                                           mNewParticles,
                                                           mIndex,
                                                           mCount,
                                                           mDispatchParams}))
    , mParticleSpawnWork(device, size, ParticleSpawn_comp)
    , mParticleSpawnBound(mParticleSpawnWork.Bind({mNewParticles, mIndex, mCount, mSeeds}))
    , mParticlePhiWork(device, size, ParticlePhi_comp)
    , mParticleToGridWork(device, size, ParticleToGrid_comp)
    , mParticleFromGridWork(device, Renderer::ComputeSize::Default1D(), ParticleFromGrid_comp)
    , mCountWork(device, false)
    , mScanWork(device, false)
    , mDispatchCountWork(device)
    , mParticlePhi(device, false)
    , mParticleToGrid(device, false)
    , mParticleFromGrid(device, false)
{
    Renderer::CopyFrom(mLocalDispatchParams, params);
    Renderer::ExecuteCommand(device, [&](vk::CommandBuffer commandBuffer)
    {
        mDispatchParams.CopyFrom(commandBuffer, mLocalDispatchParams);
    });

    // TODO should limit to 4 (or 8) particles
    mCountWork.Record([&](vk::CommandBuffer commandBuffer)
    {
        commandBuffer.debugMarkerBeginEXT({"Particle count", {{ 0.14f, 0.39f, 0.12f, 1.0f}}});
        Clear(commandBuffer, std::array<int, 4>{0, 0, 0, 0});
        mParticleCountBound.RecordIndirect(commandBuffer, mDispatchParams);
        Barrier(commandBuffer,
                vk::ImageLayout::eGeneral, vk::AccessFlagBits::eShaderWrite,
                vk::ImageLayout::eGeneral, vk::AccessFlagBits::eShaderRead);
        commandBuffer.debugMarkerEndEXT();
    });

    mScanWork.Record([&](vk::CommandBuffer commandBuffer)
    {
        commandBuffer.debugMarkerBeginEXT({"Particle scan", {{ 0.59f, 0.20f, 0.35f, 1.0f}}});
        mCount.CopyFrom(commandBuffer, *this);
        mPrefixScanBound.Record(commandBuffer);
        mParticleBucketBound.RecordIndirect(commandBuffer, mDispatchParams);
        mNewParticles.Barrier(commandBuffer, vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);
        mSeeds.Upload(commandBuffer);
        mParticleSpawnBound.Record(commandBuffer);
        mNewParticles.Barrier(commandBuffer, vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);
        particles.CopyFrom(commandBuffer, mNewParticles);
        mDispatchParams.CopyFrom(commandBuffer, mNewDispatchParams);
        commandBuffer.debugMarkerEndEXT();
    });

    mDispatchCountWork.Record([&](vk::CommandBuffer commandBuffer)
    {
        mLocalDispatchParams.CopyFrom(commandBuffer, mDispatchParams);
    });
}

void ParticleCount::Count()
{
    mCountWork.Submit();
}

void ParticleCount::Scan()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis;

    std::vector<glm::ivec2> seeds = {{dis(gen), dis(gen)},
                                     {dis(gen), dis(gen)},
                                     {dis(gen), dis(gen)},
                                     {dis(gen), dis(gen)}};

    Renderer::CopyFrom(mSeeds, seeds);
    mScanWork.Submit();
}

int ParticleCount::GetCount()
{
    mDispatchCountWork.Submit();
    mDispatchCountWork.Wait();

    Renderer::DispatchParams params(0);
    Renderer::CopyTo(mLocalDispatchParams, params);
    return params.count;
}

Renderer::GenericBuffer& ParticleCount::GetDispatchParams()
{
    return mDispatchParams;
}

void ParticleCount::InitLevelSet(LevelSet& levelSet)
{
    // TODO should shrink wrap wholes and redistance
    mParticlePhiBound = mParticlePhiWork.Bind({*this, mParticles, mIndex, levelSet});
    mParticlePhi.Record([&](vk::CommandBuffer commandBuffer)
    {
        commandBuffer.debugMarkerBeginEXT({"Particle phi", {{ 0.86f, 0.72f, 0.29f, 1.0f}}});
        levelSet.Clear(commandBuffer, std::array<float, 4>{3.0f, 0.0f, 0.0f, 0.0f});
        mParticlePhiBound.Record(commandBuffer);
        levelSet.Barrier(commandBuffer,
                         vk::ImageLayout::eGeneral, vk::AccessFlagBits::eShaderWrite,
                         vk::ImageLayout::eGeneral, vk::AccessFlagBits::eShaderRead);
        commandBuffer.debugMarkerEndEXT();
    });
}

void ParticleCount::Phi()
{
    mParticlePhi.Submit();
}

void ParticleCount::InitVelocities(Renderer::Texture& velocity, Renderer::GenericBuffer& valid)
{
    mParticleToGridBound = mParticleToGridWork.Bind({*this, mParticles, mIndex, velocity, valid});
    mParticleToGrid.Record([&](vk::CommandBuffer commandBuffer)
    {
        commandBuffer.debugMarkerBeginEXT({"Particle to grid", {{ 0.71f, 0.15f, 0.48f, 1.0f}}});
        valid.Clear(commandBuffer);
        mParticleToGridBound.Record(commandBuffer);
        velocity.Barrier(commandBuffer,
                         vk::ImageLayout::eGeneral, vk::AccessFlagBits::eShaderWrite,
                         vk::ImageLayout::eGeneral, vk::AccessFlagBits::eShaderRead);
        commandBuffer.debugMarkerEndEXT();
    });

    mParticleFromGridBound = mParticleFromGridWork.Bind({mParticles, mDispatchParams, velocity});
    mParticleFromGrid.Record([&](vk::CommandBuffer commandBuffer)
    {
        commandBuffer.debugMarkerBeginEXT({"Particle from grid", {{ 0.35f, 0.11f, 0.87f, 1.0f}}});
        mParticleFromGridBound.RecordIndirect(commandBuffer, mDispatchParams);
        mParticles.Barrier(commandBuffer, vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);
        commandBuffer.debugMarkerEndEXT();
    });
}

void ParticleCount::TransferToGrid()
{
    mParticleToGrid.Submit();
}

void ParticleCount::TransferFromGrid()
{
    mParticleFromGrid.Submit();
}

}}
