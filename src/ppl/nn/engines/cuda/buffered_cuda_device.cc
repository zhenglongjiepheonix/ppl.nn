// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include "ppl/nn/engines/cuda/buffered_cuda_device.h"
#include "ppl/nn/engines/cuda/buffered_cuda_allocator.h"
#include "ppl/nn/engines/cuda/graph_cuda_allocator.h"
#include "ppl/nn/engines/cuda/plain_cuda_allocator.h"
#include "ppl/nn/utils/stack_buffer_manager.h"
#include "ppl/nn/utils/compact_buffer_manager.h"
#include "ppl/nn/common/logger.h"

using namespace std;
using namespace ppl::common;

namespace ppl { namespace nn { namespace cuda {

#define DEFAULT_BLOCK_SIZE 1048576

RetCode BufferedCudaDevice::Init(int device_id, uint32_t mm_policy, bool enable_cuda_graph) {
    auto status = CudaDevice::Init(device_id, enable_cuda_graph);
    if (status != RC_SUCCESS) {
        LOG(ERROR) << "init cuda device failed: " << GetRetCodeStr(status);
        return status;
    }

    if (mm_policy == MM_BEST_FIT) {
        if (enable_cuda_graph) {
            allocator_.reset(new GraphCudaAllocator(GetStream()));
        } else {
            allocator_.reset(new PlainCudaAllocator());
        }
        buffer_manager_.reset(new utils::StackBufferManager(allocator_.get(), true));
    } else if (mm_policy == MM_COMPACT) {
#if PPLNN_CUDACC_VER_MAJOR * 1000 + PPLNN_CUDACC_VER_MINOR * 10 >= 10020
        size_t granularity = 0;
        CUmemAllocationProp prop = {};
        prop.type = CU_MEM_ALLOCATION_TYPE_PINNED;
        prop.location.type = CU_MEM_LOCATION_TYPE_DEVICE;
        prop.location.id = device_id;
        cuMemGetAllocationGranularity(&granularity, &prop, CU_MEM_ALLOC_GRANULARITY_MINIMUM);

        auto allocator = new BufferedCudaAllocator();
        status = allocator->Init(device_id, granularity);
        if (status != RC_SUCCESS) {
            LOG(ERROR) << "init BufferedCudaAllocator failed: " << GetRetCodeStr(status);
            delete allocator;
            return RC_OTHER_ERROR;
        }
        allocator_.reset(allocator);

        uint64_t block_size = DEFAULT_BLOCK_SIZE;
        if (granularity > DEFAULT_BLOCK_SIZE) {
            block_size = granularity;
        }
        buffer_manager_.reset(new utils::CompactBufferManager(allocator, CUDA_DEFAULT_ALIGNMENT, block_size));
#else
        LOG(WARNING) << "Due to lower CUDA version, 'Compact Memory' is not supported, choose 'Perf Mode' instead.";
        allocator_.reset(new PlainCudaAllocator());
        buffer_manager_.reset(new utils::StackBufferManager(allocator_.get(), true));
#endif
    }

    return RC_SUCCESS;
}

BufferedCudaDevice::~BufferedCudaDevice() {
    SyncStream();

    if (buffer_manager_.get()) {
        LOG(DEBUG) << "buffer manager[" << buffer_manager_->GetName() << "] allocates ["
                   << buffer_manager_->GetAllocatedBytes() << "] bytes.";
        buffer_manager_->Free(&shared_tmp_buffer_);
    }
    buffer_manager_.reset();
    allocator_.reset();
}

RetCode BufferedCudaDevice::Realloc(uint64_t bytes, BufferDesc* buffer) {
    return buffer_manager_->Realloc(bytes, buffer);
}

void BufferedCudaDevice::Free(BufferDesc* buffer) {
    if (buffer->addr) {
        buffer_manager_->Free(buffer);
    }
}

RetCode BufferedCudaDevice::AllocTmpBuffer(uint64_t bytes, BufferDesc* buffer) {
    if (bytes > tmp_buffer_size_ || bytes <= tmp_buffer_size_ / 2) {
        auto status = buffer_manager_->Realloc(bytes, &shared_tmp_buffer_);
        if (status == RC_SUCCESS) {
            tmp_buffer_size_ = bytes;
            *buffer = shared_tmp_buffer_;
        }
        return status;
    }

    *buffer = shared_tmp_buffer_;
    return RC_SUCCESS;
}

void BufferedCudaDevice::FreeTmpBuffer(BufferDesc*) {}

}}} // namespace ppl::nn::cuda
