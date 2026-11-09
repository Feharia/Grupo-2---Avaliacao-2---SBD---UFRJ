#include "layer3/bplus.hpp"

#include <algorithm>
#include <cmath>

namespace cbd {

BPlusEngine::BPlusEngine(BufferPool& pool, FileId leaf_file, FileId node_file, KeyCodec kc, KeyOf key_of,
                         bool unique, PageType leaf_type, std::uint32_t fixed_record)
    : pool_(pool), leaf_file_(leaf_file), node_file_(node_file), codec_(kc), key_of_(std::move(key_of)),
      unique_(unique), leaf_type_(leaf_type), fixed_(fixed_record) {
    const std::uint32_t bs = pool_.block_size();
    leaf_cap_ = fixed_ ? sp_capacity(bs, fixed_) : 0;
    int_cap_ = internal_capacity(bs, codec_);
    if (int_cap_ < 3) throw std::invalid_argument("chave grande demais para o nó interno");
    min_int_ = int_cap_ / 2;
}

PageGuard BPlusEngine::leaf(std::uint32_t b) const {
    return pool_.fetch(BlockId{leaf_file_, b}, kRandomReuse, kNoReservation);
}
PageGuard BPlusEngine::node(std::uint32_t b) const {
    return pool_.fetch(BlockId{node_file_, b}, kRandomReuse, kNoReservation);
}
InternalNode BPlusEngine::load_node(std::uint32_t b) const {
    auto g = node(b);
    return read_internal(g.read(), codec_);
}
void BPlusEngine::save_node(std::uint32_t b, const InternalNode& n) {
    auto g = node(b);
    write_internal(g.write(), n, codec_);
}

void BPlusEngine::init_empty() {
    auto [bid, g] = pool_.allocate(leaf_file_, kNoReservation);
    sp_init(g.write(), leaf_type_);
    meta_ = Meta{bid.block, true, 1, bid.block};
}

// ----------------------------------------------------------------------- busca
std::uint32_t BPlusEngine::leaf_for(const std::optional<Key>& lo) const {
    if (meta_.root_is_leaf) return meta_.root;
    std::uint32_t b = meta_.root;
    for (;;) {
        const InternalNode n = load_node(b);
        std::size_t i = 0;
        if (lo) while (i < n.keys.size() && compare_keys(n.keys[i], *lo) < 0) ++i;   // chaves < lo
        if (n.level == 1) return n.children[i];
        b = n.children[i];
    }
}

std::uint32_t BPlusEngine::descend(const Key& k, Path& path, bool rightmost) const {
    path.clear();
    if (meta_.root_is_leaf) return meta_.root;
    std::uint32_t b = meta_.root;
    for (;;) {
        const InternalNode n = load_node(b);
        std::size_t i = 0;
        while (i < n.keys.size()) {
            const int c = compare_keys(n.keys[i], k);
            if (rightmost ? c <= 0 : c < 0) ++i; else break;
        }
        path.emplace_back(b, i);
        if (n.level == 1) return n.children[i];
        b = n.children[i];
    }
}

std::optional<RID> BPlusEngine::find_first(const Key& k) const {
    std::uint32_t b = leaf_for(k);
    while (b != 0) {
        auto g = leaf(b);
        const auto page = g.read();
        for (const auto& [key, slot] : sorted_slots(page, key_of_)) {
            const int c = compare_keys(key, k);
            if (c == 0) return RID{b, slot};
            if (c > 0) return std::nullopt;
        }
        b = read_header(page).next;
    }
    return std::nullopt;
}

bool BPlusEngine::find_path(std::uint32_t b, const Key& k, std::uint32_t target, Path& path) const {
    const InternalNode n = load_node(b);
    const std::size_t m = n.keys.size();
    for (std::size_t i = 0; i <= m; ++i) {
        if (i > 0 && compare_keys(n.keys[i - 1], k) > 0) break;     // limite inferior do filho i
        if (i < m && compare_keys(k, n.keys[i]) > 0) continue;      // limite superior do filho i
        path.emplace_back(b, i);
        if (n.level == 1) {
            if (n.children[i] == target) return true;
        } else if (find_path(n.children[i], k, target, path)) {
            return true;
        }
        path.pop_back();
    }
    return false;
}

// ----------------------------------------------------------------------- inserção
RID BPlusEngine::insert(std::span<const std::byte> rec) {
    const Key k = key_of_(rec);
    if (unique_ && find_first(k)) throw DuplicateKey("chave duplicada: " + key_to_string(k));
    for (int attempt = 0; attempt < 4; ++attempt) {
        Path path;
        const std::uint32_t lb = descend(k, path, true);
        {
            auto g = leaf(lb);
            if (auto s = sp_insert(g.write(), rec)) return RID{lb, *s};
        }
        split_leaf(lb, std::move(path), k);
    }
    throw std::logic_error("B+: registro não coube depois de dividir a folha");
}

void BPlusEngine::split_leaf(std::uint32_t lb, Path path, const Key& k_new) {
    std::vector<std::pair<Key, std::uint16_t>> order;
    std::uint32_t total = 0;
    std::vector<std::uint32_t> lens;
    {
        auto g = leaf(lb);
        order = sorted_slots(g.read(), key_of_);
        for (const auto& e : order) {
            lens.push_back(static_cast<std::uint32_t>(sp_record(g.read(), e.second).size()) + kSlotEntry);
            total += lens.back();
        }
    }
    if (order.size() < 2) throw std::logic_error("B+: folha com um único registro não pode ser dividida");
    std::size_t mid = 0;
    Key sep;
    if (fixed_) {
        // Divide os n + 1 registros (contando o novo): ⌈(n+1)/2⌉ ficam à esquerda (37).
        // O separador é escolhido para que a descida pela direita leve o novo ao lado certo.
        const std::size_t n = order.size();
        const std::size_t L = (n + 2) / 2;
        if (L >= n) {
            mid = n - 1;
            sep = order[mid].first;
        } else if (compare_keys(k_new, order[L - 1].first) < 0) {
            mid = L - 1;                       // o novo fica à esquerda
            sep = order[mid].first;
        } else {
            mid = L;                           // o novo vai para a direita
            sep = compare_keys(k_new, order[L].first) < 0 ? k_new : order[L].first;
        }
    } else {
        for (std::uint32_t acc = 0; mid < order.size() && acc + lens[mid] <= total / 2; ++mid) acc += lens[mid];
        mid = std::clamp<std::size_t>(mid, 1, order.size() - 1);
        sep = order[mid].first;
    }

    auto [nbid, ng] = pool_.allocate(leaf_file_, kNoReservation);
    const std::uint32_t nb = nbid.block;
    sp_init(ng.write(), leaf_type_);
    std::uint32_t old_next = 0;
    {
        auto g = leaf(lb);
        for (std::size_t i = mid; i < order.size(); ++i) {
            const auto rec = sp_record(g.read(), order[i].second);
            std::vector<std::byte> copy(rec.begin(), rec.end());
            const auto ns = sp_insert(ng.write(), copy);
            if (!ns) throw std::logic_error("B+: metade da folha não coube na nova folha");
            sp_erase(g.write(), order[i].second);
            if (on_move_) on_move_(RID{lb, order[i].second}, RID{nb, *ns}, sp_record(ng.read(), *ns));
        }
        PageHeader oh = read_header(g.read());
        old_next = oh.next;
        oh.next = nb;
        write_header(g.write(), oh);
    }
    PageHeader nh = read_header(ng.read());
    nh.next = old_next;
    nh.prev = lb;
    write_header(ng.write(), nh);
    ng.reset();
    if (old_next != 0) {
        auto g = leaf(old_next);
        PageHeader h = read_header(g.read());
        h.prev = nb;
        write_header(g.write(), h);
    }
    insert_into_parent(std::move(path), lb, sep, nb);
}

void BPlusEngine::insert_into_parent(Path path, std::uint32_t left, const Key& sep, std::uint32_t right) {
    if (path.empty()) {   // a esquerda era a raiz: nova raiz
        InternalNode n;
        n.level = meta_.root_is_leaf ? 1 : static_cast<std::uint16_t>(load_node(left).level + 1);
        n.keys = {sep};
        n.children = {left, right};
        auto [bid, g] = pool_.allocate(node_file_, kNoReservation);
        write_internal(g.write(), n, codec_);
        meta_.root = bid.block;
        meta_.root_is_leaf = false;
        ++meta_.height;
        return;
    }
    const auto [pb, idx] = path.back();
    path.pop_back();
    InternalNode n = load_node(pb);
    n.keys.insert(n.keys.begin() + static_cast<std::ptrdiff_t>(idx), sep);
    n.children.insert(n.children.begin() + static_cast<std::ptrdiff_t>(idx) + 1, right);
    if (n.keys.size() <= int_cap_) {
        save_node(pb, n);
        return;
    }
    const std::size_t mid = n.keys.size() / 2;   // divide o nó interno e promove a chave do meio
    const Key up = n.keys[mid];
    InternalNode r;
    r.level = n.level;
    r.keys.assign(n.keys.begin() + static_cast<std::ptrdiff_t>(mid) + 1, n.keys.end());
    r.children.assign(n.children.begin() + static_cast<std::ptrdiff_t>(mid) + 1, n.children.end());
    n.keys.resize(mid);
    n.children.resize(mid + 1);
    save_node(pb, n);
    auto [rbid, rg] = pool_.allocate(node_file_, kNoReservation);
    write_internal(rg.write(), r, codec_);
    rg.reset();
    insert_into_parent(std::move(path), pb, up, rbid.block);
}

RID BPlusEngine::replace(RID rid, std::span<const std::byte> rec) {
    {
        auto g = leaf(rid.block);
        if (sp_replace(g.write(), rid.slot, rec)) return rid;
    }
    erase(rid);
    return insert(rec);
}

// ----------------------------------------------------------------------- remoção
bool BPlusEngine::leaf_ok(std::uint32_t b) const {
    auto g = leaf(b);
    if (fixed_) return read_header(g.read()).live >= (leaf_cap_ + 1) / 2;   // ⌈n/2⌉ (37)
    return sp_used_bytes(g.read()) * 2 >= (pool_.block_size() - kPageHeader);
}

bool BPlusEngine::leaf_ok_after_removing(std::uint32_t b, std::size_t len) const {
    auto g = leaf(b);
    if (fixed_) return read_header(g.read()).live >= (leaf_cap_ + 1) / 2 + 1;
    return (sp_used_bytes(g.read()) - len - kSlotEntry) * 2 >= (pool_.block_size() - kPageHeader);
}

bool BPlusEngine::move_record(std::uint32_t from_b, std::uint16_t slot, std::uint32_t to_b) {
    auto gf = leaf(from_b);
    auto gt = leaf(to_b);
    const auto rec = sp_record(gf.read(), slot);
    std::vector<std::byte> copy(rec.begin(), rec.end());
    const auto ns = sp_insert(gt.write(), copy);
    if (!ns) return false;
    sp_erase(gf.write(), slot);
    if (on_move_) on_move_(RID{from_b, slot}, RID{to_b, *ns}, sp_record(gt.read(), *ns));
    return true;
}

Key BPlusEngine::min_key(std::uint32_t lb) const {
    auto g = leaf(lb);
    auto order = sorted_slots(g.read(), key_of_);
    if (order.empty()) throw std::logic_error("B+: folha vazia sem chave mínima");
    return order.front().first;
}

void BPlusEngine::unlink_leaf(std::uint32_t b) {
    PageHeader h;
    {
        auto g = leaf(b);
        h = read_header(g.read());
    }
    if (h.prev) {
        auto g = leaf(h.prev);
        PageHeader p = read_header(g.read());
        p.next = h.next;
        write_header(g.write(), p);
    }
    if (h.next) {
        auto g = leaf(h.next);
        PageHeader n = read_header(g.read());
        n.prev = h.prev;
        write_header(g.write(), n);
    }
    pool_.free_block(BlockId{leaf_file_, b});   // B+ libera folhas fundidas (162)
}

void BPlusEngine::erase(RID rid) {
    Key k;
    {
        auto g = leaf(rid.block);
        const auto r = sp_record(g.read(), rid.slot);
        if (r.empty()) throw std::logic_error("B+ erase: registro inexistente");
        k = key_of_(r);
    }
    Path path;
    if (!meta_.root_is_leaf && !find_path(meta_.root, k, rid.block, path))
        throw std::logic_error("B+ erase: folha do RID não encontrada pela chave");
    {
        auto g = leaf(rid.block);
        sp_erase(g.write(), rid.slot);
    }
    if (!meta_.root_is_leaf) fix_leaf(rid.block, std::move(path));
}

void BPlusEngine::fix_leaf(std::uint32_t lb, Path path) {
    if (leaf_ok(lb)) return;
    const auto [pb, idx] = path.back();
    InternalNode p = load_node(pb);
    // 1) redistribuição com o irmão da direita: traz os menores
    if (idx + 1 < p.children.size()) {
        const std::uint32_t rb = p.children[idx + 1];
        bool moved = false;
        while (!leaf_ok(lb)) {
            std::vector<std::pair<Key, std::uint16_t>> order;
            std::size_t len;
            {
                auto g = leaf(rb);
                order = sorted_slots(g.read(), key_of_);
                if (order.empty()) break;
                len = sp_record(g.read(), order.front().second).size();
            }
            if (!leaf_ok_after_removing(rb, len)) break;
            if (!move_record(rb, order.front().second, lb)) break;
            moved = true;
        }
        if (moved && leaf_ok(lb)) {
            p.keys[idx] = min_key(rb);
            save_node(pb, p);
            return;
        }
        if (moved) {
            p.keys[idx] = min_key(rb);
            save_node(pb, p);
        }
    }
    // 2) redistribuição com o irmão da esquerda: traz os maiores
    if (idx > 0 && !leaf_ok(lb)) {
        const std::uint32_t lsib = p.children[idx - 1];
        bool moved = false;
        while (!leaf_ok(lb)) {
            std::vector<std::pair<Key, std::uint16_t>> order;
            std::size_t len;
            {
                auto g = leaf(lsib);
                order = sorted_slots(g.read(), key_of_);
                if (order.empty()) break;
                len = sp_record(g.read(), order.back().second).size();
            }
            if (!leaf_ok_after_removing(lsib, len)) break;
            if (!move_record(lsib, order.back().second, lb)) break;
            moved = true;
        }
        if (moved) {
            p.keys[idx - 1] = min_key(lb);
            save_node(pb, p);
        }
        if (leaf_ok(lb)) return;
    }
    if (leaf_ok(lb)) return;
    // 3) fusão
    auto move_all = [&](std::uint32_t from, std::uint32_t to) {
        std::vector<std::uint16_t> slots;
        {
            auto g = leaf(from);
            for (const auto& e : sorted_slots(g.read(), key_of_)) slots.push_back(e.second);
        }
        for (auto s : slots)
            if (!move_record(from, s, to)) return false;
        return true;
    };
    if (idx + 1 < p.children.size()) {
        const std::uint32_t rb = p.children[idx + 1];
        if (!move_all(rb, lb)) return;   // não coube: fica abaixo do mínimo (registros variáveis)
        unlink_leaf(rb);
        p.keys.erase(p.keys.begin() + static_cast<std::ptrdiff_t>(idx));
        p.children.erase(p.children.begin() + static_cast<std::ptrdiff_t>(idx) + 1);
    } else if (idx > 0) {
        const std::uint32_t lsib = p.children[idx - 1];
        if (!move_all(lb, lsib)) return;
        unlink_leaf(lb);
        p.keys.erase(p.keys.begin() + static_cast<std::ptrdiff_t>(idx) - 1);
        p.children.erase(p.children.begin() + static_cast<std::ptrdiff_t>(idx));
    } else {
        return;
    }
    save_node(pb, p);
    path.pop_back();
    fix_internal(pb, std::move(path));
}

void BPlusEngine::fix_internal(std::uint32_t nb, Path path) {
    if (nb == meta_.root) {
        const InternalNode n = load_node(nb);
        if (n.keys.empty()) {   // raiz com um só filho: a árvore diminui
            meta_.root = n.children[0];
            meta_.root_is_leaf = n.level == 1;
            --meta_.height;
            pool_.free_block(BlockId{node_file_, nb});
        }
        return;
    }
    InternalNode n = load_node(nb);
    if (n.keys.size() >= min_int_) return;
    const auto [gb, gi] = path.back();
    InternalNode g = load_node(gb);
    if (gi + 1 < g.children.size()) {   // irmão da direita empresta
        const std::uint32_t sb = g.children[gi + 1];
        InternalNode s = load_node(sb);
        if (s.keys.size() > min_int_) {
            n.keys.push_back(g.keys[gi]);
            n.children.push_back(s.children.front());
            g.keys[gi] = s.keys.front();
            s.keys.erase(s.keys.begin());
            s.children.erase(s.children.begin());
            save_node(nb, n);
            save_node(sb, s);
            save_node(gb, g);
            return;
        }
    }
    if (gi > 0) {   // irmão da esquerda empresta
        const std::uint32_t sb = g.children[gi - 1];
        InternalNode s = load_node(sb);
        if (s.keys.size() > min_int_) {
            n.keys.insert(n.keys.begin(), g.keys[gi - 1]);
            n.children.insert(n.children.begin(), s.children.back());
            g.keys[gi - 1] = s.keys.back();
            s.keys.pop_back();
            s.children.pop_back();
            save_node(nb, n);
            save_node(sb, s);
            save_node(gb, g);
            return;
        }
    }
    if (gi + 1 < g.children.size()) {   // funde o da direita neste
        const std::uint32_t sb = g.children[gi + 1];
        const InternalNode s = load_node(sb);
        n.keys.push_back(g.keys[gi]);
        n.keys.insert(n.keys.end(), s.keys.begin(), s.keys.end());
        n.children.insert(n.children.end(), s.children.begin(), s.children.end());
        g.keys.erase(g.keys.begin() + static_cast<std::ptrdiff_t>(gi));
        g.children.erase(g.children.begin() + static_cast<std::ptrdiff_t>(gi) + 1);
        save_node(nb, n);
        save_node(gb, g);
        pool_.free_block(BlockId{node_file_, sb});
    } else if (gi > 0) {   // funde este no da esquerda
        const std::uint32_t sb = g.children[gi - 1];
        InternalNode s = load_node(sb);
        s.keys.push_back(g.keys[gi - 1]);
        s.keys.insert(s.keys.end(), n.keys.begin(), n.keys.end());
        s.children.insert(s.children.end(), n.children.begin(), n.children.end());
        g.keys.erase(g.keys.begin() + static_cast<std::ptrdiff_t>(gi) - 1);
        g.children.erase(g.children.begin() + static_cast<std::ptrdiff_t>(gi));
        save_node(sb, s);
        save_node(gb, g);
        pool_.free_block(BlockId{node_file_, nb});
    } else {
        return;
    }
    path.pop_back();
    fix_internal(gb, std::move(path));
}

// ----------------------------------------------------------------------- carga em massa (38)
void BPlusEngine::bulk_load(RecordStream& sorted, double fill) {
    if (fill <= 0 || fill > 1) throw std::invalid_argument("fill fora de (0, 1]");
    const std::uint32_t bs = pool_.block_size();
    const std::uint32_t target_count = fixed_ ? std::max<std::uint32_t>(1, static_cast<std::uint32_t>(fill * leaf_cap_)) : 0;
    const std::uint32_t target_bytes = static_cast<std::uint32_t>(fill * (bs - kPageHeader));

    std::vector<std::pair<Key, std::uint32_t>> level;   // (1ª chave, bloco): uma entrada por folha
    std::uint32_t cur = meta_.first_leaf;
    {
        auto g = leaf(cur);
        if (read_header(g.read()).live != 0) throw std::logic_error("bulk_load em árvore não vazia");
    }
    std::optional<PageGuard> g{leaf(cur)};
    std::uint32_t count = 0, bytes = 0;
    std::optional<Key> prev;
    while (auto rec = sorted.next()) {
        const Key k = key_of_(*rec);
        if (prev) {
            const int c = compare_keys(*prev, k);
            if (c > 0) throw std::invalid_argument("bulk_load: fluxo fora de ordem");
            if (c == 0 && unique_) throw DuplicateKey("bulk_load: chave duplicada " + key_to_string(k));
        }
        prev = k;
        const std::uint32_t len = static_cast<std::uint32_t>(rec->size()) + kSlotEntry;
        const bool full = count > 0 && (fixed_ ? count >= target_count : bytes + len > target_bytes);
        if (full || !sp_fits(g->read(), rec->size())) {
            auto [nbid, ng] = pool_.allocate(leaf_file_, kNoReservation);
            sp_init(ng.write(), leaf_type_);
            PageHeader oh = read_header(g->read());
            oh.next = nbid.block;
            write_header(g->write(), oh);
            PageHeader nh = read_header(ng.read());
            nh.prev = cur;
            write_header(ng.write(), nh);
            g.emplace(std::move(ng));
            cur = nbid.block;
            count = 0;
            bytes = 0;
        }
        if (count == 0) level.emplace_back(k, cur);
        if (!sp_insert(g->write(), *rec)) throw std::logic_error("bulk_load: registro maior que a folha");
        ++count;
        bytes += len;
    }
    g.reset();
    if (level.size() <= 1) return;   // árvore de uma folha: a raiz continua sendo a folha

    std::uint16_t lvl = 1;
    const std::size_t target_children =
        std::max<std::size_t>(2, static_cast<std::size_t>(fill * static_cast<double>(int_cap_ + 1)));
    while (level.size() > 1) {
        const std::size_t nodes = (level.size() + target_children - 1) / target_children;
        const std::size_t base = level.size() / nodes, extra = level.size() % nodes;
        std::vector<std::pair<Key, std::uint32_t>> up;
        std::size_t pos = 0;
        for (std::size_t j = 0; j < nodes; ++j) {
            const std::size_t cnt = base + (j < extra ? 1 : 0);
            InternalNode n;
            n.level = lvl;
            for (std::size_t c = 0; c < cnt; ++c) {
                if (c > 0) n.keys.push_back(level[pos + c].first);
                n.children.push_back(level[pos + c].second);
            }
            auto [bid, ng] = pool_.allocate(node_file_, kNoReservation);
            write_internal(ng.write(), n, codec_);
            up.emplace_back(level[pos].first, bid.block);
            pos += cnt;
        }
        level = std::move(up);
        ++lvl;
    }
    meta_.root = level.front().second;
    meta_.root_is_leaf = false;
    meta_.height = lvl;
}

// ----------------------------------------------------------------------- forma e invariantes
BPlusEngine::Shape BPlusEngine::measure() const {
    Shape s;
    s.height = meta_.height;
    if (!meta_.root_is_leaf) {
        std::vector<std::uint32_t> frontier{meta_.root};
        while (!frontier.empty()) {
            s.levels.push_back(static_cast<std::uint32_t>(frontier.size()));
            s.internal_blocks += static_cast<std::uint32_t>(frontier.size());
            std::vector<std::uint32_t> next;
            bool leaf_level = false;
            for (auto b : frontier) {
                const InternalNode n = load_node(b);
                if (n.level == 1) { leaf_level = true; continue; }
                next.insert(next.end(), n.children.begin(), n.children.end());
            }
            if (leaf_level) break;
            frontier = std::move(next);
        }
    }
    std::optional<Key> last;
    for (std::uint32_t b = meta_.first_leaf; b != 0;) {
        auto g = leaf(b);
        ++s.leaves;
        for (const auto& [k, slot] : sorted_slots(g.read(), key_of_)) {
            ++s.records;
            if (!last || compare_keys(*last, k) != 0) ++s.distinct;
            last = k;
        }
        b = read_header(g.read()).next;
    }
    return s;
}

std::string BPlusEngine::verify(bool check_occupancy) const {
    std::optional<Key> last;
    std::uint32_t prev = 0;
    for (std::uint32_t b = meta_.first_leaf; b != 0;) {
        auto g = leaf(b);
        const PageHeader h = read_header(g.read());
        if (h.prev != prev) return "folha " + std::to_string(b) + " com prev errado";
        for (const auto& [k, slot] : sorted_slots(g.read(), key_of_)) {
            if (last && compare_keys(*last, k) > 0) return "cadeia de folhas fora de ordem na folha " + std::to_string(b);
            if (last && unique_ && compare_keys(*last, k) == 0) return "chave repetida em árvore única";
            last = k;
        }
        if (check_occupancy && !meta_.root_is_leaf && h.next != 0 && !leaf_ok(b))
            return "folha " + std::to_string(b) + " abaixo da ocupação mínima";
        prev = b;
        b = h.next;
    }
    if (meta_.root_is_leaf) return {};
    // nós internos: chaves ordenadas e filhos dentro dos limites
    std::function<std::string(std::uint32_t, std::optional<Key>, std::optional<Key>, bool)> rec =
        [&](std::uint32_t b, std::optional<Key> lo, std::optional<Key> hi, bool is_root) -> std::string {
        const InternalNode n = load_node(b);
        if (!is_root && check_occupancy && n.keys.size() < min_int_) return "nó interno abaixo do mínimo";
        for (std::size_t i = 1; i < n.keys.size(); ++i)
            if (compare_keys(n.keys[i - 1], n.keys[i]) > 0) return "nó interno com chaves fora de ordem";
        for (std::size_t i = 0; i < n.children.size(); ++i) {
            std::optional<Key> clo = i == 0 ? lo : std::optional<Key>(n.keys[i - 1]);
            std::optional<Key> chi = i == n.keys.size() ? hi : std::optional<Key>(n.keys[i]);
            if (n.level == 1) {
                auto g = leaf(n.children[i]);
                for (const auto& [k, slot] : sorted_slots(g.read(), key_of_)) {
                    if (clo && compare_keys(k, *clo) < 0) return "registro abaixo do separador";
                    if (chi && compare_keys(k, *chi) > 0) return "registro acima do separador";
                }
            } else {
                auto r = rec(n.children[i], clo, chi, false);
                if (!r.empty()) return r;
            }
        }
        return {};
    };
    return rec(meta_.root, std::nullopt, std::nullopt, true);
}

}  // namespace cbd
