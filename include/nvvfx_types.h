// NVIDIA Maxine VFX SDK type definitions
// Extracted from nvVideoEffects.h and nvCVImage.h for dynamic loading (dlopen)
// This avoids requiring the SDK headers at compile time.

#pragma once

#include <stdint.h>

typedef void *NvVFX_Handle;
typedef const char *NvVFX_EffectSelector;
typedef const char *NvVFX_ParameterSelector;
typedef void *CUstream;

// NvCV_Status — return codes shared between VFX and CV Image APIs
typedef enum {
    NVCV_SUCCESS                  = 0,
    NVCV_ERR_GENERAL              = -1,
    NVCV_ERR_UNIMPLEMENTED        = -2,
    NVCV_ERR_MEMORY               = -3,
    NVCV_ERR_EFFECT               = -4,
    NVCV_ERR_SELECTOR             = -5,
    NVCV_ERR_BUFFER               = -6,
    NVCV_ERR_PARAMETER            = -7,
    NVCV_ERR_MISMATCH             = -8,
    NVCV_ERR_PIXELFORMAT          = -9,
    NVCV_ERR_MODEL                = -10,
    NVCV_ERR_LIBRARY              = -11,
    NVCV_ERR_INITIALIZATION       = -12,
    NVCV_ERR_FILE                 = -13,
    NVCV_ERR_FEATURENOTFOUND      = -14,
    NVCV_ERR_MISSINGINPUT         = -15,
    NVCV_ERR_RESOLUTION           = -16,
    NVCV_ERR_UNSUPPORTEDGPU       = -17,
    NVCV_ERR_WRONGGPU             = -18,
    NVCV_ERR_UNSUPPORTEDDRIVER    = -19,
    NVCV_ERR_MODELPATHNOTFOUND    = -20,
    NVCV_ERR_PARSE                = -21,
    NVCV_ERR_MODELSUBSTITUTION    = -22,
    NVCV_ERR_READ                 = -23,
    NVCV_ERR_WRITE                = -24,
    NVCV_ERR_PARAMREADONLY        = -25,
    NVCV_ERR_TRT_ENQUEUE          = -26,
    NVCV_ERR_TRT_CONTEXT          = -27,
    NVCV_ERR_TRT_INFER            = -28,
    NVCV_ERR_TRT_ENGINE           = -29,
    NVCV_ERR_NPP                  = -30,
    NVCV_ERR_CONFIG               = -31,
    NVCV_ERR_TOOSMALL             = -32,
    NVCV_ERR_TOOBIG               = -33,
    NVCV_ERR_WRONGSIZE            = -34,
    NVCV_ERR_OBJECTNOTFOUND       = -35,
    NVCV_ERR_SINGULAR             = -36,
    NVCV_ERR_NOTHINGRENDERED      = -37,
    NVCV_ERR_CONVERGENCE          = -38,
    NVCV_ERR_OPENGL               = -39,
    NVCV_ERR_DIRECT3D             = -40,
    NVCV_ERR_CUDA_MEMORY          = -41,
    NVCV_ERR_CUDA_VALUE           = -42,
    NVCV_ERR_CUDA_PITCH           = -43,
    NVCV_ERR_CUDA_INIT            = -44,
    NVCV_ERR_CUDA_LAUNCH          = -45,
    NVCV_ERR_CUDA_KERNEL          = -46,
    NVCV_ERR_CUDA_DRIVER          = -47,
    NVCV_ERR_CUDA_UNSUPPORTED     = -48,
    NVCV_ERR_CUDA_ILLEGAL_ADDRESS = -49,
    NVCV_ERR_CUDA                 = -50,
} NvCV_Status;

// Pixel format constants for NvCVImage
typedef enum {
    NVCV_FORMAT_UNKNOWN = 0,
    NVCV_Y              = 1,   // luminance (grayscale)
    NVCV_A              = 2,   // alpha
    NVCV_YA             = 3,   // luminance + alpha
    NVCV_RGB            = 4,
    NVCV_BGR            = 5,
    NVCV_RGBA           = 6,
    NVCV_BGRA           = 7,
    NVCV_ARGB           = 8,
    NVCV_ABGR           = 9,
    NVCV_YUV420         = 10,
    NVCV_YUV422         = 11,
    NVCV_YUV444         = 12,
} NvCVImage_PixelFormat;

