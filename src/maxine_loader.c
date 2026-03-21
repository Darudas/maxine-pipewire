#include "maxine_loader.h"
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

// Load a required symbol — fails if not found.
#define LOAD_SYM(sdk, name)                                                    \
    do {                                                                       \
        sdk->name = (fn_NvAFX_##name)dlsym(sdk->lib_handle, "NvAFX_" #name);  \
        if (!sdk->name) {                                                      \
            fprintf(stderr, "maxine: failed to load NvAFX_%s: %s\n",           \
                    #name, dlerror());                                          \
            maxine_sdk_unload(sdk);                                            \
            return false;                                                      \
        }                                                                      \
    } while (0)

// Load an optional symbol — sets to NULL if not found, does not fail.
#define LOAD_SYM_OPT(sdk, name)                                                \
    do {                                                                       \
        sdk->name = (fn_NvAFX_##name)dlsym(sdk->lib_handle, "NvAFX_" #name);  \
    } while (0)

bool maxine_sdk_load(struct maxine_sdk *sdk, const char *sdk_path)
{
    memset(sdk, 0, sizeof(*sdk));

    const char *lib = "libnv_audiofx.so";
    char path_buf[4096];

    if (sdk_path) {
        snprintf(path_buf, sizeof(path_buf), "%s/%s", sdk_path, lib);
        lib = path_buf;
    }

    sdk->lib_handle = dlopen(lib, RTLD_LAZY);
    if (!sdk->lib_handle) {
        fprintf(stderr, "maxine: failed to load %s: %s\n", lib, dlerror());
        return false;
    }

    // Required symbols — loading fails if any are missing
    LOAD_SYM(sdk, GetEffectList);
    LOAD_SYM(sdk, CreateEffect);
    LOAD_SYM(sdk, DestroyEffect);
    LOAD_SYM(sdk, SetString);
    LOAD_SYM(sdk, SetU32);
    LOAD_SYM(sdk, SetFloat);
    LOAD_SYM(sdk, GetU32);
    LOAD_SYM(sdk, GetFloat);
    LOAD_SYM(sdk, Load);
    LOAD_SYM(sdk, Run);
    LOAD_SYM(sdk, Reset);

    // Optional symbols — NULL if not available (older SDK, etc.)
    LOAD_SYM_OPT(sdk, CreateChainedEffect);
    LOAD_SYM_OPT(sdk, SetFloatList);
    LOAD_SYM_OPT(sdk, SetU32List);
    LOAD_SYM_OPT(sdk, SetStringList);
    LOAD_SYM_OPT(sdk, SetBoolList);
    LOAD_SYM_OPT(sdk, GetString);
    LOAD_SYM_OPT(sdk, GetFloatList);
    LOAD_SYM_OPT(sdk, GetU32List);
    LOAD_SYM_OPT(sdk, GetBoolList);
    LOAD_SYM_OPT(sdk, GetStringList);
#ifdef __linux__
    LOAD_SYM_OPT(sdk, SetStreamFloatList);
#endif
    LOAD_SYM_OPT(sdk, InitializeLogger);
    LOAD_SYM_OPT(sdk, UninitializeLogger);

    return true;
}

void maxine_sdk_unload(struct maxine_sdk *sdk)
{
    if (sdk->lib_handle) {
        dlclose(sdk->lib_handle);
        sdk->lib_handle = NULL;
    }
    memset(sdk, 0, sizeof(*sdk));
}

const char *nvafx_status_str(NvAFX_Status status)
{
    switch (status) {
    case NVAFX_STATUS_SUCCESS:                      return "success";
    case NVAFX_STATUS_FAILED:                       return "failed";
    case NVAFX_STATUS_INVALID_HANDLE:               return "invalid handle";
    case NVAFX_STATUS_INVALID_PARAM:                return "invalid parameter";
    case NVAFX_STATUS_IMMUTABLE_PARAM:              return "immutable parameter";
    case NVAFX_STATUS_INSUFFICIENT_DATA:            return "insufficient data";
    case NVAFX_STATUS_EFFECT_NOT_AVAILABLE:         return "effect not available";
    case NVAFX_STATUS_OUTPUT_BUFFER_TOO_SMALL:      return "output buffer too small";
    case NVAFX_STATUS_MODEL_LOAD_FAILED:            return "model load failed";
    case NVAFX_STATUS_MODEL_NOT_LOADED:             return "model not loaded";
    case NVAFX_STATUS_INCOMPATIBLE_MODEL:           return "incompatible model";
    case NVAFX_STATUS_GPU_UNSUPPORTED:              return "GPU unsupported (no tensor cores?)";
    case NVAFX_STATUS_CUDA_CONTEXT_CREATION_FAILED: return "CUDA context creation failed";
    case NVAFX_STATUS_NO_SUPPORTED_GPU_FOUND:       return "no supported GPU found";
    case NVAFX_STATUS_WRONG_GPU:                    return "wrong GPU selected";
    case NVAFX_STATUS_CUDA_ERROR:                   return "CUDA error";
    case NVAFX_STATUS_INVALID_OPERATION:            return "invalid operation";
    default:                                        return "unknown error";
    }
}
