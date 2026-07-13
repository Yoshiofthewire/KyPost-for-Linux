#pragma once

#include "domain/DevicePairing.h"

#include <optional>

class SecureStore;

// The one shared contract for "are we paired, and with what credentials",
// so every Phase 4 repository that needs pairing state reads it here rather
// than re-deriving SecureStore key names independently.
class PairingStore
{
public:
    explicit PairingStore(SecureStore& secureStore);

    // nullopt when never paired -- specifically, when "sub" is absent.
    // Other fields default to empty QString if individually missing
    // (defensive; save() always writes all seven keys together so this
    // should not happen in practice).
    std::optional<DevicePairing> load() const;

    // Writes all seven keys. Returns false if any individual
    // SecureStore::set() call fails (matches SecureStore's own
    // bool-returning contract) -- on false, some keys may have been
    // written and others not; callers should treat this as "pairing
    // state is now indeterminate", not attempt partial rollback.
    bool save(const DevicePairing& pairing);

    // Removes all seven keys.
    void clear();

    bool isPaired() const; // load().has_value()

private:
    SecureStore& m_secureStore;
};
