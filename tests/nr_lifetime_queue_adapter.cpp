// Isolated adapter for the production runtime GPU harness. The game reuses
// ResTrack_Dx12's existing queue hook; this fixture deliberately has no FG layer.
#include <windows.h>
#include <d3d12.h>
#include "nr_detours_compat.h"
#include "../OptiScaler/dlssnr/NrGpuLifetime.h"

using Execute = void(STDMETHODCALLTYPE*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);
Execute originalExecute = nullptr;
void STDMETHODCALLTYPE ExecuteHook(ID3D12CommandQueue* queue, UINT count, ID3D12CommandList* const* lists)
{
    const auto batch = DlssNr::GpuLifetime::Submitting(queue, count, lists);
    originalExecute(queue, count, lists);
    DlssNr::GpuLifetime::Submitted(queue, batch);
}
bool NrTestEnsureQueueHook(ID3D12Device* device)
{
    if (originalExecute)
        return true;
    ID3D12CommandQueue* queue = nullptr;
    D3D12_COMMAND_QUEUE_DESC desc {};
    desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (FAILED(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&queue))))
        return false;
    originalExecute = reinterpret_cast<Execute>((*reinterpret_cast<void***>(queue))[10]);
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(reinterpret_cast<PVOID*>(&originalExecute), reinterpret_cast<PVOID>(ExecuteHook));
    const auto status = DetourTransactionCommit();
    queue->Release();
    if (status != NO_ERROR)
        originalExecute = nullptr;
    return status == NO_ERROR;
}
