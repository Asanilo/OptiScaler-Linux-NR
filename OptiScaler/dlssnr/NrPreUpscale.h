#pragma once

namespace DlssNr
{
// Placement is decided before NR creates its feature. A creation/skip frame must still stay on the
// pre-upscale path, otherwise the post pass rebuilds the same feature at display resolution.
constexpr bool UsePreUpscale(bool enabled, bool preUpscale, bool dualFeature, bool superSampling)
{
    return enabled && preUpscale && !dualFeature && superSampling;
}

// NGX has both resource-typed and void* parameter accessors. Preserve the accessor that supplied
// Color, and restore it on every exit from the upscaler, including failed evaluations.
// Kept independent of D3D12 so the parameter contract can be regression-tested on Linux.
template <typename Resource, typename Parameters, auto Success> class ScopedColorSubstitution
{
  public:
    ScopedColorSubstitution(Parameters* params, const char* key) : _params(params), _key(key)
    {
        if (_params == nullptr)
            return;

        if (_params->Get(_key, &_original) == Success && _original != nullptr)
            return;

        _original = nullptr;
        _typed = false;
        void* untyped = nullptr;
        if (_params->Get(_key, &untyped) == Success)
            _original = static_cast<Resource*>(untyped);
    }

    ScopedColorSubstitution(const ScopedColorSubstitution&) = delete;
    ScopedColorSubstitution& operator=(const ScopedColorSubstitution&) = delete;

    Resource* Original() const { return _original; }

    bool Replace(Resource* edited)
    {
        if (_original == nullptr || edited == nullptr)
            return false;

        Set(edited);
        _substituted = true;
        return true;
    }

    ~ScopedColorSubstitution()
    {
        if (_substituted)
            Set(_original);
    }

  private:
    void Set(Resource* value)
    {
        if (_typed)
            _params->Set(_key, value);
        else
            _params->Set(_key, static_cast<void*>(value));
    }

    Parameters* _params = nullptr;
    const char* _key = nullptr;
    Resource* _original = nullptr;
    bool _typed = true;
    bool _substituted = false;
};
} // namespace DlssNr