// Component type constants for NvCVImage
typedef enum {
    NVCV_TYPE_UNKNOWN = 0,
    NVCV_U8           = 1,   // unsigned 8-bit
    NVCV_U16          = 2,   // unsigned 16-bit
    NVCV_S16          = 3,   // signed 16-bit
    NVCV_F16          = 4,   // half float
    NVCV_U32          = 5,   // unsigned 32-bit
    NVCV_S32          = 6,   // signed 32-bit
    NVCV_F32          = 7,   // float
    NVCV_U64          = 8,   // unsigned 64-bit
    NVCV_F64          = 9,   // double
} NvCVImage_ComponentType;

// Memory location
#define NVCV_CPU  0
#define NVCV_GPU  1
#define NVCV_CUDA 1

// Layout
#define NVCV_INTERLEAVED 0
#define NVCV_CHUNKY      0
#define NVCV_PLANAR      1
#define NVCV_UYVY        2

// NvCVImage — simplified struct matching the SDK's memory layout
typedef struct NvCVImage {
    unsigned int width;          // pixel width
    unsigned int height;         // pixel height
    int pitch;                   // bytes per row (may be negative for bottom-up)
    unsigned char pixelFormat;   // NvCVImage_PixelFormat
    unsigned char componentType; // NvCVImage_ComponentType
    unsigned char numComponents; // number of components per pixel
    unsigned char planar;        // NVCV_INTERLEAVED or NVCV_PLANAR
    unsigned char gpuMem;        // NVCV_CPU or NVCV_GPU
    unsigned char reserved[3];   // padding
    void *pixels;                // pointer to pixel data
    void *deletePtr;             // internal — do not touch
    void (*deleteProc)(void *);  // internal — do not touch
    uint64_t bufferBytes;        // allocated buffer size in bytes
} NvCVImage;

// NvVFX effect selectors
#define NVVFX_EFFECT_GREEN_SCREEN        "GreenScreen"
#define NVVFX_EFFECT_BACKGROUND_BLUR     "BackgroundBlur"
#define NVVFX_EFFECT_ARTIFACT_REDUCTION  "ArtifactReduction"
#define NVVFX_EFFECT_SUPER_RES           "SuperRes"
#define NVVFX_EFFECT_UPSCALE             "Upscale"
#define NVVFX_EFFECT_DENOISING           "Denoising"
#define NVVFX_EFFECT_EYE_CONTACT         "EyeContact"

// NvVFX parameter selectors
#define NVVFX_INPUT_IMAGE_0              "SrcImage0"
#define NVVFX_INPUT_IMAGE_1              "SrcImage1"
#define NVVFX_OUTPUT_IMAGE_0             "DstImage0"
#define NVVFX_MODEL_DIRECTORY            "ModelDir"
#define NVVFX_CUDA_STREAM                "CudaStream"
#define NVVFX_INFO                       "Info"
#define NVVFX_SCALE                      "Scale"
#define NVVFX_STRENGTH                   "Strength"
#define NVVFX_STRENGTH_LEVELS            "StrengthLevels"
#define NVVFX_MODE                       "Mode"
#define NVVFX_TEMPORAL                   "Temporal"
#define NVVFX_GPU                        "GPU"
#define NVVFX_BATCH_SIZE                 "BatchSize"
#define NVVFX_MODEL_BATCH                "ModelBatch"
#define NVVFX_STATE                      "State"
#define NVVFX_STATE_SIZE                 "StateSize"
#define NVVFX_STATE_COUNT                "NumState"
#define NVVFX_OUTPUT_CLASS               "OutputClass"
#define NVVFX_INPUT_IMAGE_FORMAT         "InputImageFormat"
#define NVVFX_OUTPUT_IMAGE_FORMAT        "OutputImageFormat"
