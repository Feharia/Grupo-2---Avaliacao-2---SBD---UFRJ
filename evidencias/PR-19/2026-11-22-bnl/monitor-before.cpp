#include "layer2/monitor.hpp"

#include "layer2/algorithms.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace cbd {

namespace {
std::size_t ceil_div(std::size_t a, std::size_t b) { return b == 0 ? 0 : (a + b - 1) / b; }
int key_class(FieldType t) {
    switch (t) {
        case FieldType::Int32: return 0;
        case FieldType::Int64: return 1;
        default: return 2;
    }
}
const StructureStats* find_structure(const TableStats& ts, StructureId id) {
    for (const auto& s : ts.structures)
        if (s.id == id) return &s;
    return nullptr;
}
}  // namespace

// ======================================================================= ResultSink (142, 152, 158, 166)
ResultSink::ResultSink(ResultShape shape, bool swapped) : shape_(std::move(shape)), swapped_(swapped) {
    if (shape_.dump) {
        dump_ = std::make_unique<std::ofstream>(*shape_.dump, std::ios::app);
        if (!*dump_) throw std::runtime_error("não abriu o arquivo de dump: " + shape_.dump->string());
    }
}

void ResultSink::encode(const RecordView& r, const std::vector<FieldId>& fields) {
    const Schema& s = *r.schema();
    auto put = [&](const void* p, std::size_t n) {
        const auto* b = static_cast<const std::byte*>(p);
        buf_.insert(buf_.end(), b, b + n);
    };
    auto one = [&](FieldId f) {
        if (r.is_null(f)) {
            buf_.push_back(std::byte{0});   // marca de NULL (158)
            return;
        }
        buf_.push_back(std::byte{1});
        const Key k = r.key(f);
        if (auto p = std::get_if<std::int32_t>(&k)) {
            const std::int64_t v = *p;
            put(&v, 8);
        } else if (auto q = std::get_if<std::int64_t>(&k)) {
            put(&*q, 8);
        } else {
            const auto& t = std::get<std::string>(k);
            const std::uint32_t n = static_cast<std::uint32_t>(t.size());
            put(&n, 4);
            put(t.data(), t.size());
        }
    };
    if (fields.empty()) {
        for (FieldId f = 0; f < s.fields.size(); ++f) one(f);
    } else {
        for (FieldId f : fields) one(f);
    }
}

void ResultSink::finish_tuple() {
    sum_ += fnv1a64(buf_);   // soma módulo 2^64
    ++rows_;
    if (dump_) {
        std::ostringstream os;
        os << std::hex << std::setfill('0');
        for (std::byte b : buf_) os << std::setw(2) << static_cast<unsigned>(b);
        *dump_ << os.str() << '\n';
    }
    buf_.clear();
}

void ResultSink::emit(const RecordView& r) {
    encode(r, shape_.operand0_fields);
    finish_tuple();
}

void ResultSink::emit_pair(const RecordView& outer, const RecordView& inner) {
    const RecordView& op0 = swapped_ ? inner : outer;
    const RecordView& op1 = swapped_ ? outer : inner;
    encode(op0, shape_.operand0_fields);
    encode(op1, shape_.operand1_fields);
    finish_tuple();
}

// ======================================================================= PhaseRecorder
PhaseRecorder::PhaseRecorder(StorageManager& sm) : sm_(sm) {}

void PhaseRecorder::begin(std::string name) {
    Counters now = sm_.snapshot();
    if (open_) done_.push_back(Phase{open_->first, now - open_->second});
    open_.emplace(std::move(name), std::move(now));
}

std::vector<Phase> PhaseRecorder::finish() {
    if (open_) {
        done_.push_back(Phase{open_->first, sm_.snapshot() - open_->second});
        open_.reset();
    }
    return std::move(done_);
}

// ======================================================================= planos
std::size_t JoinPlan::frames_used() const {
    switch (alg) {
        case JoinAlgorithm::Bnl:   // chunk + 1 da interna + as molduras retidas do diretório (173, CBD-004)
            return chunk + 1 + dir_frames;
        case JoinAlgorithm::SortMerge:   // + as molduras da lista de runs (CBD-005)
            return std::max({run_pages + 2, fan_in + 1, final_fan_in + group_pages + ordered_extra_frames}) + desc_frames;
        case JoinAlgorithm::Hash:
            return std::max(partitions > 1 ? partitions + 1 : std::size_t{2}, build_pages + 1);
    }
    return m;
}

