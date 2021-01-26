#pragma once

#include "gapi/Result.hpp"

#include <queue>

namespace OpenDemo
{
    namespace GAPI
    {
        namespace DX12
        {
            class ResourceImpl;

            class GpuMemoryHeap
            {
            public:
                struct Allocation
                {
                    size_t size = 0;
                    size_t offset = 0;
                    uint64_t fenceValue = 0;
                    ComSharedPtr<ID3D12Resource> resource;
                };

                struct Page
                {
                    Page() = delete;
                    ~Page();
                    Page(size_t size, std::unique_ptr<ResourceImpl>&& resource) : size(size), resource(std::move(resource)) {};

                    size_t size;
                    size_t offset = 0;
                    std::unique_ptr<ResourceImpl> resource;
                };

            public:
                GpuMemoryHeap(size_t pageSize) : defaultPageSize_(pageSize) {};

                Result Init(const U8String& name);
                Result Allocate(Allocation& allocation, size_t size, size_t alignment = 1);

            private:
                Result getNextPageForAllocation(size_t allocSize, std::unique_ptr<Page>& page);

            private:
                size_t defaultPageSize_;
                U8String name_ = "";
                uint32_t pageIndex = 0;
                std::unique_ptr<Page> currentPage_;
                std::queue<std::unique_ptr<Page>> freePages_;
                std::vector<std::unique_ptr<Page>> usedPages_;
            };
        }
    }
}