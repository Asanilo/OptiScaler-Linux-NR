#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_set>

namespace DlssNr::Lifetime
{
// Implemented by a real D3D12 fence in production and in the GPU harness.
struct Completion
{
    virtual ~Completion() = default;
    virtual uint64_t Value() const = 0;
};

struct Submission
{
    std::shared_ptr<Completion> fence;
    uint64_t value = 0;
};

// One command-list recording, not one Evaluate call or one Present. Keep its
// resources while the executable recording exists: it may be submitted again.
// Reset/Release closes recording ownership; every submission must then finish.
// Callers serialize access, including reading fence completion and slot claims.
struct Recording
{
    bool active = true;
    bool submitted = false;
    bool poisoned = false;
    unsigned int pendingSubmissions = 0;
    uint64_t timestampFrequency = 0;
    bool timestampFrequencyValid = true;
    std::vector<Submission> submissions;
    std::vector<std::weak_ptr<Recording>> dependencies;
    std::vector<std::shared_ptr<void>> retained;
    std::unordered_set<const void*> keys;

    void Hold(const void* key, std::shared_ptr<void> resource)
    {
        if (resource && keys.insert(key).second)
            retained.push_back(std::move(resource));
    }

    void Submit(std::shared_ptr<Completion> fence, uint64_t value)
    {
        submitted = true;
        if (!fence || value == 0)
        {
            poisoned = true;
            return;
        }
        // A repeated submission to one queue extends its completion requirement.
        for (auto& entry : submissions)
        {
            if (entry.fence == fence)
            {
                if (value > entry.value)
                    entry.value = value;
                return;
            }
        }
        submissions.push_back({ std::move(fence), value });
    }

    bool GpuComplete() const
    {
        if (poisoned || pendingSubmissions != 0)
            return false;
        for (const auto& entry : submissions)
        {
            const auto completed = entry.fence->Value();
            // D3D12 returns UINT64_MAX on device removal. That is not completion.
            if (completed == UINT64_MAX || completed < entry.value)
                return false;
        }
        return true;
    }

    bool Reusable() const { return !active && GpuComplete(); }
    bool ReadbackReady() const { return submitted && Reusable(); }

    void ReleaseCompleted()
    {
        if (Reusable())
        {
            keys.clear();
            retained.clear();
        }
    }
};

using Token = std::shared_ptr<Recording>;

inline bool SlotAvailable(const std::weak_ptr<Recording>& slot)
{
    const auto previous = slot.lock();
    return !previous || previous->Reusable();
}

inline int ClaimSlot(std::weak_ptr<Recording>* slots, unsigned int count, unsigned int& cursor, const Token& recording)
{
    for (unsigned int n = 0; n < count; ++n)
    {
        const auto slot = (cursor + n) % count;
        if (SlotAvailable(slots[slot]))
        {
            slots[slot] = recording;
            cursor = (slot + 1) % count;
            return static_cast<int>(slot);
        }
    }
    return -1;
}
} // namespace DlssNr::Lifetime