std::size_t SelectPlan::frames_used() const {
    // M inclui as páginas de índice (172): no probe e no clustering, a página do índice fica fixada
    // enquanto o fetch fixa a de dados; a descida da B+ fixa um nó por vez.
    const bool index_and_data = path == PathKind::SecondaryHash || path == PathKind::SecondaryBTreeNonClustering ||
                                path == PathKind::SecondaryBTreeClustering;
    const std::size_t base = index_and_data ? 2 : 1;
    return sort_rids ? scratch_pages + base : base;
}

std::variant<JoinPlan, NotApplicable> Monitor::plan_join(JoinAlgorithm alg, const JoinInput& outer,
                                                         const JoinInput& inner, std::size_t m,
                                                         const JoinParams& params) const {
    JoinPlan p;
    p.alg = alg;
    p.m = m;
    p.swapped = outer.operand == 1;
    const Schema& so = storage_.schema(outer.path.table);
    const Schema& si = storage_.schema(inner.path.table);
    const FieldDef& fo = so.field(outer.field);
    const FieldDef& fi = si.field(inner.field);
    if (key_class(fo.type) != key_class(fi.type)) return NotApplicable{"tipos dos campos da junção diferem"};
    TableStats to, ti;
    try {
        to = storage_.stats(outer.path.table);
        ti = storage_.stats(inner.path.table);
    } catch (const std::logic_error& e) {
        return NotApplicable{e.what(), NotApplicable::Kind::Inviavel};
    }
    p.b_outer = to.b;
    p.b_inner = ti.b;
    p.d_outer = dense_blocks(to);
    p.d_inner = dense_blocks(ti);
    p.inner_unique = fi.unique;
    if (m < 3) return NotApplicable{"m < 3", NotApplicable::Kind::Inviavel};

    switch (alg) {
        case JoinAlgorithm::Bnl:
            // a externa menor (b ≤ m − 2, emenda da 108) cabe inteira: uma passada; senão, chunk = ⌊m/2⌋
            if (params.chunk) p.chunk = params.chunk;
            else if (p.b_outer + 2 <= m) p.chunk = std::max<std::size_t>(1, p.b_outer);
            else p.chunk = std::max<std::size_t>(1, m / 2);
            if (p.chunk > m - 2) return NotApplicable{"chunk > m − 2", NotApplicable::Kind::Inviavel};
            {   // o diretório do chunk (um RecordView por registro) ocupa ⌈registros × 24 B / bloco⌉
                // molduras, retidas na execução (decisão 173, CBD-004); o que sobra das m − 1 − chunk
                // fica de cache para a interna. Se o diretório não couber, o chunk diminui.
                const std::size_t bs = storage_.block_size();
                const std::size_t per_page = to.b ? ceil_div(static_cast<std::size_t>(to.n), to.b) : 1;
                auto entries = [&](std::size_t c) { return (m - 1 - c) * bs / sizeof(RecordView); };
                while (p.chunk > 1 && p.chunk * per_page > entries(p.chunk)) --p.chunk;
                if (p.chunk * per_page > entries(p.chunk))
                    return NotApplicable{"diretório do BNL não cabe em m", NotApplicable::Kind::Inviavel};
                p.dir_frames = std::max<std::size_t>(1, ceil_div(p.chunk * per_page * sizeof(RecordView), bs));
                p.dir_entries = p.dir_frames * bs / sizeof(RecordView);
            }
            break;
        case JoinAlgorithm::SortMerge: {
            auto ordered = [](const JoinInput& in) {   // (164)
                return in.path.ordered_by && *in.path.ordered_by == in.field &&
                       (in.path.kind == PathKind::PrimaryBTree || in.path.kind == PathKind::SecondaryBTreeClustering);
            };
            p.sort_outer = !ordered(outer);
            p.sort_inner = !ordered(inner);
            auto extra_frames = [](const JoinInput& in, const TableStats& stats, bool needs_sort) -> std::size_t {
                // Clustering sobre B+ usa só a folha de dados após localizar o primeiro RID.
                // Sobre hash, o cursor mantém também a folha do índice durante a varredura do bucket.
                return !needs_sort && in.path.kind == PathKind::SecondaryBTreeClustering &&
                       !stats.structures.empty() && stats.structures.front().kind == StructureKind::Hash ? 1 : 0;
            };
            p.ordered_extra_frames = extra_frames(outer, to, p.sort_outer) + extra_frames(inner, ti, p.sort_inner);
            p.group_pages = p.inner_unique ? 0 : params.group_pages;
            if (p.group_pages + 2 + p.ordered_extra_frames > m)
                return NotApplicable{"grupo e cursores ordenados não cabem na intercalação", NotApplicable::Kind::Inviavel};
            // A lista de runs cresce com b/m e é cobrada em M (CBD-005): desc_frames molduras guardam os
            // descritores das runs das duas entradas e da saída de um passe de intercalação, e saem do
            // rascunho, do fan-in e da intercalação final. Mais molduras para a lista deixam as runs
            // menores e mais numerosas; repete até o número de molduras parar de subir.
            const std::size_t bs = storage_.block_size();
            std::size_t ro = 1, ri = 1, d = 0;
            for (int it = 0;; ++it) {
                if (d + 3 > m || p.group_pages + 2 + p.ordered_extra_frames + d > m || it > 16)
                    return NotApplicable{"lista de runs não cabe em m", NotApplicable::Kind::Inviavel};
                p.run_pages = m - 2 - d;
                p.fan_in = m - 1 - d;
                p.final_fan_in = m - p.group_pages - d - p.ordered_extra_frames;
                // Limite conservador pelo maior registro observado, com o diretório cobrado (173).
                // O máximo declarado de VarChar pode ser muito maior que os dados presentes.
                const std::size_t cap_o = ScratchSpace::capacity_for(to.max_record_bytes, p.run_pages, bs);
                const std::size_t cap_i = ScratchSpace::capacity_for(ti.max_record_bytes, p.run_pages, bs);
                if ((p.sort_outer && to.n && cap_o == 0) || (p.sort_inner && ti.n && cap_i == 0))
                    return NotApplicable{"registro e diretório não cabem nas páginas das runs", NotApplicable::Kind::Inviavel};
                ro = p.sort_outer ? std::max<std::size_t>(1, ceil_div(static_cast<std::size_t>(to.n), cap_o)) : 1;
                ri = p.sort_inner ? std::max<std::size_t>(1, ceil_div(static_cast<std::size_t>(ti.n), cap_i)) : 1;
                const std::size_t lista = (p.sort_outer ? ro + ceil_div(ro, p.fan_in) : 0) +
                                          (p.sort_inner ? ri + ceil_div(ri, p.fan_in) : 0);
                const std::size_t nd = ceil_div(lista * kRunDescriptorBytes, bs);
                if (nd <= d) break;
                d = nd;
            }
            p.desc_frames = d;
            p.runs_outer = ro;
            p.runs_inner = ri;
            while (ro + ri > p.final_fan_in) {
                if (ro >= ri && ro > 1) { ro = ceil_div(ro, p.fan_in); ++p.merge_passes_outer; }
                else if (ri > 1) { ri = ceil_div(ri, p.fan_in); ++p.merge_passes_inner; }
                else return NotApplicable{"intercalação final não cabe em m", NotApplicable::Kind::Inviavel};
            }
            break;
        }
        case JoinAlgorithm::Hash: {
            const double f = 1.2;   // fator de folga declarado
            p.build_pages = m - 1;
            // o build cabe se os registros e o diretório da tabela hash couberem em m − 1 páginas (173);
            // a folga f só entra na contagem de partições (emenda da 108)
            const std::size_t cap_build = ScratchSpace::capacity_for(so, p.build_pages, storage_.block_size());
            if (params.partitions == 0 && to.n <= cap_build) {
                p.partitions = 1;   // o build cabe: sem particionamento
                break;
            }
            // 1º passe com até m − 1 partições; o que não couber é particionado de novo (171)
            p.partitions = params.partitions
                               ? params.partitions
                               : std::min<std::size_t>(m - 1, ceil_div(static_cast<std::size_t>(std::ceil(to.n * f)), cap_build));
            if (p.partitions > m - 1)
                return NotApplicable{"partições pedidas excedem m − 1", NotApplicable::Kind::Inviavel};
            auto prepart = [](const JoinInput& in) {   // (164)
                return in.path.kind == PathKind::PrimaryHash && in.path.hash_partitioned_by && in.path.field == in.field;
            };
            bool po = prepart(outer), pi = prepart(inner);
            if (po && pi && !(*outer.path.hash_partitioned_by == *inner.path.hash_partitioned_by)) {
                if (p.b_outer >= p.b_inner) pi = false; else po = false;   // aproveita a maior
            }
            const HashPartitioning* hp = po ? &*outer.path.hash_partitioned_by
                                            : (pi ? &*inner.path.hash_partitioned_by : nullptr);
            if (hp) {
                const std::size_t parts = std::min<std::size_t>(p.partitions, hp->buckets);
                p.bucket_group = ceil_div(hp->buckets, parts);
                p.partitions = ceil_div(hp->buckets, p.bucket_group);
                if (p.partitions > m - 1) return NotApplicable{"partições do hash pré-particionado excedem m − 1", NotApplicable::Kind::Inviavel};
            }
            // passes previstos pela fórmula do livro, ⌈log_{m−1}(b_build) − 1⌉, no mínimo 1 (171).
            // A execução decide pelo tamanho real das partições, que pode ser menor que b.
            p.levels = std::max<std::size_t>(
                1, static_cast<std::size_t>(std::ceil(std::log(std::max(1.0, static_cast<double>(p.d_outer))) /
                                                          std::log(static_cast<double>(m - 1)) - 1.0 - 1e-9)));
            p.prepartitioned_outer = po;
            p.prepartitioned_inner = pi;
            break;
        }
    }
    return p;
}

