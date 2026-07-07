#include "GPUTimer.h"

GPUTimer::GPUTimer()
{
}

GPUTimer::~GPUTimer()
{
    Clear();
}

void GPUTimer::Create(const VulkanContext& ctx)
{
    this->ctx = &ctx;
    timestampPeriod = ctx.GetPhysicalDeviceProp().limits.timestampPeriod;

    VkQueryPoolCreateInfo ci{};
    ci.sType = VkStructureType::VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    ci.queryType = VkQueryType::VK_QUERY_TYPE_TIMESTAMP;
    ci.queryCount = 2;
    vkCreateQueryPool(ctx.GetDevice(), &ci, nullptr, &queryPool);
}

void GPUTimer::Clear()
{
    if (queryPool == VK_NULL_HANDLE)
        return;
    vkDestroyQueryPool(ctx->GetDevice(), queryPool, nullptr);
    queryPool = VK_NULL_HANDLE;
}

void GPUTimer::Begin(VkCommandBuffer cmd)
{
    vkCmdResetQueryPool(cmd, queryPool, 0, 2);
    vkCmdWriteTimestamp(cmd, VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPool, 0);
}

void GPUTimer::End(VkCommandBuffer cmd)
{
    vkCmdWriteTimestamp(cmd, VkPipelineStageFlagBits::VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool, 1);
}

auto GPUTimer::GetElapsedMs() const -> double
{
    uint64_t timestamps[2]{};

    VkResult result = vkGetQueryPoolResults(
        ctx->GetDevice(), 
        queryPool,
        0,
        2,
        sizeof(timestamps),
        timestamps,
        sizeof(uint64_t),
        VkQueryResultFlagBits::VK_QUERY_RESULT_64_BIT | VkQueryResultFlagBits::VK_QUERY_RESULT_WAIT_BIT
    );

    if (result != VK_SUCCESS)
        return 0.0;

    double elapsedNs = static_cast<double>(timestamps[1] - timestamps[0]) * timestampPeriod;
    return elapsedNs / 1'000'000.0;
}
