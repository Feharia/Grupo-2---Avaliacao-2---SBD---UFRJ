#include "layer2/algorithms.hpp"

#include <algorithm>
#include <cmath>
#include <functional>

namespace cbd {

namespace {

// ======================================================================= fontes ordenadas (sort-merge)
struct SortedSource {
    virtual ~SortedSource() = default;
    virtual const RecordView* cur() = 0;   // registro atual ou nullptr no fim
    virtual void advance() = 0;
};

class RunSource final : public SortedSource {   // uma run selada, lida em sequência
public:
    explicit RunSource(TempFile& run) : scan_(run.scan_blocks(kSeqOnce)) { advance(); }
    const RecordView* cur() override { return has_ ? &cur_ : nullptr; }
    void advance() override {
        has_ = false;
        for (;;) {
            if (!cursor_) {
                cursor_ = scan_.next();
                if (!cursor_) return;
            }
            if (auto r = cursor_->next()) { cur_ = *r; has_ = true; return; }
            cursor_.reset();   // solta a página antes de fixar a seguinte
        }
    }
private:
    BlockScan scan_;
    std::optional<BlockCursor> cursor_;
    RecordView cur_;
    bool has_ = false;
};

class SearchSource final : public SortedSource {   // entrada já ordenada (ordered_by, 164)
public:
    explicit SearchSource(SearchCursor c) : c_(std::move(c)) { advance(); }
    const RecordView* cur() override { return cur_ ? &*cur_ : nullptr; }
    void advance() override { cur_ = c_.next(); }
private:
    SearchCursor c_;
    std::optional<RecordView> cur_;
};

class MergeSource final : public SortedSource {   // intercala k fontes (k ≤ fan_in)
public:
    MergeSource(std::vector<std::unique_ptr<SortedSource>> in, FieldId f) : in_(std::move(in)), field_(f) { pick(); }
    const RecordView* cur() override { return best_ < in_.size() ? in_[best_]->cur() : nullptr; }
    void advance() override {
        if (best_ < in_.size()) {
            in_[best_]->advance();
            pick();
        }
    }
private:
    void pick() {
        best_ = in_.size();
        std::optional<Key> bk;
        for (std::size_t i = 0; i < in_.size(); ++i) {
            const RecordView* r = in_[i]->cur();
            if (!r) continue;
            Key k = r->key(field_);
            if (!bk || compare_keys(k, *bk) < 0) { bk = std::move(k); best_ = i; }
        }
    }
    std::vector<std::unique_ptr<SortedSource>> in_;
    FieldId field_;
    std::size_t best_ = 0;
};

// Gera runs ordenadas: run_pages de ScratchSpace + 1 página de entrada + 1 de saída = m.
std::vector<TempFile> make_runs(StorageManager& sm, const JoinInput& in, std::size_t run_pages, const Reservation& res) {
    const Schema& schema = sm.schema(in.path.table);
    std::vector<TempFile> runs;
    ScratchSpace sc = sm.create_scratch(schema, res, run_pages);
    auto flush_run = [&] {
        if (sc.size() == 0) return;
        sc.sort_by(in.field);
        TempFile t = sm.create_temp(schema, res);
        for (std::size_t i = 0; i < sc.size(); ++i) t.append(sc.at(i));
        t.seal();
        runs.push_back(std::move(t));
        sc.clear();
    };
    BlockScan scan = sm.scan_blocks(in.path.table, kSeqOnce, res);
    while (auto c = scan.next()) {
        while (auto r = c->next()) {
            if (r->is_null(in.field)) continue;   // chave NULL nunca casa (158)
            if (!sc.add(*r)) {
                flush_run();
                if (!sc.add(*r)) throw DoesNotFit("registro maior que a memória de trabalho");
            }
        }
    }
    flush_run();
    return runs;
}

// Um passe de intercalação: grupos de fan_in runs viram uma run cada (fan_in + 1 molduras).
std::vector<TempFile> merge_pass(StorageManager& sm, const Schema& schema, std::vector<TempFile> runs, FieldId field,
                                 std::size_t fan_in, const Reservation& res) {
    std::vector<TempFile> out;
    for (std::size_t i = 0; i < runs.size(); i += fan_in) {
        const std::size_t end = std::min(runs.size(), i + fan_in);
        TempFile merged = sm.create_temp(schema, res);
        {
            std::vector<std::unique_ptr<SortedSource>> src;
            for (std::size_t j = i; j < end; ++j) src.push_back(std::make_unique<RunSource>(runs[j]));
            MergeSource ms(std::move(src), field);
            while (const RecordView* r = ms.cur()) {
                merged.append(*r);
                ms.advance();
            }
        }
        merged.seal();
        for (std::size_t j = i; j < end; ++j) { TempFile drop = std::move(runs[j]); }   // apaga as runs consumidas
        out.push_back(std::move(merged));
    }
    return out;
}

std::unique_ptr<SortedSource> runs_source(std::vector<TempFile>& runs, FieldId field) {
    std::vector<std::unique_ptr<SortedSource>> src;
    for (auto& r : runs) src.push_back(std::make_unique<RunSource>(r));
    return std::make_unique<MergeSource>(std::move(src), field);
}

}  // namespace

// ======================================================================= nested-loop em blocos
void bnl_join(StorageManager& sm, const JoinInput& outer, const JoinInput& inner, const JoinPlan& plan,
              const Reservation& res, ResultSink& sink, PhaseRecorder& ph) {
    ph.begin("bnl");
    BlockScan oscan = sm.scan_blocks(outer.path.table, kSeqOnce, res);
    for (;;) {
        std::vector<BlockCursor> chunk;   // plan.chunk páginas da externa fixadas
        while (chunk.size() < plan.chunk) {
            auto c = oscan.next();
            if (!c) break;
            chunk.push_back(std::move(*c));
        }
        if (chunk.empty()) break;
        // diretório das chaves do chunk: limitado aos registros das páginas fixadas (§7.1)
        std::vector<std::pair<Key, RecordView>> keys;
        for (auto& c : chunk)
            while (auto r = c.next())
                if (!r->is_null(outer.field)) keys.emplace_back(r->key(outer.field), *r);
        std::stable_sort(keys.begin(), keys.end(),
                         [](const auto& a, const auto& b) { return compare_keys(a.first, b.first) < 0; });
        BlockScan iscan = sm.scan_blocks(inner.path.table, kSeqReuse, res);
        while (auto ic = iscan.next()) {
            while (auto r = ic->next()) {
                if (r->is_null(inner.field)) continue;
                const Key k = r->key(inner.field);
                auto lo = std::lower_bound(keys.begin(), keys.end(), k,
                                           [](const auto& e, const Key& v) { return compare_keys(e.first, v) < 0; });
                for (auto it = lo; it != keys.end() && compare_keys(it->first, k) == 0; ++it) sink.emit_pair(it->second, *r);
            }
        }
    }
}

// ======================================================================= sort-merge
void sort_merge_join(StorageManager& sm, const JoinInput& outer, const JoinInput& inner, const JoinPlan& plan,
                     const Reservation& res, ResultSink& sink, PhaseRecorder& ph) {
    const Schema& so = sm.schema(outer.path.table);
    const Schema& si = sm.schema(inner.path.table);
    std::vector<TempFile> ro, ri;
    if (plan.sort_outer) { ph.begin("runs_externa"); ro = make_runs(sm, outer, plan.run_pages, res); }
    if (plan.sort_inner) { ph.begin("runs_interna"); ri = make_runs(sm, inner, plan.run_pages, res); }
    ph.begin("passes");
    auto count = [](const std::vector<TempFile>& r, bool sorted) { return sorted ? r.size() : std::size_t{1}; };
    while (count(ro, plan.sort_outer) + count(ri, plan.sort_inner) > plan.final_fan_in) {
        const bool do_outer = plan.sort_outer && ro.size() > 1 && (ro.size() >= ri.size() || !plan.sort_inner || ri.size() <= 1);
        if (do_outer) ro = merge_pass(sm, so, std::move(ro), outer.field, plan.fan_in, res);
        else if (plan.sort_inner && ri.size() > 1) ri = merge_pass(sm, si, std::move(ri), inner.field, plan.fan_in, res);
        else throw DoesNotFit("intercalação final não cabe em m");
    }
    ph.begin("intercalacao_final");
    KeyRange all_outer, all_inner;   // faixa inteira: leitura em ordem pela estrutura
    all_outer.field = outer.field;
    all_inner.field = inner.field;
    std::unique_ptr<SortedSource> O = plan.sort_outer ? runs_source(ro, outer.field)
        : std::make_unique<SearchSource>(sm.find(outer.path, all_outer, kSeqOnce, res));
    std::unique_ptr<SortedSource> I = plan.sort_inner ? runs_source(ri, inner.field)
        : std::make_unique<SearchSource>(sm.find(inner.path, all_inner, kSeqOnce, res));
    std::optional<ScratchSpace> group;
    if (plan.group_pages > 0) group.emplace(sm.create_scratch(si, res, plan.group_pages));
    while (O->cur() && I->cur()) {
        const Key ko = O->cur()->key(outer.field);
        const Key ki = I->cur()->key(inner.field);
        const int c = compare_keys(ko, ki);
        if (c < 0) { O->advance(); continue; }
        if (c > 0) { I->advance(); continue; }
        if (!group) {   // interna UNIQUE: o registro atual da interna fica fixado enquanto a externa avança
            const RecordView iv = *I->cur();
            while (O->cur() && compare_keys(O->cur()->key(outer.field), ki) == 0) {
                sink.emit_pair(*O->cur(), iv);
                O->advance();
            }
            I->advance();
            continue;
        }
        group->clear();
        while (I->cur() && compare_keys(I->cur()->key(inner.field), ki) == 0) {
            if (!group->add(*I->cur())) throw DoesNotFit("grupo de duplicatas da interna não cabe em group_pages");
            I->advance();
        }
        while (O->cur() && compare_keys(O->cur()->key(outer.field), ki) == 0) {
            for (std::size_t g = 0; g < group->size(); ++g) sink.emit_pair(*O->cur(), group->at(g));
            O->advance();
        }
    }
    O.reset();
    I.reset();
}

// ======================================================================= partition-hash (externa = build), recursivo (171)
namespace {
struct HashSource {                          // uma entrada de uma partição
    std::function<BlockScan()>   scan;
    std::optional<std::uint32_t> blocks;     // conhecido nos temporários; vazio em tabelas e grupos de buckets
};
constexpr std::size_t kMaxHashPasses = 6;    // parada da recursão (171)
}  // namespace

void hash_join(StorageManager& sm, const JoinInput& outer, const JoinInput& inner, const JoinPlan& plan,
               const Reservation& res, ResultSink& sink, PhaseRecorder& ph) {
    const Schema& so = sm.schema(outer.path.table);
    const Schema& si = sm.schema(inner.path.table);
    std::size_t extra_passes = 0, bailouts = 0;

    auto probe_with = [&](const ScratchSpace& sc, const HashSource& p) {
        BlockScan ps = p.scan();
        while (auto c = ps.next())
            while (auto r = c->next()) {
                if (r->is_null(inner.field)) continue;   // chave NULL nunca casa (158)
                auto matches = sc.probe(r->key(inner.field));
                while (auto mrec = matches.next()) sink.emit_pair(*mrec, *r);
            }
    };

    // build em m − 1 páginas + 1 de probe; devolve false se o build não coube
    auto try_build_probe = [&](const HashSource& b, const HashSource& p) -> bool {
        ScratchSpace sc = sm.create_scratch(so, res, plan.build_pages);
        {
            BlockScan bs = b.scan();
            while (auto c = bs.next())
                while (auto r = c->next()) {
                    if (r->is_null(outer.field)) continue;
                    if (!sc.add(*r)) return false;   // não coube: a partição é particionada de novo
                }
        }
        sc.build_hash(outer.field, plan.table_seed);
        probe_with(sc, p);
        return true;
    };

    // Bailout (171): grupo que a recursão não divide. Laço em blocos dentro da partição:
    // m − 2 páginas de build + 1 do build sendo lido + 1 do probe = m.
    auto bailout = [&](const HashSource& b, const HashSource& p) {
        ++bailouts;
        ScratchSpace sc = sm.create_scratch(so, res, plan.build_pages - 1);
        auto flush = [&] {
            if (sc.size() == 0) return;
            sc.build_hash(outer.field, plan.table_seed);
            probe_with(sc, p);
            sc.clear();
        };
        BlockScan bs = b.scan();
        while (auto c = bs.next())
            while (auto r = c->next()) {
                if (r->is_null(outer.field)) continue;
                if (!sc.add(*r)) {
                    flush();   // a página de *r continua fixada pelo cursor
                    if (!sc.add(*r)) throw DoesNotFit("registro maior que a memória de trabalho");
                }
            }
        flush();
    };

    auto partition = [&](const HashSource& src, FieldId field, const Schema& s, std::size_t parts,
                         const std::function<std::size_t(const Key&)>& part_of) {
        std::vector<TempFile> out;
        out.reserve(parts);
        for (std::size_t i = 0; i < parts; ++i) out.push_back(sm.create_temp(s, res));
        BlockScan scan = src.scan();
        while (auto c = scan.next())
            while (auto r = c->next()) {
                if (r->is_null(field)) continue;
                out[part_of(r->key(field))].append(*r);
            }
        for (auto& t : out) t.seal();
        return out;
    };

    // processa uma partição; se o build não couber, particiona de novo com outra semente (153, 171)
    std::function<void(const HashSource&, const HashSource&, std::size_t)> process =
        [&](const HashSource& b, const HashSource& p, std::size_t passes) {
            if ((b.blocks && *b.blocks == 0) || (p.blocks && *p.blocks == 0)) return;   // partição vazia
            const bool too_big = b.blocks && *b.blocks > plan.build_pages;
            if (!too_big && try_build_probe(b, p)) return;
            if (passes >= kMaxHashPasses) { bailout(b, p); return; }
            ++extra_passes;
            const std::size_t parts =
                b.blocks ? std::clamp<std::size_t>((static_cast<std::size_t>(std::ceil(*b.blocks * 1.2)) + plan.build_pages - 1) /
                                                       plan.build_pages,
                                                   2, plan.m - 1)
                         : plan.m - 1;
            const std::uint64_t seed = plan.partition_seed + 0x9E3779B97F4A7C15ull * (passes + 1);
            auto part_of = [seed, parts](const Key& k) { return static_cast<std::size_t>(hash_key(k, seed) % parts); };
            std::vector<TempFile> sb = partition(b, outer.field, so, parts, part_of);
            std::vector<TempFile> sp = partition(p, inner.field, si, parts, part_of);
            for (std::size_t i = 0; i < parts; ++i) {
                const HashSource cb{[&sb, i] { return sb[i].scan_blocks(kSeqOnce); }, sb[i].blocks()};
                const HashSource cp{[&sp, i] { return sp[i].scan_blocks(kSeqOnce); }, sp[i].blocks()};
                if (b.blocks && *cb.blocks >= *b.blocks) bailout(cb, cp);   // não diminuiu: grupo indivisível
                else process(cb, cp, passes + 1);
                TempFile drop_b = std::move(sb[i]), drop_p = std::move(sp[i]);   // apaga os já usados
            }
        };

    const HashSource whole_outer{[&] { return sm.scan_blocks(outer.path.table, kSeqOnce, res); }, std::nullopt};
    const HashSource whole_inner{[&] { return sm.scan_blocks(inner.path.table, kSeqOnce, res); }, std::nullopt};

    if (plan.partitions <= 1) {
        ph.begin("build_probe");
        process(whole_outer, whole_inner, 0);
    } else {
        const std::size_t P = plan.partitions;
        const AccessPath* prepath = plan.prepartitioned_outer ? &outer.path : (plan.prepartitioned_inner ? &inner.path : nullptr);
        const std::function<std::size_t(const Key&)> part0 = [&](const Key& k) -> std::size_t {
            if (prepath) return sm.bucket_of(*prepath, k) / plan.bucket_group;   // mesma função do arquivo hash
            return static_cast<std::size_t>(hash_key(k, plan.partition_seed) % P);
        };
        std::vector<TempFile> po, pi;
        if (!plan.prepartitioned_outer) { ph.begin("particiona_externa"); po = partition(whole_outer, outer.field, so, P, part0); }
        if (!plan.prepartitioned_inner) { ph.begin("particiona_interna"); pi = partition(whole_inner, inner.field, si, P, part0); }
        ph.begin("build_probe");   // inclui a E/S dos passes recursivos e dos bailouts
        const std::uint32_t nb = prepath ? prepath->hash_partitioned_by->buckets : 0;
        auto buckets_of = [&](const AccessPath& path, std::size_t p) {
            const auto first = static_cast<std::uint32_t>(p * plan.bucket_group);
            const auto last = static_cast<std::uint32_t>(std::min<std::size_t>(nb, (p + 1) * plan.bucket_group) - 1);
            return HashSource{[&sm, &path, &res, first, last] { return sm.scan_buckets(path, first, last, kSeqOnce, res); },
                              std::nullopt};
        };
        for (std::size_t p = 0; p < P; ++p) {
            const HashSource b = plan.prepartitioned_outer
                                     ? buckets_of(outer.path, p)
                                     : HashSource{[&po, p] { return po[p].scan_blocks(kSeqOnce); }, po[p].blocks()};
            const HashSource q = plan.prepartitioned_inner
                                     ? buckets_of(inner.path, p)
                                     : HashSource{[&pi, p] { return pi[p].scan_blocks(kSeqOnce); }, pi[p].blocks()};
            process(b, q, 1);
        }
    }
    // resumo da execução, numa fase final sem E/S (aparece em fases.csv)
    ph.begin("resumo: passes_recursivos=" + std::to_string(extra_passes) + " bailouts=" + std::to_string(bailouts));
}

// ======================================================================= seleções
void select_scan(StorageManager& sm, TableId t, FieldId field, const std::vector<KeyRange>& keys, bool stop_at_first,
                 const Reservation& res, ResultSink& sink) {
    for (const auto& kr : keys) {   // uma varredura por chave do lote (§9)
        BlockScan scan = sm.scan_blocks(t, kSeqOnce, res);
        bool stop = false;
        while (!stop) {
            auto c = scan.next();
            if (!c) break;
            while (auto r = c->next()) {
                if (r->is_null(field)) continue;
                if (kr.contains(r->key(field))) {
                    sink.emit(*r);
                    if (stop_at_first) { stop = true; break; }
                }
            }
        }
    }
}

void select_find(StorageManager& sm, const AccessPath& path, const Predicate& p, Intent i, const Reservation& res,
                 ResultSink& sink) {
    SearchCursor c = sm.find(path, p, i, res);
    while (auto r = c.next()) sink.emit(*r);
}

void select_probe(StorageManager& sm, const AccessPath& path, const Predicate& p, const SelectPlan& plan,
                  const Reservation& res, ResultSink& sink) {
    RidCursor rc = sm.probe(path, p, kRandomReuse);
    if (!plan.sort_rids) {   // baseline: RIDs na ordem das posting lists (117)
        while (auto rid = rc.next()) {
            PinnedRecord pr = sm.fetch(path.table, *rid, kRandomOnce, res);
            sink.emit(pr.view);
        }
        return;
    }
    ScratchSpace rs = sm.create_rid_scratch(res, plan.scratch_pages);
    auto drain = [&] {
        rs.sort_rids();
        for (std::size_t i = 0; i < rs.size(); ++i) {
            PinnedRecord pr = sm.fetch(path.table, rs.rid_at(i), kRandomOnce, res);
            sink.emit(pr.view);
        }
        rs.clear();
    };
    while (auto rid = rc.next()) {
        if (!rs.add(*rid)) {
            drain();
            rs.add(*rid);
        }
    }
    drain();
}

// ======================================================================= estimativas (fórmulas dos livros)
Cost estimate_join(const JoinPlan& p, const TableStats& o, const TableStats& i) {
    // fórmulas dos livros com os parâmetros reais: b para ler a estrutura, d (blocos densos) para
    // gravar e reler runs e partições (emenda da 143)
    const double br = o.b, bs = i.b;
    const double dr = dense_blocks(o), ds = dense_blocks(i);
    Cost c;
    switch (p.alg) {
        case JoinAlgorithm::Bnl: {
            const double chunks = std::ceil(br / static_cast<double>(std::max<std::size_t>(1, p.chunk)));
            c.transfers = br + chunks * bs;
            c.seeks = 2 * chunks;
            break;
        }
        case JoinAlgorithm::SortMerge: {
            auto side = [&](double b, double d, bool needs_sort, std::size_t passes, std::size_t runs) {
                if (!needs_sort) return std::pair<double, double>{b, 1};   // leitura em ordem pela estrutura
                const double t = b + d + 2 * d * passes + d;               // lê, grava runs, passes, lê no final
                const double s = 2.0 * runs + 2.0 * d * passes;
                return std::pair<double, double>{t, s};
            };
            const auto so = side(br, dr, p.sort_outer, p.merge_passes_outer, p.runs_outer);
            const auto si = side(bs, ds, p.sort_inner, p.merge_passes_inner, p.runs_inner);
            c.transfers = so.first + si.first;
            c.seeks = so.second + si.second + (p.sort_outer ? dr : br) + (p.sort_inner ? ds : bs);
            break;
        }
        case JoinAlgorithm::Hash: {
            if (p.partitions <= 1) {
                c.transfers = br + bs;
                c.seeks = 2;
                break;
            }
            // cada passe grava e relê as partições; uma entrada pré-particionada pula o 1º passe (171)
            const double L = static_cast<double>(std::max<std::size_t>(1, p.levels));
            auto side = [&](double b, double d, bool pre) { return b + 2 * (pre ? L - 1 : L) * d; };
            const double nh = static_cast<double>(p.partitions) * std::pow(static_cast<double>(p.m - 1), L - 1);
            c.transfers = side(br, dr, p.prepartitioned_outer) + side(bs, ds, p.prepartitioned_inner) + 4 * nh;
            c.seeks = 2 * (dr + ds) * L + 2 * nh;
            break;
        }
    }
    return c;
}

Cost estimate_select(const SelectPlan& p, const TableStats& t, const StructureStats* s) {
    Cost c;
    const double B = static_cast<double>(p.batch);
    const double rows = p.expected_rows;
    const double bfr = std::max<double>(1, t.bfr);
    const double per_key = p.range ? rows : (B > 0 ? rows / B : 0);
    const double h_int = s && s->height ? std::max(0, *s->height - 1) : 0;   // níveis internos
    switch (p.path) {
        case PathKind::Scan:
            if (p.range) { c.transfers = t.b; c.seeks = 1; }
            else { c.transfers = B * (p.unique_field ? t.b / 2.0 : t.b); c.seeks = B; }
            break;
        case PathKind::PrimaryBTree:
        case PathKind::SecondaryBTreeClustering: {
            const double leaves = std::max(1.0, std::ceil(per_key / bfr));
            const double one = h_int + (p.unique_field && !p.range ? 1 : leaves);
            c.transfers = p.range ? one : B * one;
            c.seeks = p.range ? h_int + 1 : B * (h_int + 1);
            break;
        }
        case PathKind::PrimaryHash: {
            const double chain = s && s->chain_avg ? *s->chain_avg : 1;
            c.transfers = B * chain;
            c.seeks = B * chain;
            break;
        }
        case PathKind::SecondaryHash: {
            const double chain = s && s->chain_avg ? *s->chain_avg : 1;
            c.transfers = B * (chain + 1);
            c.seeks = c.transfers;
            break;
        }
        case PathKind::SecondaryBTreeNonClustering: {
            const double one = h_int + 1 + per_key;   // h + folha + um bloco por RID (h + s)
            c.transfers = p.range ? one : B * one;
            c.seeks = c.transfers;
            break;
        }
    }
    return c;
}

}  // namespace cbd
