#pragma once

#include "SubmissionLifetime.h"
#include <d3d12.h>

namespace DlssNr::GpuLifetime
{
using Token = Lifetime::Token;
using ReleaseFeature = void(__cdecl*)(void*);

// Installs Reset/Release tracking on the real list and a lightweight queue
// submission hook even when FG is off. Null means NR must leave the game alone.
Token Begin(ID3D12GraphicsCommandList* list);
void Hold(const Token& token, IUnknown* object);
void Hold(ID3D12GraphicsCommandList* list, IUnknown* object);
void RegisterFeature(ID3D12GraphicsCommandList* list, void* feature, ReleaseFeature release);
bool HoldFeature(ID3D12GraphicsCommandList* list, void* feature);
void RetireFeature(void* feature);
bool FeatureSubmitted(void* feature);
bool FeatureDiscarded(void* feature);
bool Sequence(const Token& current, const Token& previous);
bool Reusable(const std::weak_ptr<Lifetime::Recording>& token);
bool ReadbackReady(const Token& token);
uint64_t TimestampFrequency(const Token& token);
int ClaimSlot(std::weak_ptr<Lifetime::Recording>* slots, unsigned int count,
              unsigned int& cursor, const Token& recording);
void Poll();

// Pin the recordings before Execute, so concurrent Reset cannot retire them in
// the gap between Execute and Signal. Complete on both FG/non-FG return paths.
std::vector<Token> Submitting(ID3D12CommandQueue* queue, UINT count, ID3D12CommandList* const* lists);
void Submitted(ID3D12CommandQueue* queue, const std::vector<Token>& batch);
const char* FailureReason();
} // namespace DlssNr::GpuLifetime
