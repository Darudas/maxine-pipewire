// Dynamic loader for NVIDIA Maxine VFX SDK
// Uses dlopen/dlsym so the SDK doesn't need to be present at compile time.
// Loads both libnvVideoEffects.so and libNVCVImage.so.

#pragma once

#include "nvvfx_types.h"
#include <stdbool.h>

// Function pointer types — NvVFX effect lifecycle
typedef NvCV_Status (*fn_NvVFX_CreateEffect)(NvVFX_EffectSelector, NvVFX_Handle *);
typedef NvCV_Status (*fn_NvVFX_DestroyEffect)(NvVFX_Handle);

// Function pointer types — NvVFX parameter setters
typedef NvCV_Status (*fn_NvVFX_SetU32)(NvVFX_Handle, NvVFX_ParameterSelector, unsigned int);
typedef NvCV_Status (*fn_NvVFX_SetS32)(NvVFX_Handle, NvVFX_ParameterSelector, int);
typedef NvCV_Status (*fn_NvVFX_SetF32)(NvVFX_Handle, NvVFX_ParameterSelector, float);
typedef NvCV_Status (*fn_NvVFX_SetF64)(NvVFX_Handle, NvVFX_ParameterSelector, double);
typedef NvCV_Status (*fn_NvVFX_SetU64)(NvVFX_Handle, NvVFX_ParameterSelector, uint64_t);
typedef NvCV_Status (*fn_NvVFX_SetString)(NvVFX_Handle, NvVFX_ParameterSelector, const char *);
typedef NvCV_Status (*fn_NvVFX_SetImage)(NvVFX_Handle, NvVFX_ParameterSelector, NvCVImage *);
typedef NvCV_Status (*fn_NvVFX_SetObject)(NvVFX_Handle, NvVFX_ParameterSelector, void *);
typedef NvCV_Status (*fn_NvVFX_SetCudaStream)(NvVFX_Handle, NvVFX_ParameterSelector, CUstream);

// Function pointer types — NvVFX parameter getters
typedef NvCV_Status (*fn_NvVFX_GetU32)(NvVFX_Handle, NvVFX_ParameterSelector, unsigned int *);
typedef NvCV_Status (*fn_NvVFX_GetS32)(NvVFX_Handle, NvVFX_ParameterSelector, int *);
typedef NvCV_Status (*fn_NvVFX_GetF32)(NvVFX_Handle, NvVFX_ParameterSelector, float *);
typedef NvCV_Status (*fn_NvVFX_GetF64)(NvVFX_Handle, NvVFX_ParameterSelector, double *);
typedef NvCV_Status (*fn_NvVFX_GetU64)(NvVFX_Handle, NvVFX_ParameterSelector, uint64_t *);
typedef NvCV_Status (*fn_NvVFX_GetString)(NvVFX_Handle, NvVFX_ParameterSelector, const char **);
typedef NvCV_Status (*fn_NvVFX_GetImage)(NvVFX_Handle, NvVFX_ParameterSelector, NvCVImage *);
typedef NvCV_Status (*fn_NvVFX_GetObject)(NvVFX_Handle, NvVFX_ParameterSelector, void **);
typedef NvCV_Status (*fn_NvVFX_GetCudaStream)(NvVFX_Handle, NvVFX_ParameterSelector, CUstream *);

// Function pointer types — NvVFX execution
typedef NvCV_Status (*fn_NvVFX_Load)(NvVFX_Handle);
typedef NvCV_Status (*fn_NvVFX_Run)(NvVFX_Handle, int);

// Function pointer types — NvVFX state management
typedef NvCV_Status (*fn_NvVFX_AllocateState)(NvVFX_Handle, void **);
typedef NvCV_Status (*fn_NvVFX_DeallocateState)(NvVFX_Handle, void *);
typedef NvCV_Status (*fn_NvVFX_ResetState)(NvVFX_Handle, void *);

