// Dynamic loader for NVIDIA Maxine AFX SDK
// Uses dlopen/dlsym so the SDK doesn't need to be present at compile time.

#pragma once

#include "nvafx_types.h"
#include <stdbool.h>

// Function pointer types — required SDK functions
typedef NvAFX_Status (*fn_NvAFX_GetEffectList)(int *, NvAFX_EffectSelector *[]);
typedef NvAFX_Status (*fn_NvAFX_CreateEffect)(NvAFX_EffectSelector, NvAFX_Handle *);
typedef NvAFX_Status (*fn_NvAFX_DestroyEffect)(NvAFX_Handle);
typedef NvAFX_Status (*fn_NvAFX_SetString)(NvAFX_Handle, NvAFX_ParameterSelector, const char *);
typedef NvAFX_Status (*fn_NvAFX_SetU32)(NvAFX_Handle, NvAFX_ParameterSelector, unsigned int);
typedef NvAFX_Status (*fn_NvAFX_SetFloat)(NvAFX_Handle, NvAFX_ParameterSelector, float);
typedef NvAFX_Status (*fn_NvAFX_GetU32)(NvAFX_Handle, NvAFX_ParameterSelector, unsigned int *);
typedef NvAFX_Status (*fn_NvAFX_GetFloat)(NvAFX_Handle, NvAFX_ParameterSelector, float *);
typedef NvAFX_Status (*fn_NvAFX_Load)(NvAFX_Handle);
typedef NvAFX_Status (*fn_NvAFX_Run)(NvAFX_Handle, const float **, float **, unsigned int, unsigned int);
typedef NvAFX_Status (*fn_NvAFX_Reset)(NvAFX_Handle, NvAFX_Bool *, int);

// Function pointer types — optional SDK functions (may not exist in older SDK versions)
typedef NvAFX_Status (*fn_NvAFX_CreateChainedEffect)(NvAFX_EffectSelector, NvAFX_Handle *);
typedef NvAFX_Status (*fn_NvAFX_SetFloatList)(NvAFX_Handle, NvAFX_ParameterSelector, float *, unsigned int);
typedef NvAFX_Status (*fn_NvAFX_SetU32List)(NvAFX_Handle, NvAFX_ParameterSelector, const unsigned int *, unsigned int);
typedef NvAFX_Status (*fn_NvAFX_SetStringList)(NvAFX_Handle, NvAFX_ParameterSelector, const char **, unsigned int);
typedef NvAFX_Status (*fn_NvAFX_SetBoolList)(NvAFX_Handle, NvAFX_ParameterSelector, const NvAFX_Bool *, unsigned int);
typedef NvAFX_Status (*fn_NvAFX_GetString)(NvAFX_Handle, NvAFX_ParameterSelector, char *, int);
typedef NvAFX_Status (*fn_NvAFX_GetFloatList)(NvAFX_Handle, NvAFX_ParameterSelector, float *, unsigned int);
typedef NvAFX_Status (*fn_NvAFX_GetU32List)(NvAFX_Handle, NvAFX_ParameterSelector, unsigned int *[], int *);
typedef NvAFX_Status (*fn_NvAFX_GetBoolList)(NvAFX_Handle, NvAFX_ParameterSelector, NvAFX_Bool *[], int *);
typedef NvAFX_Status (*fn_NvAFX_GetStringList)(NvAFX_Handle, NvAFX_ParameterSelector, char **, int *, unsigned int);
#ifdef __linux__
typedef NvAFX_Status (*fn_NvAFX_SetStreamFloatList)(NvAFX_Handle, NvAFX_ParameterSelector,
                                                     const unsigned int *, const float **, unsigned int);
#endif
typedef NvAFX_Status (*fn_NvAFX_InitializeLogger)(int, const char *);
typedef NvAFX_Status (*fn_NvAFX_UninitializeLogger)(void);

struct maxine_sdk {
    void *lib_handle;

    // Required functions — always loaded
    fn_NvAFX_CreateEffect   CreateEffect;
    fn_NvAFX_DestroyEffect  DestroyEffect;
    fn_NvAFX_SetString      SetString;
    fn_NvAFX_SetU32         SetU32;
    fn_NvAFX_SetFloat       SetFloat;
    fn_NvAFX_GetU32         GetU32;
    fn_NvAFX_GetFloat       GetFloat;
    fn_NvAFX_Load           Load;
    fn_NvAFX_Run            Run;
    fn_NvAFX_Reset          Reset;

    // Optional functions — NULL if not available in loaded SDK
    fn_NvAFX_GetEffectList       GetEffectList;
    fn_NvAFX_CreateChainedEffect CreateChainedEffect;
    fn_NvAFX_SetFloatList        SetFloatList;
    fn_NvAFX_SetU32List          SetU32List;
    fn_NvAFX_SetStringList       SetStringList;
    fn_NvAFX_SetBoolList         SetBoolList;
    fn_NvAFX_GetString           GetString;
    fn_NvAFX_GetFloatList        GetFloatList;
    fn_NvAFX_GetU32List          GetU32List;
    fn_NvAFX_GetBoolList         GetBoolList;
    fn_NvAFX_GetStringList       GetStringList;
#ifdef __linux__
    fn_NvAFX_SetStreamFloatList  SetStreamFloatList;
#endif
    fn_NvAFX_InitializeLogger    InitializeLogger;
    fn_NvAFX_UninitializeLogger  UninitializeLogger;
};

// Load the Maxine SDK dynamically.
// sdk_path: path to directory containing libnv_audiofx.so (or NULL for default search)
// Returns true on success.
bool maxine_sdk_load(struct maxine_sdk *sdk, const char *sdk_path);

// Unload the SDK.
void maxine_sdk_unload(struct maxine_sdk *sdk);

// Get human-readable error string for NvAFX_Status.
const char *nvafx_status_str(NvAFX_Status status);
