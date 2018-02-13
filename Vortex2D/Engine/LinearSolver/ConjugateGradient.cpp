//
//  ConjugateGradient.cpp
//  Vertex2D
//

#include "ConjugateGradient.h"

#include "vortex2d_generated_spirv.h"

namespace Vortex2D { namespace Fluid {

ConjugateGradient::ConjugateGradient(const Renderer::Device& device,
                                     const glm::ivec2& size,
                                     Preconditioner& preconditioner)
    : mPreconditioner(preconditioner)
    , r(device, size.x*size.y)
    , s(device, size.x*size.y)
    , z(device, size.x*size.y)
    , inner(device, size.x*size.y)
    , alpha(device, 1)
    , beta(device, 1)
    , rho(device, 1)
    , rho_new(device, 1)
    , sigma(device, 1)
    , error(device)
    , matrixMultiply(device, size, MultiplyMatrix_comp)
    , scalarDivision(device, glm::ivec2(1), Divide_comp)
    , scalarMultiply(device, size, Multiply_comp)
    , multiplyAdd(device, size, MultiplyAdd_comp)
    , multiplySub(device, size, MultiplySub_comp)
    , reduceSum(device, size)
    , reduceMax(device, size)
    , reduceMaxBound(reduceMax.Bind(r, error))
    , reduceSumRhoBound(reduceSum.Bind(inner, rho))
    , reduceSumSigmaBound(reduceSum.Bind(inner, sigma))
    , reduceSumRhoNewBound(reduceSum.Bind(inner, rho_new))
    , multiplySBound(scalarMultiply.Bind({z, s, inner}))
    , multiplyZBound(scalarMultiply.Bind({z, r, inner}))
    , divideRhoBound(scalarDivision.Bind({rho, sigma, alpha}))
    , divideRhoNewBound(scalarDivision.Bind({rho_new, rho, beta}))
    , multiplySubRBound(multiplySub.Bind({r, z, alpha, r}))
    , multiplyAddZBound(multiplyAdd.Bind({z, s, beta, s}))
    , mSolveInit(device, false)
    , mSolve(device, false)
    , mErrorRead(device)
{

    mErrorRead.Record([&](vk::CommandBuffer commandBuffer)
    {
        error.Download(commandBuffer);
    });
}

void ConjugateGradient::Init(Renderer::GenericBuffer& d,
                             Renderer::GenericBuffer& l,
                             Renderer::GenericBuffer& b,
                             Renderer::GenericBuffer& pressure)
{
    mPreconditioner.Init(d, l, r, z);

    matrixMultiplyBound = matrixMultiply.Bind({d, l, s, z});
    multiplyAddPBound = multiplyAdd.Bind({pressure, s, alpha, pressure});

    mSolveInit.Record([&](vk::CommandBuffer commandBuffer)
    {
        commandBuffer.debugMarkerBeginEXT({"PCG Init", {{ 0.63f, 0.04f, 0.66f, 1.0f}}});

        // r = b
        r.CopyFrom(commandBuffer, b);

        // calculate error
        reduceMaxBound.Record(commandBuffer);
        error.Barrier(commandBuffer, vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);

        // p = 0
        pressure.Clear(commandBuffer);

        // z = M^-1 r
        z.Clear(commandBuffer);
        mPreconditioner.Record(commandBuffer);
        z.Barrier(commandBuffer, vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);

        // s = z
        s.CopyFrom(commandBuffer, z);

        // rho = zTr
        multiplyZBound.Record(commandBuffer);
        inner.Barrier(commandBuffer, vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);
        reduceSumRhoBound.Record(commandBuffer);
        rho.Barrier(commandBuffer, vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);

        commandBuffer.debugMarkerEndEXT();
    });

    mSolve.Record([&](vk::CommandBuffer commandBuffer)
    {
        commandBuffer.debugMarkerBeginEXT({"PCG Step", {{ 0.51f, 0.90f, 0.72f, 1.0f}}});

        // z = As
        matrixMultiplyBound.Record(commandBuffer);
        z.Barrier(commandBuffer, vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);

        // sigma = zTs
        multiplySBound.Record(commandBuffer);
        inner.Barrier(commandBuffer, vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);
        reduceSumSigmaBound.Record(commandBuffer);
        sigma.Barrier(commandBuffer, vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);

        // alpha = rho / sigma
        divideRhoBound.Record(commandBuffer);
        alpha.Barrier(commandBuffer, vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);

        // p = p + alpha * s
        multiplyAddPBound.Record(commandBuffer);
        pressure.Barrier(commandBuffer, vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);

        // r = r - alpha * z
        multiplySubRBound.Record(commandBuffer);
        r.Barrier(commandBuffer, vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);

        // calculate max error
        reduceMaxBound.Record(commandBuffer);
        error.Barrier(commandBuffer, vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);

        // z = M^-1 r
        z.Clear(commandBuffer);
        mPreconditioner.Record(commandBuffer);
        z.Barrier(commandBuffer, vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);

        // rho_new = zTr
        multiplyZBound.Record(commandBuffer);
        inner.Barrier(commandBuffer, vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);
        reduceSumRhoNewBound.Record(commandBuffer);
        rho_new.Barrier(commandBuffer, vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);

        // beta = rho_new / rho
        divideRhoNewBound.Record(commandBuffer);
        beta.Barrier(commandBuffer, vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);

        // s = z + beta * s
        multiplyAddZBound.Record(commandBuffer);
        s.Barrier(commandBuffer, vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);

        // rho = rho_new
        rho.CopyFrom(commandBuffer, rho_new);

        commandBuffer.debugMarkerEndEXT();
    });
}

void ConjugateGradient::Solve(Parameters& params)
{
    mSolveInit.Submit();
    mErrorRead.Submit();

    for (unsigned i = 0;; ++i)
    {
        // exit condition
        mErrorRead.Wait();

        params.OutIterations = i;
        Renderer::CopyTo(error, params.OutError);
        // TODO should divide by the initial error
        if (params.IsFinished(i, params.OutError))
        {
            return;
        }

        mErrorRead.Submit();
        mSolve.Submit();
    }
}

}}
