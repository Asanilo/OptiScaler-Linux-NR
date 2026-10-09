#include "pch.h"
#include "NrGpuLifetime.h"

#include <Util.h>
#include <resource_tracking/ResTrack_dx12.h>
#include <detours/detours.h>
#include <mutex>
#include <unordered_map>
#include <algorithm>

namespace DlssNr::GpuLifetime
{
namespace
{
struct Fence final : Lifetime::Completion
{
    ID3D12Fence* fence = nullptr;
    ID3D12CommandQueue* queue = nullptr;
    uint64_t next = 0;
    ~Fence() override { if (fence) fence->Release(); if (queue) queue->Release(); }
    uint64_t Value() const override { return fence->GetCompletedValue(); }
};
struct Feature
{
    std::shared_ptr<void> owner;
    Token builtOn;
    bool submitted = false;
};

// Release hooks may run while retained COM references are being dropped.
std::recursive_mutex mutex;
std::unordered_map<void*, Token> recordings;
std::vector<Token> closed;
std::unordered_map<void*, Feature> features;
std::unordered_map<ID3D12CommandQueue*, std::shared_ptr<Fence>> fences;
const char* failure = "";
using ResetFn = HRESULT(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, ID3D12CommandAllocator*, ID3D12PipelineState*);
using ReleaseFn = ULONG(STDMETHODCALLTYPE*)(IUnknown*);
ResetFn originalReset = nullptr;
ReleaseFn originalRelease = nullptr;
void* resetAddress = nullptr;
void* releaseAddress = nullptr;

void* Real(IUnknown* object)
{
    if (!object) return nullptr;
    // Same unwrapping IID as Util; keep this hot path free of per-dispatch logs.
    static const GUID streamline { 0xadec44e2, 0x61f0, 0x45c3,
                                   { 0xad, 0x9f, 0x1b, 0x37, 0x37, 0x92, 0x84, 0xff } };
    IUnknown* real = nullptr;
    if (SUCCEEDED(object->QueryInterface(streamline, reinterpret_cast<void**>(&real))) && real)
    {
        real->Release();
        return real;
    }
    return object;
}

void Close(void* list)
{
    const auto found = recordings.find(list);
    if (found == recordings.end())
        return;
    found->second->active = false;
    closed.push_back(std::move(found->second));
    recordings.erase(found);
}

HRESULT STDMETHODCALLTYPE ResetHook(ID3D12GraphicsCommandList* list,
                                    ID3D12CommandAllocator* allocator, ID3D12PipelineState* state)
{
    const auto result = originalReset(list, allocator, state);
    if (SUCCEEDED(result))
    {
        std::lock_guard<std::recursive_mutex> lock(mutex);
        Close(list);
    }
    return result;
}

ULONG STDMETHODCALLTYPE ReleaseHook(IUnknown* list)
{
    // Identity only after final Release; never dereference a destroyed object.
    const auto count = originalRelease(list);
    if (count == 0)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex);
        Close(list);
    }
    return count;
}

bool Install(ID3D12GraphicsCommandList* list)
{
    auto** vtable = *reinterpret_cast<void***>(Real(list));
    if (originalReset)
        return resetAddress == vtable[10] && releaseAddress == vtable[2];
    resetAddress = vtable[10];
    releaseAddress = vtable[2];
    originalReset = reinterpret_cast<ResetFn>(resetAddress);
    originalRelease = reinterpret_cast<ReleaseFn>(releaseAddress);
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(reinterpret_cast<PVOID*>(&originalReset), ResetHook);
    DetourAttach(reinterpret_cast<PVOID*>(&originalRelease), ReleaseHook);
    if (DetourTransactionCommit() != NO_ERROR)
    {
        originalReset = nullptr;
        originalRelease = nullptr;
        failure = "command-list Reset/Release hooks failed";
        return false;
    }
    return true;
}
} // namespace

void Poll()
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    for (auto it = closed.begin(); it != closed.end();)
    {
        if ((*it)->Reusable())
        {
            (*it)->ReleaseCompleted();
            it = closed.erase(it);
        }
        else
            ++it;
    }
}

Token Begin(ID3D12GraphicsCommandList* list)
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    Poll();
    if (!list || *failure)
        return nullptr;
    if (list->GetType() != D3D12_COMMAND_LIST_TYPE_DIRECT && list->GetType() != D3D12_COMMAND_LIST_TYPE_COMPUTE)
    {
        failure = "NR requires a direct/compute command list; bundles are unsupported";
        return nullptr;
    }
    if (!Install(list))
    {
        failure = "command-list implementation changed; lifetime tracking unavailable";
        return nullptr;
    }
    ID3D12Device* device = nullptr;
    if (FAILED(list->GetDevice(IID_PPV_ARGS(&device))))
    {
        failure = "could not obtain the command-list device";
        return nullptr;
    }
    const bool hooked = ResTrack_Dx12::EnsureQueueSubmissionHook(device);
    device->Release();
    if (!hooked)
    {
        failure = "queue submission hook unavailable";
        return nullptr;
    }
    auto& token = recordings[Real(list)];
    if (!token)
        token = std::make_shared<Lifetime::Recording>();
    // A queue that stops answering fences must not grow retained state forever.
    if (closed.size() > 256 || recordings.size() > 256)
    {
        failure = "too many unfinished NR recordings; disabling NR safely";
        return nullptr;
    }
    return token;
}

