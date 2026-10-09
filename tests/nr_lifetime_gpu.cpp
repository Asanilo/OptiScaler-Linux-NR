// Real D3D12 queues/fences exercise the production Recording/slot policy.
// Does not load NGX or replace the separate game/hook-routing acceptance gate.
#include "../OptiScaler/dlssnr/SubmissionLifetime.h"
#ifdef NR_LIFETIME_STANDALONE
#include "../OptiScaler/dlssnr/NrGpuLifetime.h"
#endif
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <cstdio>
#include <cstring>
#include <stdexcept>

using namespace DlssNr::Lifetime;
void Check(HRESULT result, const char* operation)
{
    if (FAILED(result))
    {
        std::fprintf(stderr, "%s: 0x%08lx\n", operation, static_cast<unsigned long>(result));
        throw std::runtime_error(operation);
    }
}
void Require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

struct GpuCompletion : Completion
{
    ID3D12Fence* fence = nullptr;
    ~GpuCompletion() override
    {
        if (fence)
            fence->Release();
    }
    uint64_t Value() const override { return fence->GetCompletedValue(); }
};
void Wait(ID3D12Fence* fence, uint64_t value)
{
    HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    Require(event != nullptr, "CreateEvent failed");
    Check(fence->SetEventOnCompletion(value, event), "SetEventOnCompletion");
    const auto status = WaitForSingleObject(event, 30000);
    CloseHandle(event);
    Require(status == WAIT_OBJECT_0, "GPU fence timed out");
}
ID3D12Resource* Buffer(ID3D12Device* device, D3D12_HEAP_TYPE type, D3D12_RESOURCE_STATES state)
{
    D3D12_HEAP_PROPERTIES heap {};
    heap.Type = type;
    D3D12_RESOURCE_DESC desc {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = 256;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ID3D12Resource* resource = nullptr;
    Check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(&resource)),
          "CreateCommittedResource");
    return resource;
}
void Hold(const Token& recording, IUnknown* resource)
{
    resource->AddRef();
    recording->Hold(resource, std::shared_ptr<void>(resource, [](void* p) { static_cast<IUnknown*>(p)->Release(); }));
}
#ifdef NR_LIFETIME_STANDALONE
struct TestFeature
{
    bool* released;
};
void __cdecl ReleaseTestFeature(void* value)
{
    auto* feature = static_cast<TestFeature*>(value);
    *feature->released = true;
    delete feature;
}
#endif