std::variant<SelectPlan, NotApplicable> Monitor::plan_select(const AccessPath& path, bool range, std::size_t batch,
                                                             const SelectParams& params) const {
    SelectPlan p;
    p.path = path.kind;
    p.range = range;
    p.batch = batch;
    p.m = params.m;
    p.expected_rows = params.expected_rows;
    p.sort_rids = params.sort_rids;
    if (range && path.kind == PathKind::PrimaryHash) return NotApplicable{"caso 4: faixa no hash primário, que não preserva a ordem das chaves (128, 170)"};
    if (range && path.kind == PathKind::SecondaryHash)
        return NotApplicable{"caso 2: faixa em UNIQUE pelo índice secundário; o §4.2 só admite hash como secundário de UNIQUE (128, 170)"};
    if (path.kind != PathKind::Scan && !(range ? path.supports_range : path.supports_point))
        return NotApplicable{"o caminho não responde a este predicado"};
    p.filter_in_layer2 = path.kind == PathKind::Scan;
    if (p.sort_rids) {
        if (path.kind != PathKind::SecondaryBTreeNonClustering && path.kind != PathKind::SecondaryHash)
            return NotApplicable{"sort_rids só em índice secundário não clustering"};
        if (p.m < 3) return NotApplicable{"sort_rids exige m ≥ 3", NotApplicable::Kind::Inviavel};
        p.scratch_pages = p.m - 2;
    }
    if (p.m < 1) return NotApplicable{"m < 1", NotApplicable::Kind::Inviavel};
    if (p.frames_used() > p.m)   // índice e dados juntos não cabem em m (172)
        return NotApplicable{"página de índice e página de dados não cabem em m", NotApplicable::Kind::Inviavel};
    try {
        (void)storage_.stats(path.table);
    } catch (const std::logic_error& e) {
        return NotApplicable{e.what(), NotApplicable::Kind::Inviavel};
    }
    return p;
}

