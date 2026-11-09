// Camada 4 — política de substituição por região (11, 12).
#pragma once

#include "common/common.hpp"

#include <list>
#include <memory>
#include <unordered_map>

namespace cbd {

class ReplacementPolicy {
public:
    virtual ~ReplacementPolicy() = default;
    virtual void on_access(FrameId) = 0;            // fixada: sai das candidatas
    virtual void on_unpin(FrameId, Intent) = 0;     // pins chegaram a zero: volta às candidatas
    virtual void on_remove(FrameId) = 0;            // esvaziada fora da política
    virtual std::optional<FrameId> pick_victim() = 0;
    virtual std::size_t candidates() const = 0;
};

std::unique_ptr<ReplacementPolicy> make_policy(Policy, bool toss_immediate);

}  // namespace cbd
