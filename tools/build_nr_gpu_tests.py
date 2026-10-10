#!/usr/bin/env python3
"""Build isolated Windows GPU fixtures using LLVM-MinGW and official Detours 4.0.1.

No game files, system compiler or Proton prefix are modified. Third-party source
is staged in the output directory; compatibility edits do not enter production.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path
import shutil
import subprocess

REPO = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--toolchain", required=True, type=Path)
    parser.add_argument("--detours", required=True, type=Path, help="unmodified official v4.0.1 source directory")
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    compiler = args.toolchain.resolve() / "bin/x86_64-w64-mingw32-clang++"
    source = args.detours.resolve() / "src"
    output = args.output.resolve()
    if not compiler.is_file() or not (source / "detours.h").is_file():
        raise SystemExit("Missing compiler or Detours source")
    output.mkdir(parents=True, exist_ok=True)
    staged = output / "detours-compat"
    shutil.copytree(source, staged, dirs_exist_ok=True)
    header = staged / "detours.h"
    text = header.read_text()
    if not re.search(r"#define\s+DETOURS_VERSION\s+0x4c0c1\b", text) or text.count("#if (_MSC_VER < 1299)") != 2:
        raise SystemExit("Expected unmodified Detours 4.0.1 header")
    header.write_text(text.replace("#if (_MSC_VER < 1299)", "#if defined(_MSC_VER) && (_MSC_VER < 1299)"))
    modules = staged / "modules.cpp"
    text = modules.read_text()
    if text.count('return GetProcAddress(hClr, "_CorExeMain");') != 1:
        raise SystemExit("Expected unmodified Detours modules.cpp")
    modules.write_text(text.replace("#include <windows.h>", "#include <windows.h>\n#include <strsafe.h>")
                      .replace('return GetProcAddress(hClr, "_CorExeMain");',
                               'return reinterpret_cast<PVOID>(GetProcAddress(hClr, "_CorExeMain"));'))
    common = [str(compiler), "-std=c++17", "-static"]
    third_party = []
    for name in ("detours", "disasm", "modules"):
        obj = output / f"{name}.o"
        subprocess.run(common + ["-fms-extensions", "-Wno-unknown-pragmas", "-c", str(staged / f"{name}.cpp"), "-o", str(obj)], check=True)
        third_party.append(str(obj))
    strict = common + ["-Wall", "-Wextra", "-Werror"]
    # The standalone fixture supplies application plumbing; cache defaults come
    # from the real production Config declarations, not a duplicate test model.
    fixture = output / "cache-fixture-config"
    fixture.mkdir(exist_ok=True)
    fields = re.findall(r"^    CustomOptional<[^>]+> DlssNrCache\w+ \{ [^}]+ \};$", (REPO / "OptiScaler/Config.h").read_text(), re.M)
    if len(fields) != 18:
        raise SystemExit("Expected 18 production cache config declarations")
    (fixture / "Config.h").write_text("#pragma once\n#include <cstdint>\n"
        "template<class T> struct CustomOptional { T value; T value_or_default() const { return value; } "
        "CustomOptional& operator=(T v) { value=v; return *this; } };\nclass Config { public:\n"
        "CustomOptional<bool> UsePrecompiledShaders { true };\n" + "\n".join(fields) +
        "\nstatic Config* Instance() { static Config cfg; return &cfg; }\n};\n")
    lifetime_obj = output / "NrGpuLifetime.o"
    subprocess.run(strict + ["-DNR_LIFETIME_STANDALONE", "-I" + str(REPO / "tests/standalone"),
                   "-I" + str(REPO / "OptiScaler/include"), "-c",
                   str(REPO / "OptiScaler/dlssnr/NrGpuLifetime.cpp"), "-o", str(lifetime_obj)], check=True)
    gpu = REPO / "tests/nr_lifetime_gpu.cpp"
    libs = ["-ld3d12", "-ldxgi", "-ldxguid"]
    binaries = []
    for name, inputs, extra in (
        ("nr_lifetime_gpu.exe", [gpu], []),
        ("nr_lifetime_runtime_gpu.exe", [gpu, REPO / "tests/nr_lifetime_queue_adapter.cpp",
          REPO / "OptiScaler/dlssnr/NrGpuLifetime.cpp"],
         ["-DNR_LIFETIME_STANDALONE", "-I" + str(REPO / "tests/standalone"),
          "-I" + str(REPO / "OptiScaler/include")] + third_party),
        ("nr_stabilizer_gpu.exe", [REPO / "tests/nr_stabilizer_gpu.cpp"], ["-ld3dcompiler_47"]),
        ("nr_cache_gpu.exe", [REPO / "tests/nr_cache_gpu.cpp", REPO / "tests/nr_lifetime_queue_adapter.cpp",
          REPO / "tests/nr_cache_shader_compile.cpp", lifetime_obj,
          REPO / "OptiScaler/shaders/dlssnr/DlssNr_EditCache_Dx12.cpp",
          REPO / "OptiScaler/shaders/Shader_Dx12.cpp", REPO / "OptiScaler/gpu_time/GpuTime_Dx12.cpp"],
         ["-DNR_LIFETIME_STANDALONE", "-Wno-unknown-pragmas", "-I" + str(fixture), "-I" + str(REPO / "tests/standalone/cache"),
          "-I" + str(REPO / "OptiScaler"), "-I" + str(REPO / "OptiScaler/include"), "-ld3dcompiler_47"] + third_party),
    ):
        target = output / name
        subprocess.run(strict + [str(p) for p in inputs] + extra + libs + ["-o", str(target)], check=True)
        binaries.append({"name": name, "sha256": sha(target)})
    tracked = ("OptiScaler/dlssnr/DlssNr_Capture.h", "OptiScaler/dlssnr/NrGpuLifetime.cpp", "OptiScaler/dlssnr/NrGpuLifetime.h", "OptiScaler/dlssnr/SubmissionLifetime.h",
               "OptiScaler/shaders/dlssnr/NrStabilizer_Common.h",
               "OptiScaler/shaders/dlssnr/precompile/nr_stabilize.hlsl",
               "OptiScaler/shaders/dlssnr/precompile/NrStabilizer_Shader.h",
               "OptiScaler/shaders/dlssnr/DlssNr_EditCache_Dx12.cpp", "OptiScaler/shaders/dlssnr/DlssNr_EditCache_Dx12.h",
               "OptiScaler/shaders/dlssnr/DlssNr_CacheCommon.h", "OptiScaler/dlssnr/EditCacheCadence.h",
               "OptiScaler/shaders/dlssnr/precompile/dlssnr_cache.hlsl", "OptiScaler/shaders/dlssnr/precompile/DlssNr_Cache_Shader.h",
               "OptiScaler/shaders/Shader_Dx12.cpp", "OptiScaler/gpu_time/GpuTime_Dx12.cpp", "OptiScaler/gpu_time/GpuTime_Dx12.h",
               "OptiScaler/dlssnr/GpuTimingWindow.h", "tests/nr_gpu_timing_test.cpp", "OptiScaler/Config.h",
               "tests/nr_cache_gpu.cpp", "tests/nr_cache_shader_compile.cpp", "tests/nr_cache_cadence_test.cpp",
               "tests/standalone/cache/pch.h", "tests/standalone/cache/SysUtils.h", "tests/standalone/cache/State.h",
               "tests/standalone/cache/Util.h", "tools/build_nr_gpu_tests.py", "tools/run_nr_cache_gpu_tests.py",
               "tools/compile_nr_shader.cpp",
               "tests/nr_lifetime_gpu.cpp", "tests/nr_lifetime_queue_adapter.cpp", "tests/nr_stabilizer_gpu.cpp")
    manifest = {
        "source_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=REPO, text=True).strip(),
        "dirty": bool(subprocess.check_output(["git", "status", "--porcelain"], cwd=REPO)),
        "source_sha256": {p: sha(REPO / p) for p in tracked},
        "compiler": subprocess.check_output([str(compiler), "--version"], text=True).splitlines()[0],
        "detours": {"upstream": "https://github.com/microsoft/Detours", "commit": "e4bfd6b03e50de46b47abfbd1e46b384f0c5f833",
                    "original_header_sha256": sha(source / "detours.h"), "staged_header_sha256": sha(header),
                    "compatibility_edits": "guard legacy LONG_PTR typedef, include strsafe, explicit function-pointer cast; no hooking logic changed",
                    "production": "MSVC build links the unchanged repository detours.lib"},
        "files": binaries,
        "limits": "No NGX vendor model, actual exported game hook routing, FG, appearance or frame-performance acceptance",
    }
    (output / "gpu-build-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps({"built": [x["name"] for x in binaries], "manifest": str(output / "gpu-build-manifest.json")}))


if __name__ == "__main__":
    main()