// ======================================================================= medição (passos 1–9, 149, 159)
template <class Exec, class Est>
Outcome Monitor::measured(std::size_t m, std::size_t frames, bool swapped, const ResultShape& shape, Exec&& exec,
                          Est&& est) {
    if (frames > m) throw std::logic_error("orçamento do plano excede m (148)");
    // CBD-010: o que o preparo deixou sujo é gravado antes do 1º snapshot e contado à parte, para
    // não cair no delta da operação se uma expulsão o gravar, nem no flush de fim
    const std::uint64_t prep = storage_.flush_all();              // 2b: preparo
    Reservation res = storage_.reserve(m);                        // 3
    ResultSink sink(shape, swapped);
    const auto t0 = std::chrono::steady_clock::now();
    const Counters s0 = storage_.snapshot();                      // 4
    PhaseRecorder phases(storage_);
    try {
        exec(res, sink, phases);                                  // 5
    } catch (const DoesNotFit& e) {
        phases.finish();
        return NotApplicable{std::string("não cabe em M: ") + e.what(), NotApplicable::Kind::Inviavel};
    }
    const Counters s1 = storage_.snapshot();                      // 6
    auto ph = phases.finish();
    const auto t1 = std::chrono::steady_clock::now();
    OpResult r;
    r.prep_writes = prep;
    r.delta = s1 - s0;
    if (r.delta.global.data_area_requests != 0)
        throw std::logic_error("página de dados pedida sem reserva durante a operação medida (159)");
    if (r.delta.global.index_area_requests != 0)
        throw std::logic_error("página de índice pedida sem reserva durante a operação medida (172)");
    r.flush_writes = storage_.flush_all();                        // 7: verificação, deve ser 0
    r.estimate = est();                                           // 8
    r.rows = sink.rows();
    r.checksum = sink.checksum();
    r.phases = std::move(ph);
    r.wall_time = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0);
    return r;                                                     // 9: a reserva é liberada aqui
}

