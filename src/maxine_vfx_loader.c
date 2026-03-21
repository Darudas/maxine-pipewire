#include "maxine_vfx_loader.h"
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

// Load a required symbol from the VFX library — fails if not found.
#define LOAD_VFX_SYM(sdk, name)                                                     \
    do {                                                                             \
        sdk->name = (fn_NvVFX_##name)dlsym(sdk->lib_vfx, "NvVFX_" #name);          \
        if (!sdk->name) {                                                            \
            fprintf(stderr, "maxine-vfx: failed to load NvVFX_%s: %s\n",             \
                    #name, dlerror());                                                \
            maxine_vfx_sdk_unload(sdk);                                              \
            return false;                                                            \
        }                                                                            \
    } while (0)

// Load an optional symbol from the VFX library — sets to NULL if not found.
#define LOAD_VFX_SYM_OPT(sdk, name)                                                 \
    do {                                                                             \
        sdk->name = (fn_NvVFX_##name)dlsym(sdk->lib_vfx, "NvVFX_" #name);          \
    } while (0)

// Load a required symbol from the NvCVImage library — fails if not found.
#define LOAD_CVIMG_SYM(sdk, field, sym_name)                                         \
    do {                                                                             \
        sdk->field = (fn_##sym_name)dlsym(sdk->lib_cvimg, #sym_name);               \
        if (!sdk->field) {                                                           \
            fprintf(stderr, "maxine-vfx: failed to load %s: %s\n",                   \
                    #sym_name, dlerror());                                            \
            maxine_vfx_sdk_unload(sdk);                                              \
            return false;                                                            \
        }                                                                            \
    } while (0)

bool maxine_vfx_sdk_load(struct maxine_vfx_sdk *sdk, const char *sdk_path)
{
    memset(sdk, 0, sizeof(*sdk));

    const char *lib_vfx   = "libnvVideoEffects.so";
    const char *lib_cvimg  = "libNVCVImage.so";
    char path_vfx[4096];
    char path_cvimg[4096];

    if (sdk_path) {
        snprintf(path_vfx, sizeof(path_vfx), "%s/%s", sdk_path, lib_vfx);
        snprintf(path_cvimg, sizeof(path_cvimg), "%s/%s", sdk_path, lib_cvimg);
        lib_vfx = path_vfx;
        lib_cvimg = path_cvimg;
    }

    // Load NvCVImage library first (VFX depends on it)
    sdk->lib_cvimg = dlopen(lib_cvimg, RTLD_LAZY);
    if (!sdk->lib_cvimg) {
        fprintf(stderr, "maxine-vfx: failed to load %s: %s\n", lib_cvimg, dlerror());
        return false;
    }

    // Load VFX library
    sdk->lib_vfx = dlopen(lib_vfx, RTLD_LAZY);
    if (!sdk->lib_vfx) {
        fprintf(stderr, "maxine-vfx: failed to load %s: %s\n", lib_vfx, dlerror());
        maxine_vfx_sdk_unload(sdk);
        return false;
    }

    // Effect lifecycle
    LOAD_VFX_SYM(sdk, CreateEffect);
    LOAD_VFX_SYM(sdk, DestroyEffect);

    // Parameter setters
    LOAD_VFX_SYM(sdk, SetU32);
    LOAD_VFX_SYM(sdk, SetS32);
    LOAD_VFX_SYM(sdk, SetF32);
    LOAD_VFX_SYM(sdk, SetF64);
    LOAD_VFX_SYM(sdk, SetU64);
    LOAD_VFX_SYM(sdk, SetString);
    LOAD_VFX_SYM(sdk, SetImage);
    LOAD_VFX_SYM(sdk, SetObject);
    LOAD_VFX_SYM(sdk, SetCudaStream);

    // Parameter getters
    LOAD_VFX_SYM(sdk, GetU32);
    LOAD_VFX_SYM(sdk, GetS32);
    LOAD_VFX_SYM(sdk, GetF32);
    LOAD_VFX_SYM(sdk, GetF64);
    LOAD_VFX_SYM(sdk, GetU64);
    LOAD_VFX_SYM(sdk, GetString);
    LOAD_VFX_SYM(sdk, GetImage);
    LOAD_VFX_SYM(sdk, GetObject);
    LOAD_VFX_SYM(sdk, GetCudaStream);

    // Execution
    LOAD_VFX_SYM(sdk, Load);
    LOAD_VFX_SYM(sdk, Run);

    // State management — optional (not all SDK versions expose these)
    LOAD_VFX_SYM_OPT(sdk, AllocateState);
    LOAD_VFX_SYM_OPT(sdk, DeallocateState);
    LOAD_VFX_SYM_OPT(sdk, ResetState);

    // CUDA helpers
    LOAD_VFX_SYM(sdk, CudaStreamCreate);
    LOAD_VFX_SYM(sdk, CudaStreamDestroy);

    // NvCVImage functions from libNVCVImage.so
    LOAD_CVIMG_SYM(sdk, ImageAlloc,    NvCVImage_Alloc);
    LOAD_CVIMG_SYM(sdk, ImageDealloc,  NvCVImage_Dealloc);
    LOAD_CVIMG_SYM(sdk, ImageCreate,   NvCVImage_Create);
    LOAD_CVIMG_SYM(sdk, ImageDestroy,  NvCVImage_Destroy);
    LOAD_CVIMG_SYM(sdk, ImageTransfer, NvCVImage_Transfer);
    LOAD_CVIMG_SYM(sdk, ImageInit,     NvCVImage_Init);

    return true;
}

void maxine_vfx_sdk_unload(struct maxine_vfx_sdk *sdk)
{
    if (sdk->lib_vfx) {
        dlclose(sdk->lib_vfx);
        sdk->lib_vfx = NULL;
    }
    if (sdk->lib_cvimg) {
        dlclose(sdk->lib_cvimg);
        sdk->lib_cvimg = NULL;
    }
    memset(sdk, 0, sizeof(*sdk));
}

const char *nvcv_status_str(NvCV_Status status)
{
    switch (status) {
    case NVCV_SUCCESS:                  return "success";
    case NVCV_ERR_GENERAL:              return "general error";
    case NVCV_ERR_UNIMPLEMENTED:        return "unimplemented";
    case NVCV_ERR_MEMORY:               return "memory allocation error";
    case NVCV_ERR_EFFECT:               return "effect error";
    case NVCV_ERR_SELECTOR:             return "invalid selector";
    case NVCV_ERR_BUFFER:               return "buffer error";
    case NVCV_ERR_PARAMETER:            return "invalid parameter";
    case NVCV_ERR_MISMATCH:             return "mismatch";
    case NVCV_ERR_PIXELFORMAT:          return "unsupported pixel format";
    case NVCV_ERR_MODEL:                return "model error";
    case NVCV_ERR_LIBRARY:              return "library error";
    case NVCV_ERR_INITIALIZATION:       return "initialization error";
    case NVCV_ERR_FILE:                 return "file error";
    case NVCV_ERR_FEATURENOTFOUND:      return "feature not found";
    case NVCV_ERR_MISSINGINPUT:         return "missing input";
    case NVCV_ERR_RESOLUTION:           return "unsupported resolution";
    case NVCV_ERR_UNSUPPORTEDGPU:       return "unsupported GPU";
    case NVCV_ERR_WRONGGPU:             return "wrong GPU";
    case NVCV_ERR_UNSUPPORTEDDRIVER:    return "unsupported driver version";
    case NVCV_ERR_MODELPATHNOTFOUND:    return "model path not found";
    case NVCV_ERR_PARSE:                return "parse error";
    case NVCV_ERR_MODELSUBSTITUTION:    return "model substitution error";
    case NVCV_ERR_READ:                 return "read error";
    case NVCV_ERR_WRITE:                return "write error";
    case NVCV_ERR_PARAMREADONLY:        return "parameter is read-only";
    case NVCV_ERR_TRT_ENQUEUE:          return "TensorRT enqueue error";
    case NVCV_ERR_TRT_CONTEXT:          return "TensorRT context error";
    case NVCV_ERR_TRT_INFER:            return "TensorRT inference error";
    case NVCV_ERR_TRT_ENGINE:           return "TensorRT engine error";
    case NVCV_ERR_NPP:                  return "NPP error";
    case NVCV_ERR_CONFIG:               return "configuration error";
    case NVCV_ERR_TOOSMALL:             return "value too small";
    case NVCV_ERR_TOOBIG:               return "value too big";
    case NVCV_ERR_WRONGSIZE:            return "wrong size";
    case NVCV_ERR_OBJECTNOTFOUND:       return "object not found";
    case NVCV_ERR_SINGULAR:             return "singular matrix";
    case NVCV_ERR_NOTHINGRENDERED:      return "nothing rendered";
    case NVCV_ERR_CONVERGENCE:          return "convergence error";
    case NVCV_ERR_OPENGL:               return "OpenGL error";
    case NVCV_ERR_DIRECT3D:             return "Direct3D error";
    case NVCV_ERR_CUDA_MEMORY:          return "CUDA memory error";
    case NVCV_ERR_CUDA_VALUE:           return "CUDA value error";
    case NVCV_ERR_CUDA_PITCH:           return "CUDA pitch error";
    case NVCV_ERR_CUDA_INIT:            return "CUDA init error";
    case NVCV_ERR_CUDA_LAUNCH:          return "CUDA launch error";
    case NVCV_ERR_CUDA_KERNEL:          return "CUDA kernel error";
    case NVCV_ERR_CUDA_DRIVER:          return "CUDA driver error";
    case NVCV_ERR_CUDA_UNSUPPORTED:     return "CUDA unsupported";
    case NVCV_ERR_CUDA_ILLEGAL_ADDRESS: return "CUDA illegal address";
    case NVCV_ERR_CUDA:                 return "CUDA error";
    default:                            return "unknown error";
    }
}
