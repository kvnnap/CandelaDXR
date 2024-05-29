#pragma once

#include <stdexcept>
#include <iostream>
#include <optix_types.h>

#ifdef _DEBUG

#define OPTIX_ERROR(func, ret) \
    if (ret != OPTIX_SUCCESS) \
        std::cerr << #func << " failed. Error " << ret << ". Line " << __LINE__ << "." << std::endl;

#define OPTIX_EXCEPTION(ret) \
    if (ret != OPTIX_SUCCESS) \
        throw std::runtime_error("OptiX error!");

#define OPTIX(func) \
do { \
    OptixResult ret = func; \
    OPTIX_ERROR(func, ret); \
    OPTIX_EXCEPTION(ret); \
} while (false)

#define OPTIX_NOEXCEPT(func) \
do { \
    OptixResult ret = func; \
    OPTIX_ERROR(func, ret); \
} while (false)

#else

#define OPTIX(func) func
#define OPTIX_NOEXCEPT(func) func

#endif
