// Standalone Windows SDK/Wine D3DCompile helper; writes a reproducible DXBC header.
#include <windows.h>
#include <d3dcompiler.h>
#include <fstream>
#include <iterator>
#include <cstdio>
#include <string>

int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    std::ifstream input(argv[1], std::ios::binary);
    if (!input)
        return 3;
    const std::string source { std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
    ID3DBlob* binary = nullptr;
    ID3DBlob* errors = nullptr;
    const HRESULT result = D3DCompile(source.data(), source.size(), argv[1], nullptr, nullptr, "CSMain", "cs_5_0",
                                      D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &binary, &errors);
    if (FAILED(result))
    {
        std::ofstream report(std::string(argv[2]) + ".errors");
        if (errors)
            report.write(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize());
        if (errors)
            errors->Release();
        return 4;
    }
    std::ofstream header(argv[2]);
    if (!header)
    {
        binary->Release();
        return 5;
    }
    header << "// Generated from nr_stabilize.hlsl (CSMain, cs_5_0, optimisation 3).\n#pragma once\n"
              "const unsigned char NrStabilizer_cso[] = {\n";
    const auto* bytes = static_cast<const unsigned char*>(binary->GetBufferPointer());
    for (size_t i = 0; i < binary->GetBufferSize(); ++i)
    {
        char value[8];
        std::snprintf(value, sizeof(value), "0x%02x,", bytes[i]);
        header << value << (i % 16 == 15 ? "\n" : " ");
    }
    header << "\n};\n";
    binary->Release();
    if (errors)
        errors->Release();
    return header.good() ? 0 : 6;
}
