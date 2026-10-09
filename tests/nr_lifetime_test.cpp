#include "../OptiScaler/dlssnr/SubmissionLifetime.h"
#include <cassert>
#include <iostream>

using namespace DlssNr::Lifetime;
struct CpuCompletion : Completion
{
    uint64_t completed = 0;
    uint64_t Value() const override { return completed; }
};

int main()
{
    auto first = std::make_shared<CpuCompletion>();
    auto second = std::make_shared<CpuCompletion>();
    auto generation = std::make_shared<Recording>();
    std::weak_ptr<Recording> slot = generation;
    bool released = false;
    auto resource = std::shared_ptr<void>(new int(1),
                                          [&](void* value)
                                          {
                                              released = true;
                                              delete static_cast<int*>(value);
                                          });
    generation->Hold(resource.get(), resource);
    resource.reset();
    assert(!SlotAvailable(slot)); // open recording
    generation->Submit(first, 1);
    first->completed = 1;
    assert(!SlotAvailable(slot));  // closed-but-executable lists can be replayed
    generation->Submit(first, 2);  // later submission extends ownership
    generation->Submit(second, 3); // every queue must finish
    generation->active = false;
    generation->ReleaseCompleted();
    assert(!released && !SlotAvailable(slot));
    first->completed = 2;
    assert(!generation->ReadbackReady());
    second->completed = 3;
    assert(SlotAvailable(slot) && generation->ReadbackReady());
    generation->ReleaseCompleted();
    assert(released);

    auto pending = std::make_shared<Recording>();
    pending->active = false;
    pending->pendingSubmissions = 1;
    assert(!pending->Reusable()); // Reset in the Execute-to-Signal gap
    pending->Submit(first, 4);
    --pending->pendingSubmissions;
    assert(!pending->Reusable());
    first->completed = 4;
    assert(pending->Reusable());
    first->completed = UINT64_MAX;
    assert(!pending->Reusable()); // device removal never counts as success

    auto failed = std::make_shared<Recording>();
    failed->active = false;
    failed->Submit(nullptr, 0);
    assert(!failed->Reusable() && !failed->ReadbackReady());
    auto discarded = std::make_shared<Recording>();
    discarded->active = false;
    assert(discarded->Reusable() && !discarded->ReadbackReady());
    std::cout << "NR recording/submission ownership contracts passed (CPU only)\n";
}