void Hold(const Token& token, IUnknown* object)
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (!token || !object || token->keys.count(object))
        return;
    object->AddRef();
    token->Hold(object, std::shared_ptr<void>(object, [](void* value) {
        static_cast<IUnknown*>(value)->Release();
    }));
}

void Hold(ID3D12GraphicsCommandList* list, IUnknown* object)
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    const auto found = recordings.find(Real(list));
    if (found != recordings.end())
        Hold(found->second, object);
}

void RegisterFeature(ID3D12GraphicsCommandList* list, void* feature, ReleaseFeature release)
{
    if (!feature || !release)
        return;
    std::lock_guard<std::recursive_mutex> lock(mutex);
    auto& entry = features[feature];
    entry.owner = std::shared_ptr<void>(feature, release);
    const auto found = recordings.find(Real(list));
    if (found != recordings.end())
    {
        entry.builtOn = found->second;
        found->second->Hold(feature, entry.owner);
    }
}

bool HoldFeature(ID3D12GraphicsCommandList* list, void* feature)
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    const auto recording = recordings.find(Real(list));
    const auto entry = features.find(feature);
    if (recording == recordings.end() || entry == features.end())
        return false;
    recording->second->Hold(feature, entry->second.owner);
    return true;
}

void RetireFeature(void* feature)
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    features.erase(feature); // recordings and every submission retain their owner
}

bool FeatureSubmitted(void* feature)
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    const auto entry = features.find(feature);
    if (entry == features.end())
        return false;
    if (const auto created = entry->second.builtOn)
        entry->second.submitted |= created->submitted && !created->poisoned;
    return entry->second.submitted;
}

bool FeatureDiscarded(void* feature)
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    const auto entry = features.find(feature);
    return entry != features.end() && entry->second.builtOn &&
           !entry->second.builtOn->active && !entry->second.builtOn->submitted &&
           entry->second.builtOn->pendingSubmissions == 0;
}

bool Sequence(const Token& current, const Token& previous)
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (!previous || current == previous || previous->Reusable())
        return true;
    if (!previous->submitted || previous->pendingSubmissions || previous->poisoned)
        return false;
    current->dependencies.push_back(previous);
    return true;
}

bool Reusable(const std::weak_ptr<Lifetime::Recording>& token)
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    return Lifetime::SlotAvailable(token);
}

bool ReadbackReady(const Token& token)
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    return token && token->ReadbackReady();
}

uint64_t TimestampFrequency(const Token& token)
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    return token && token->timestampFrequencyValid ? token->timestampFrequency : 0;
}

int ClaimSlot(std::weak_ptr<Lifetime::Recording>* slots, unsigned int count,
              unsigned int& cursor, const Token& recording)
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    return Lifetime::ClaimSlot(slots, count, cursor, recording);
}

std::vector<Token> Submitting(ID3D12CommandQueue* queue, UINT count, ID3D12CommandList* const* lists)
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    std::vector<Token> used;
    for (UINT i = 0; i < count; ++i)
    {
        const auto found = recordings.find(Real(lists[i]));
        if (found != recordings.end())
        {
            ++found->second->pendingSubmissions;
            used.push_back(found->second);
            for (const auto& weak : found->second->dependencies)
            {
                if (const auto dependency = weak.lock())
                    for (const auto& previous : dependency->submissions)
                    {
                        const auto fence = std::static_pointer_cast<Fence>(previous.fence);
                        if (fence->queue != queue && FAILED(queue->Wait(fence->fence, previous.value)))
                        {
                            failure = "cross-queue NR dependency wait failed";
                            found->second->poisoned = true;
                        }
                    }
            }
        }
    }
    return used;
}

void Submitted(ID3D12CommandQueue* queue, const std::vector<Token>& used)
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (used.empty())
        return;
    auto& fence = fences[queue];
    if (!fence)
    {
        fence = std::make_shared<Fence>();
        queue->AddRef();
        fence->queue = queue;
        ID3D12Device* device = nullptr;
        if (FAILED(queue->GetDevice(IID_PPV_ARGS(&device))) || !device)
            failure = "could not obtain submission queue device";
        else
        {
            if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence->fence))))
                failure = "could not create NR completion fence";
            device->Release();
        }
    }
    const auto value = ++fence->next;
    if (!fence->fence || FAILED(queue->Signal(fence->fence, value)))
    {
        failure = "NR completion signal failed; retaining unfinished resources";
        for (const auto& token : used)
        {
            token->Submit(nullptr, 0);
            --token->pendingSubmissions;
        }
        LOG_ERROR("DLSS-NR {}", failure);
        return;
    }
    for (const auto& token : used)
    {
        UINT64 frequency = 0;
        if (FAILED(queue->GetTimestampFrequency(&frequency)) || frequency == 0 ||
            (token->timestampFrequency && token->timestampFrequency != frequency))
            token->timestampFrequencyValid = false;
        token->timestampFrequency = frequency;
        token->Submit(fence, value);
        --token->pendingSubmissions;
    }
}

const char* FailureReason()
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    return failure;
}
} // namespace DlssNr::GpuLifetime
