// Shader_Dx12 links this generic helper; the tested pipeline uses the exact
// precompiled production DXBC, so no runtime shader substitution occurs.
#include <windows.h>
#include <d3dcompiler.h>
#include <cstring>
ID3DBlob* CompileShader(const char* source, const char* entry, const char* target)
{
    ID3DBlob* output = nullptr;
    ID3DBlob* error = nullptr;
    const auto result = D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr, entry, target,
                                   D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &output, &error);
    if (error)
        error->Release();
    if (FAILED(result))
        return nullptr;
    return output;
}
