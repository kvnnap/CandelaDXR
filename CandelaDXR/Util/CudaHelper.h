#pragma once

#include <stdexcept>
#include <iostream>
#include <sstream>
#include <cuda_runtime.h>

#ifdef _DEBUG

#define CUDA_ERROR(func, ret) \
    if (ret != cudaSuccess) { \
        std::cerr << #func << " failed. Error " << cudaGetErrorName(ret) << " (" << cudaGetErrorString(ret) << \
            "). Line " << __LINE__ << "." << std::endl; \
    }

#define CUDA_EXCEPTION(ret) \
    if (ret != cudaSuccess) \
        throw std::runtime_error("CUDA error!");

#define CUDA(func) \
do { \
    cudaError_t ret = func; \
    CUDA_ERROR(func, ret); \
    CUDA_EXCEPTION(ret); \
} while (false)

#define CUDA_NOEXCEPT(func) \
do { \
    cudaError_t ret = func; \
    CUDA_ERROR(func, ret); \
} while (false)

#else

#define CUDA(func) func
#define CUDA_NOEXCEPT(func) func

#endif