// Function pointer types — CUDA helpers
typedef NvCV_Status (*fn_NvVFX_CudaStreamCreate)(CUstream *);
typedef NvCV_Status (*fn_NvVFX_CudaStreamDestroy)(CUstream);

// Function pointer types — NvCVImage functions (from libNVCVImage.so)
typedef NvCV_Status (*fn_NvCVImage_Alloc)(NvCVImage *, unsigned int, unsigned int,
                                          int, unsigned char, unsigned char, unsigned int);
typedef void        (*fn_NvCVImage_Dealloc)(NvCVImage *);
typedef NvCV_Status (*fn_NvCVImage_Create)(unsigned int, unsigned int, int,
                                           unsigned char, unsigned char, unsigned int,
                                           NvCVImage **);
typedef void        (*fn_NvCVImage_Destroy)(NvCVImage *);
typedef NvCV_Status (*fn_NvCVImage_Transfer)(const NvCVImage *, NvCVImage *, float,
                                             CUstream, NvCVImage *);
typedef NvCV_Status (*fn_NvCVImage_Init)(NvCVImage *, unsigned int, unsigned int, int,
                                         void *, unsigned char, unsigned char, unsigned int,
                                         unsigned int);

struct maxine_vfx_sdk {
    void *lib_vfx;    // libnvVideoEffects.so handle
    void *lib_cvimg;  // libNVCVImage.so handle

    // Effect lifecycle
    fn_NvVFX_CreateEffect   CreateEffect;
    fn_NvVFX_DestroyEffect  DestroyEffect;

    // Parameter setters
    fn_NvVFX_SetU32         SetU32;
    fn_NvVFX_SetS32         SetS32;
    fn_NvVFX_SetF32         SetF32;
    fn_NvVFX_SetF64         SetF64;
    fn_NvVFX_SetU64         SetU64;
    fn_NvVFX_SetString      SetString;
    fn_NvVFX_SetImage       SetImage;
    fn_NvVFX_SetObject      SetObject;
    fn_NvVFX_SetCudaStream  SetCudaStream;

    // Parameter getters
    fn_NvVFX_GetU32         GetU32;
    fn_NvVFX_GetS32         GetS32;
    fn_NvVFX_GetF32         GetF32;
    fn_NvVFX_GetF64         GetF64;
    fn_NvVFX_GetU64         GetU64;
    fn_NvVFX_GetString      GetString;
    fn_NvVFX_GetImage       GetImage;
    fn_NvVFX_GetObject      GetObject;
    fn_NvVFX_GetCudaStream  GetCudaStream;

    // Execution
    fn_NvVFX_Load           Load;
    fn_NvVFX_Run            Run;

    // State management
    fn_NvVFX_AllocateState    AllocateState;
    fn_NvVFX_DeallocateState  DeallocateState;
    fn_NvVFX_ResetState       ResetState;

    // CUDA helpers
    fn_NvVFX_CudaStreamCreate   CudaStreamCreate;
    fn_NvVFX_CudaStreamDestroy  CudaStreamDestroy;

    // NvCVImage functions (from libNVCVImage.so)
    fn_NvCVImage_Alloc     ImageAlloc;
    fn_NvCVImage_Dealloc   ImageDealloc;
    fn_NvCVImage_Create    ImageCreate;
    fn_NvCVImage_Destroy   ImageDestroy;
    fn_NvCVImage_Transfer  ImageTransfer;
    fn_NvCVImage_Init      ImageInit;
};

// Load the Maxine VFX SDK dynamically.
// sdk_path: path to directory containing libnvVideoEffects.so (or NULL for default search)
// Returns true on success.
bool maxine_vfx_sdk_load(struct maxine_vfx_sdk *sdk, const char *sdk_path);

// Unload the VFX SDK.
void maxine_vfx_sdk_unload(struct maxine_vfx_sdk *sdk);

// Get human-readable error string for NvCV_Status.
const char *nvcv_status_str(NvCV_Status status);