int main(int argc, char** argv)
{
    if (argc > 2)
    {
        if (!std::freopen(argv[2], "w", stdout))
            return 2;
        std::setvbuf(stdout, nullptr, _IONBF, 0);
    }
    const bool unsafeReuse = argc > 1 && std::strcmp(argv[1], "--unsafe-reuse") == 0;
    const bool unsafeRetire = argc > 1 && std::strcmp(argv[1], "--unsafe-retire") == 0;
    ID3D12Fence* gate1 = nullptr;
    ID3D12Fence* gate2 = nullptr;
    try
    {
        IDXGIFactory4* factory = nullptr;
        Check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "CreateDXGIFactory1");
        ID3D12Device* device = nullptr;
        for (UINT index = 0; !device; ++index)
        {
            IDXGIAdapter1* adapter = nullptr;
            if (factory->EnumAdapters1(index, &adapter) == DXGI_ERROR_NOT_FOUND)
                break;
            DXGI_ADAPTER_DESC1 desc {};
            adapter->GetDesc1(&desc);
            if (desc.VendorId == 0x10de && !(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
            {
                Check(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)), "D3D12CreateDevice");
                std::printf("NVIDIA hardware adapter %04x:%04x, dedicated VRAM %llu MB\n", desc.VendorId, desc.DeviceId,
                            static_cast<unsigned long long>(desc.DedicatedVideoMemory >> 20));
            }
            adapter->Release();
        }
        factory->Release();
        Require(device != nullptr, "No NVIDIA hardware adapter; refusing a software-only pass");
        ID3D12CommandQueue* queue[2] {};
        ID3D12CommandAllocator* allocator[2] {};
        std::shared_ptr<GpuCompletion> completion[2];
        D3D12_COMMAND_QUEUE_DESC queueDesc {};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        for (unsigned int i = 0; i < 2; ++i)
        {
            Check(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue[i])), "CreateCommandQueue");
            Check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator[i])),
                  "CreateCommandAllocator");
            completion[i] = std::make_shared<GpuCompletion>();
            Check(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&completion[i]->fence)), "CreateFence");
        }
        Check(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gate1)), "gate1");
        Check(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gate2)), "gate2");
        auto* upload = Buffer(device, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
        auto* readback = Buffer(device, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
        uint32_t* data = nullptr;
        D3D12_RANGE noRead { 0, 0 };
        Check(upload->Map(0, &noRead, reinterpret_cast<void**>(&data)), "Map upload");
        data[0] = 123;
        ID3D12GraphicsCommandList* list = nullptr;
        Check(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator[0], nullptr, IID_PPV_ARGS(&list)),
              "CreateCommandList");
        list->CopyBufferRegion(readback, 0, upload, 0, sizeof(uint32_t));
        Check(list->Close(), "Close");
        auto recording =
#ifdef NR_LIFETIME_STANDALONE
            DlssNr::GpuLifetime::Begin(list);
#else
            std::make_shared<Recording>();
#endif
        Require(recording != nullptr, "production runtime Begin/hook failed");
        std::weak_ptr<Recording> slot = recording;
        std::weak_ptr<Recording> pool[2];
        unsigned int cursor = 0;
        Require(ClaimSlot(pool, 2, cursor, recording) >= 0, "first slot claim failed");
        Require(ClaimSlot(pool, 2, cursor, recording) >= 0, "second slot claim failed");
        Require(ClaimSlot(pool, 2, cursor, recording) < 0, "bounded descriptor pool did not exhaust");
        Hold(recording, upload);
        Hold(recording, readback);
        Hold(recording, allocator[0]);
        auto retired = std::make_shared<bool>(false);
        auto owner = std::shared_ptr<void>(new int,
                                           [retired](void* p)
                                           {
                                               *retired = true;
                                               delete static_cast<int*>(p);
                                           });
        if (!unsafeRetire)
            recording->Hold(owner.get(), owner);
        owner.reset();
        Check(queue[0]->Wait(gate1, 1), "delay queue 1");
        ID3D12CommandList* lists[] { list };
        queue[0]->ExecuteCommandLists(1, lists);
        Check(queue[0]->Signal(completion[0]->fence, 1), "signal queue 1");
#ifndef NR_LIFETIME_STANDALONE
        recording->Submit(completion[0], 1);
#endif
        Check(queue[1]->Wait(completion[0]->fence, 1), "order queue 2");
        Check(queue[1]->Wait(gate2, 1), "delay queue 2");
        queue[1]->ExecuteCommandLists(1, lists); // replay the same executable recording
        Check(queue[1]->Signal(completion[1]->fence, 1), "signal queue 2");
#ifndef NR_LIFETIME_STANDALONE
        recording->Submit(completion[1], 1);
#else
        Require(recording->submissions.size() == 2, "actual Execute/Signal hooks missed a queue submission");
#endif
        Require(!SlotAvailable(slot), "slot reusable while command list executable");
        // Reset with a different allocator while the old submissions remain queued.
        Check(list->Reset(allocator[1], nullptr), "Reset with fresh allocator");
#ifndef NR_LIFETIME_STANDALONE
        recording->active = false;
#else
        Require(!recording->active, "actual Reset hook missed the recording generation");
#endif
        recording->ReleaseCompleted();
        Require(!*retired, "EARLY RETIREMENT detected before GPU completion");
        Require(!SlotAvailable(slot) && !recording->ReadbackReady(), "pending GPU slot incorrectly available");
        if (unsafeReuse)
            data[0] = 999; // intentional in-flight upload overwrite, not an invalid GPU free
        Check(gate1->Signal(1), "release queue 1");
        Wait(completion[0]->fence, 1);
        Require(!SlotAvailable(slot) && !recording->ReadbackReady(), "second queue/replay was ignored");
        Check(gate2->Signal(1), "release queue 2");
        Wait(completion[1]->fence, 1);
        Require(SlotAvailable(slot) && recording->ReadbackReady(), "completed slot remains unavailable");
        uint32_t* result = nullptr;
        D3D12_RANGE range { 0, sizeof(uint32_t) };
        Check(readback->Map(0, &range, reinterpret_cast<void**>(&result)), "Map readback");
        const auto actual = result[0];
        readback->Unmap(0, &noRead);
        Require(actual == 123, "EARLY REUSE detected: GPU saw overwritten constants");
        recording->ReleaseCompleted();
        Require(*retired, "completed owner was not retired");
#ifdef NR_LIFETIME_STANDALONE
        // A different recording uses the previous recording's scratch result.
        // Only the production Sequence/Execute adapter supplies the cross-queue
        // dependency; the fixture does not enqueue its own queue-to-queue Wait.
        auto first = DlssNr::GpuLifetime::Begin(list);
        Require(first != nullptr, "Begin after Reset failed");
        bool featureReleased = false;
        auto* feature = new TestFeature { &featureReleased };
        DlssNr::GpuLifetime::RegisterFeature(list, feature, ReleaseTestFeature);
        Require(!DlssNr::GpuLifetime::FeatureSubmitted(feature), "feature creation counted as a submission");
        data[0] = 456;
        list->CopyBufferRegion(readback, 0, upload, 0, sizeof(uint32_t));
        Hold(first, upload);
        Hold(first, readback);
        Hold(first, allocator[1]);
        Check(list->Close(), "Close dependency producer");
        Check(queue[0]->Wait(gate1, 2), "delay dependency producer");
        queue[0]->ExecuteCommandLists(1, lists);
        Check(queue[0]->Signal(completion[0]->fence, 2), "signal producer");
        Require(DlssNr::GpuLifetime::FeatureSubmitted(feature), "feature submission hook missed create");
        DlssNr::GpuLifetime::RetireFeature(feature);
        Require(!featureReleased, "feature retired before its GPU work completed");
        ID3D12CommandAllocator* consumerAllocator = nullptr;
        ID3D12GraphicsCommandList* consumer = nullptr;
        Check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&consumerAllocator)),
              "consumer allocator");
        Check(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, consumerAllocator, nullptr,
                                        IID_PPV_ARGS(&consumer)),
              "consumer list");
        auto* consumerResult = Buffer(device, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
        // Producer destination is READBACK, which must stay COPY_DEST. Copy the
        // retained upload instead; the fence event proves dependency ordering.
        consumer->CopyBufferRegion(consumerResult, 0, upload, 0, sizeof(uint32_t));
        auto second = DlssNr::GpuLifetime::Begin(consumer);
        Require(second && DlssNr::GpuLifetime::Sequence(second, first), "submitted dependency not accepted");
        Hold(second, upload);
        Hold(second, consumerResult);
        Hold(second, consumerAllocator);
        Check(consumer->Close(), "Close consumer");
        ID3D12CommandList* consumers[] { consumer };
        queue[1]->ExecuteCommandLists(1, consumers);
        Check(queue[1]->Signal(completion[1]->fence, 2), "signal consumer");
        HANDLE ordered = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        Require(ordered != nullptr, "dependency event failed");
        Check(completion[1]->fence->SetEventOnCompletion(2, ordered), "consumer event");
        const auto beforeProducer = WaitForSingleObject(ordered, 30);
        CloseHandle(ordered);
        Require(beforeProducer == WAIT_TIMEOUT, "production cross-queue Wait was absent");
        Require(!DlssNr::GpuLifetime::DrainForShutdown(1), "shutdown ignored unfinished GPU work");
        Require(!featureReleased, "shutdown released an unfinished model owner");
        Check(gate1->Signal(2), "release dependency producer");
        Wait(completion[0]->fence, 2);
        Wait(completion[1]->fence, 2);
        Require(DlssNr::GpuLifetime::DrainForShutdown(), "completed SDK shutdown failed to drain");
        Require(featureReleased, "model owner was not released before SDK shutdown");
        Check(consumerResult->Map(0, &range, reinterpret_cast<void**>(&result)), "consumer readback");
        Require(result[0] == 456, "dependency consumer got incorrect GPU data");
        consumerResult->Unmap(0, &noRead);
        consumerResult->Release();
        consumer->Release();
        consumerAllocator->Release();
        // Final Release must close an unsubmitted recording without CPU mocks.
        bool discardedFeatureReleased = false;
        auto* discardedFeature = new TestFeature { &discardedFeatureReleased };
        Require(DlssNr::GpuLifetime::Begin(list) != nullptr, "Begin after SDK drain failed");
        DlssNr::GpuLifetime::RegisterFeature(list, discardedFeature, ReleaseTestFeature);
        DlssNr::GpuLifetime::RetireFeature(discardedFeature);
        list->Release();
        list = nullptr;
        Require(discardedFeatureReleased, "actual final Release failed to retire discarded feature");
        std::puts("PASS: actual Reset/Release/Execute/Signal hooks, cross-queue dependency, model-owner retirement and "
                  "SDK drain");
#endif
        // Reset of an unsubmitted generation frees ownership without claiming a readback.
        auto discarded = std::make_shared<Recording>();
        discarded->active = false;
        Require(discarded->Reusable() && !discarded->ReadbackReady(), "discarded generation mismatch");
        if (list)
            Check(list->Close(), "Close new recording");
        upload->Unmap(0, nullptr);
        upload->Release();
        readback->Release();
        if (list)
            list->Release();
        for (unsigned int i = 0; i < 2; ++i)
        {
            allocator[i]->Release();
            queue[i]->Release();
        }
        gate1->Release();
        gate1 = nullptr;
        gate2->Release();
        gate2 = nullptr;
        device->Release();
        std::puts("PASS: real GPU delay, slot exhaustion, Reset, replay/two queues, readback and retirement");
        return 0;
    }
    catch (const std::exception& error)
    {
        // Unblock deliberately delayed queues even on a failed assertion.
        if (gate1)
            gate1->Signal(2);
        if (gate2)
            gate2->Signal(2);
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        std::printf("FAIL: %s\n", error.what());
        return 1;
    }
}
