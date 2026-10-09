#pragma once
#include "SysUtils.h"
#include <Config.h>
#include "State.h"
#include "Util.h"
#include <d3dx/d3dx12.h>
#include <shaders/Shader_Dx12Utils.h>
// Shader heap creation normally marks the application's descriptor-capture
// observer. There is no observer in this isolated fixture.
struct ScopedSkipHeapCapture
{
    ScopedSkipHeapCapture() {}
    ~ScopedSkipHeapCapture() {}
};