Outcome Monitor::join(JoinAlgorithm alg, const JoinInput& outer, const JoinInput& inner, std::size_t m,
                      const ResultShape& shape, const JoinParams& params) {
    auto planned = plan_join(alg, outer, inner, m, params);       // 1
    if (auto na = std::get_if<NotApplicable>(&planned)) return *na;
    const JoinPlan plan = std::get<JoinPlan>(planned);
    const TableStats to = storage_.stats(outer.path.table), ti = storage_.stats(inner.path.table);
    return measured(m, plan.frames_used(), plan.swapped, shape,   // 2
                    [&](const Reservation& res, ResultSink& sink, PhaseRecorder& ph) {
                        switch (alg) {
                            case JoinAlgorithm::Bnl: bnl_join(storage_, outer, inner, plan, res, sink, ph); break;
                            case JoinAlgorithm::SortMerge: sort_merge_join(storage_, outer, inner, plan, res, sink, ph); break;
                            case JoinAlgorithm::Hash: hash_join(storage_, outer, inner, plan, res, sink, ph); break;
                        }
                    },
                    [&] { return estimate_join(plan, to, ti); });
}

Outcome Monitor::select_points(const AccessPath& path, std::span<const Point> batch, const ResultShape& shape,
                               const SelectParams& params) {
    const FieldId field = batch.empty() ? path.field : batch.front().field;
    for (const auto& pt : batch)
        if (pt.field != field) throw std::invalid_argument("lote com campos diferentes");
    if (path.kind != PathKind::Scan && field != path.field) return NotApplicable{"predicado fora do campo do caminho"};
    auto planned = plan_select(path, false, batch.size(), params);
    if (auto na = std::get_if<NotApplicable>(&planned)) return *na;
    SelectPlan plan = std::get<SelectPlan>(planned);
    plan.unique_field = storage_.schema(path.table).field(field).unique;
    const TableStats ts = storage_.stats(path.table);
    const StructureStats* st = find_structure(ts, path.id);
    std::vector<Point> points(batch.begin(), batch.end());   // o lote vem de arquivo, preparado antes (125)
    return measured(plan.m, plan.frames_used(), false, shape,
                    [&](const Reservation& res, ResultSink& sink, PhaseRecorder& ph) {
                        ph.begin("lote");
                        if (path.kind == PathKind::Scan) {
                            std::vector<KeyRange> keys;
                            keys.reserve(points.size());
                            for (const auto& pt : points) keys.push_back(as_range(pt));
                            select_scan(storage_, path.table, field, keys, plan.unique_field, res, sink);
                        } else if (path.kind == PathKind::SecondaryHash ||
                                   path.kind == PathKind::SecondaryBTreeNonClustering) {
                            for (const auto& pt : points) select_probe(storage_, path, pt, plan, res, sink);
                        } else {
                            for (const auto& pt : points) select_find(storage_, path, pt, kRandomOnce, res, sink);
                        }
                    },
                    [&] { return estimate_select(plan, ts, st); });
}

Outcome Monitor::select_range(const AccessPath& path, const KeyRange& range, const ResultShape& shape,
                              const SelectParams& params) {
    if (path.kind != PathKind::Scan && range.field != path.field) return NotApplicable{"predicado fora do campo do caminho"};
    auto planned = plan_select(path, true, 1, params);
    if (auto na = std::get_if<NotApplicable>(&planned)) return *na;
    SelectPlan plan = std::get<SelectPlan>(planned);
    plan.unique_field = storage_.schema(path.table).field(range.field).unique;
    const TableStats ts = storage_.stats(path.table);
    const StructureStats* st = find_structure(ts, path.id);
    return measured(plan.m, plan.frames_used(), false, shape,
                    [&](const Reservation& res, ResultSink& sink, PhaseRecorder& ph) {
                        ph.begin("faixa");
                        if (path.kind == PathKind::Scan) {
                            select_scan(storage_, path.table, range.field, {range}, false, res, sink);
                        } else if (path.kind == PathKind::SecondaryBTreeNonClustering) {
                            select_probe(storage_, path, range, plan, res, sink);
                        } else {
                            select_find(storage_, path, range, kSeqOnce, res, sink);
                        }
                    },
                    [&] { return estimate_select(plan, ts, st); });
}

Monitor::Monitor(const Config& cfg) : storage_(cfg) {}

}  // namespace cbd
