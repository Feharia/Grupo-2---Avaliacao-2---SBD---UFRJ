#include "layer4/replacement_policy.hpp"

namespace cbd {

namespace {

// Lista ordenada pelo momento em que a moldura ficou livre de pins: frente = mais antiga.
// LRU escolhe a frente; MRU escolhe o fim. Toss-immediate põe a moldura na ponta da vítima.
class ListPolicy final : public ReplacementPolicy {
public:
    ListPolicy(bool mru, bool toss) : mru_(mru), toss_(toss) {}

    void on_access(FrameId f) override { erase(f); }
    void on_remove(FrameId f) override { erase(f); }

    void on_unpin(FrameId f, Intent intent) override {
        erase(f);
        const bool victim_next = toss_ && !intent.reuse;
        if (victim_next) {
            if (mru_) order_.push_back(f); else order_.push_front(f);
            pos_[f] = mru_ ? std::prev(order_.end()) : order_.begin();
        } else {
            order_.push_back(f);
            pos_[f] = std::prev(order_.end());
        }
    }

    std::optional<FrameId> pick_victim() override {
        if (order_.empty()) return std::nullopt;
        const FrameId f = mru_ ? order_.back() : order_.front();
        erase(f);
        return f;
    }

    std::size_t candidates() const override { return order_.size(); }

private:
    void erase(FrameId f) {
        auto it = pos_.find(f);
        if (it == pos_.end()) return;
        order_.erase(it->second);
        pos_.erase(it);
    }
    bool mru_, toss_;
    std::list<FrameId> order_;
    std::unordered_map<FrameId, std::list<FrameId>::iterator> pos_;
};

}  // namespace

std::unique_ptr<ReplacementPolicy> make_policy(Policy p, bool toss) {
    return std::make_unique<ListPolicy>(p == Policy::Mru, toss);
}

}  // namespace cbd
