// Camada 1 — Bancada de teste: gera a base, monta as configurações, executa as operações com
// repetições e grava resultados, ocupação, fases, lotes, constantes e manifesto (129–133).
// Desvio do handoff: a especificação fica neste arquivo (spec_padrao), porque toml++ não está
// disponível no ambiente do protótipo; o formato TOML continua sendo a meta (129).
#include "layer2/monitor.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <tuple>

namespace fs = std::filesystem;
using namespace cbd;

namespace {

// ======================================================================= utilidades
struct Rng {   // splitmix64: determinístico em qualquer biblioteca padrão
    std::uint64_t s;
    explicit Rng(std::uint64_t seed) : s(seed) {}
    std::uint64_t next() {
        std::uint64_t z = (s += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    std::int64_t range(std::int64_t lo, std::int64_t hi) {   // [lo, hi]
        return lo + static_cast<std::int64_t>(next() % static_cast<std::uint64_t>(hi - lo + 1));
    }
    double unit() { return static_cast<double>(next() >> 11) / 9007199254740992.0; }
};

std::uint64_t str_hash(const std::string& s) { return fnv1a64(std::as_bytes(std::span(s.data(), s.size()))); }

std::string csv_escape(const std::string& s) {
    if (s.find_first_of(",\"\n") == std::string::npos) return s;
    std::string o = "\"";
    for (char c : s) o += (c == '"') ? std::string("\"\"") : std::string(1, c);
    return o + "\"";
}

class Csv {
public:
    explicit Csv(const fs::path& p) : out_(p) {
        if (!out_) throw std::runtime_error("não abriu " + p.string());
    }
    Csv& row(const std::vector<std::string>& cells) {
        for (std::size_t i = 0; i < cells.size(); ++i) out_ << (i ? "," : "") << csv_escape(cells[i]);
        out_ << '\n';
        return *this;
    }
    void flush() { out_.flush(); }
private:
    std::ofstream out_;
};

std::string value_text(const Value& v) {
    if (std::holds_alternative<std::monostate>(v)) return "\\N";
    if (auto p = std::get_if<std::int64_t>(&v)) return std::to_string(*p);
    return std::get<std::string>(v);
}
std::string fmt(double v, int prec = 3) {
    std::ostringstream os;
    os.setf(std::ios::fixed);
    os.precision(prec);
    os << v;
    return os.str();
}

// ======================================================================= base de dados (§6.4)
using Row = std::vector<Value>;
struct BaseTable {
    std::string            name;
    std::vector<FieldSpec> fields;
    std::vector<Row>       rows;
    Schema schema() const { return make_schema(fields); }
    std::size_t col(const std::string& f) const {
        for (std::size_t i = 0; i < fields.size(); ++i)
            if (fields[i].name == f) return i;
        throw std::out_of_range("coluna inexistente: " + f);
    }
};

std::string words(Rng& r, std::size_t max_len) {
    static const char* w[] = {"livros", "musica", "futebol", "xadrez", "cinema", "trilhas", "pintura", "teatro",
                              "corrida", "fotografia", "culinaria", "danca", "jardinagem", "robotica"};
    std::string s;
    const std::size_t target = static_cast<std::size_t>(r.range(1, static_cast<std::int64_t>(max_len)));
    while (s.size() < target) {
        std::string next = w[r.next() % 14];
        if (s.size() + next.size() + 1 > max_len) break;
        s += (s.empty() ? "" : " ") + next;
    }
    return s.empty() ? "nada" : s;
}

std::map<std::string, BaseTable> generate_base(double scale, std::uint64_t seed) {
    Rng r(seed);
    const std::int64_t n_aluno = std::max<std::int64_t>(50, std::llround(20000 * scale));
    const std::int64_t n_turma = std::max<std::int64_t>(30, std::llround(6000 * scale));
    const std::int64_t n_curso = 200, n_disc = 800, n_prof = 500;
    std::map<std::string, BaseTable> db;

    BaseTable curso{"curso", {{"codigo", FieldType::Int32, 0, true}, {"nome", FieldType::Char, 40},
                              {"escola", FieldType::Char, 30}, {"carga_horaria_total", FieldType::Int32},
                              {"coordenador", FieldType::Int32}, {"email", FieldType::Char, 40}}, {}};
    for (std::int64_t i = 1; i <= n_curso; ++i)
        curso.rows.push_back({i, "Curso " + std::to_string(i), "Escola " + std::to_string(i % 12),
                              r.range(2400, 4000), r.range(1, n_prof), "curso" + std::to_string(i) + "@ufrj.br"});

    BaseTable disc{"disciplina", {{"codigo", FieldType::Int32, 0, true}, {"curso", FieldType::Int32},
                                  {"nome", FieldType::Char, 40}, {"carga_horaria", FieldType::Int32}}, {}};
    for (std::int64_t i = 1; i <= n_disc; ++i)
        disc.rows.push_back({i, 1 + (i % n_curso), "Disciplina " + std::to_string(i), r.range(2, 6) * 15});

    BaseTable turma{"turma", {{"numero", FieldType::Int32, 0, true}, {"disciplina", FieldType::Int32},
                              {"semestre", FieldType::Int32}, {"sala", FieldType::Char, 8},
                              {"professor", FieldType::Int32}}, {}};
    for (std::int64_t i = 1; i <= n_turma; ++i)
        turma.rows.push_back({i, r.range(1, n_disc), r.range(0, 1) ? std::int64_t{20252} : std::int64_t{20251},
                              "S" + std::to_string(r.range(100, 999)), r.range(1, n_prof)});

    BaseTable aluno{"aluno", {{"dre", FieldType::Int32, 0, true}, {"cpf", FieldType::Char, 11, true},
                              {"nome", FieldType::Char, 40}, {"nascimento", FieldType::Int32},
                              {"sexo", FieldType::Char, 1}, {"curso", FieldType::Int32}, {"cr", FieldType::Int32},
                              {"professor_orientador", FieldType::Int32, 0, false, true},
                              {"logradouro", FieldType::Char, 32}, {"numero", FieldType::Int32},
                              {"cep", FieldType::Char, 8}, {"cidade", FieldType::Char, 10}}, {}};
    std::vector<std::int64_t> order(static_cast<std::size_t>(n_aluno));
    std::iota(order.begin(), order.end(), 0);
    for (std::size_t i = order.size(); i > 1; --i) std::swap(order[i - 1], order[r.next() % i]);   // ordem física aleatória
    std::vector<std::string> cpfs;
    for (std::int64_t i : order) {
        char cpf[16];
        std::snprintf(cpf, sizeof cpf, "%011llu",
                      static_cast<unsigned long long>((static_cast<std::uint64_t>(i) * 7919ull + 12345678901ull) % 100000000000ull));
        double cr = 650 + 150 * (r.unit() + r.unit() + r.unit() - 1.5) * 1.4;
        cr = std::clamp(cr, 0.0, 1000.0);
        const Value orient = r.range(1, 10) == 1 ? Value{} : Value{r.range(1, n_prof)};
        aluno.rows.push_back({120000000 + i, std::string(cpf), "Aluno " + std::to_string(i),
                              r.range(1995, 2006) * 10000 + r.range(1, 12) * 100 + r.range(1, 28),
                              std::string(r.range(0, 1) ? "F" : "M"), r.range(1, n_curso),
                              static_cast<std::int64_t>(std::llround(cr)), orient,
                              "Rua " + std::to_string(r.range(1, 900)), r.range(1, 3000),
                              std::to_string(r.range(20000000, 28999999)), "Rio"});
        cpfs.push_back(cpf);
    }

    BaseTable insc{"inscricao", {{"dre", FieldType::Int32}, {"turma", FieldType::Int32},
                                 {"frequencia", FieldType::Int32}, {"media", FieldType::Int32},
                                 {"semestre", FieldType::Int32}, {"obs", FieldType::Char, 44}}, {}};
    for (const auto& a : aluno.rows)
        for (int k = 0; k < 5; ++k)
            insc.rows.push_back({std::get<std::int64_t>(a[0]), r.range(1, n_turma), r.range(0, 100), r.range(0, 1000),
                                 r.range(0, 1) ? std::int64_t{20252} : std::int64_t{20251}, "obs " + std::to_string(k)});
    for (std::size_t i = insc.rows.size(); i > 1; --i) std::swap(insc.rows[i - 1], insc.rows[r.next() % i]);

    BaseTable det{"detalhes", {{"cpf", FieldType::Char, 11, true}, {"hobby", FieldType::VarChar, 40, false, true},
                               {"observacao", FieldType::VarChar, 200, false, true}}, {}};
    std::vector<std::size_t> dorder(cpfs.size());
    std::iota(dorder.begin(), dorder.end(), 0);
    for (std::size_t i = dorder.size(); i > 1; --i) std::swap(dorder[i - 1], dorder[r.next() % i]);
    for (auto i : dorder)
        det.rows.push_back({cpfs[i], r.range(1, 5) == 1 ? Value{} : Value{words(r, 40)},
                            r.range(1, 3) == 1 ? Value{} : Value{words(r, 200)}});

    for (auto* t : {&curso, &disc, &turma, &aluno, &insc, &det}) db[t->name] = std::move(*t);
    return db;
}

void write_base(const std::map<std::string, BaseTable>& db, const fs::path& dir) {
    fs::create_directories(dir / "dados");
    Csv schema(dir / "esquema.csv");
    schema.row({"tabela", "campo", "tipo"});
    for (const auto& [name, t] : db) {
        Csv out(dir / "dados" / (name + ".csv"));
        std::vector<std::string> header;
        for (const auto& f : t.fields) {
            header.push_back(f.name);
            const bool text = f.type == FieldType::Char || f.type == FieldType::VarChar;
            schema.row({name, f.name, text ? "text" : "int"});
        }
        out.row(header);
        for (const auto& row : t.rows) {
            std::vector<std::string> cells;
            for (const auto& v : row) cells.push_back(value_text(v));
            out.row(cells);
        }
    }
}

class RowsStream final : public RecordStream {   // registros codificados pela camada 3 (158)
public:
    RowsStream(Monitor& m, TableId t, const std::vector<Row>& rows, std::vector<std::size_t> order)
        : m_(m), t_(t), rows_(rows), order_(std::move(order)) {}
    std::optional<std::span<const std::byte>> next() override {
        if (i_ >= order_.size()) return std::nullopt;
        buf_ = m_.encode(t_, rows_[order_[i_++]]);
        return std::span<const std::byte>(buf_);
    }
private:
    Monitor& m_;
    TableId t_;
    const std::vector<Row>& rows_;
    std::vector<std::size_t> order_;
    std::size_t i_ = 0;
    std::vector<std::byte> buf_;
};

int value_cmp(const Value& a, const Value& b) {
    if (auto p = std::get_if<std::int64_t>(&a)) {
        const auto q = std::get<std::int64_t>(b);
        return *p < q ? -1 : (*p > q ? 1 : 0);
    }
    const int c = std::get<std::string>(a).compare(std::get<std::string>(b));
    return c < 0 ? -1 : (c > 0 ? 1 : 0);
}

Key to_key(const Value& v, FieldType t) {
    switch (t) {
        case FieldType::Int32: return static_cast<std::int32_t>(std::get<std::int64_t>(v));
        case FieldType::Int64: return std::get<std::int64_t>(v);
        default: return std::get<std::string>(v);
    }
}

// ======================================================================= especificação (129)
struct IndexDef { StructureKind kind; std::string field; bool clustering = false; };
struct VariantDef {
    std::string name, base;
    StructureKind kind = StructureKind::Heap;
    std::string key;
    std::vector<IndexDef> indexes;
};
struct PathRef {
    std::string table;
    PathKind    kind = PathKind::Scan;
    std::string field;
    std::string config;   // configuração da matriz do enunciado (a–e); nas junções, fica no JoinCase
    PathRef(std::string t, PathKind k, std::string f, std::string c = {})
        : table(std::move(t)), kind(k), field(std::move(f)), config(std::move(c)) {}
};
struct JoinCase { PathRef p0, p1; std::string config; };   // operando 0, operando 1
enum class OpKind { Join, Points, Range };
struct OpDef {
    std::string id, sql;
    OpKind kind;
    std::string tipo;                      // tipo do §5.1/§5.2 (natural_join, igualdade_unique...)
    std::string tamanho;                   // junção: menor_menor, menor_maior, maior_maior
    std::string seletividade;              // seleção: pontual, 0,1%, 50%
    std::vector<PathRef> paths;            // seleção: um caminho por variante
    std::string field;                     // seleção: campo do predicado
    double selectivity = 0;                // faixa
    std::string range_mode;                // "meio", "topo", "base", "valor"
    // junção
    std::vector<JoinCase> join_inputs;
    std::string field0, field1;
    bool also_swapped = false;
};
struct Spec {
    double scale = 1.0;
    std::uint64_t seed = 20261122;
    std::vector<Policy> policies{Policy::Lru, Policy::Mru};
    std::size_t m_join = 32, m_select = 2, batch = 1000;
    int min_reps = 3, max_reps = 10;
    double tolerance = 0.05;
    double fill = 0.7;
    std::vector<VariantDef> variants;
    std::vector<OpDef> ops;
    std::string only;
    bool tuples = false;   // --tuplas: grava as tuplas do motor (166)
};

Spec spec_padrao() {
    Spec s;
    using SK = StructureKind;
    s.variants = {
        {"curso_heap", "curso", SK::Heap, "", {}},
        {"disciplina_heap", "disciplina", SK::Heap, "", {}},
        {"turma_heap", "turma", SK::Heap, "", {{SK::SecBTree, "professor"}}},
        {"aluno_heap", "aluno", SK::Heap, "", {{SK::SecHash, "cpf"}, {SK::SecBTree, "professor_orientador"}, {SK::SecBTree, "cr"}}},
        {"aluno_bt_dre", "aluno", SK::BTree, "dre", {}},
        {"aluno_bt_curso", "aluno", SK::BTree, "curso", {{SK::SecBTree, "curso", true}}},
        {"aluno_h_dre", "aluno", SK::Hash, "dre", {}},
        {"aluno_h_curso", "aluno", SK::Hash, "curso", {{SK::SecBTree, "curso", true}}},
        {"inscricao_heap", "inscricao", SK::Heap, "", {{SK::SecBTree, "dre"}}},
        {"inscricao_bt_dre", "inscricao", SK::BTree, "dre", {}},
        {"inscricao_h_dre", "inscricao", SK::Hash, "dre", {}},
        {"detalhes_heap", "detalhes", SK::Heap, "", {}},
    };
    using PK = PathKind;
    auto sel = [](std::string id, std::string tipo, std::string sel_label, OpKind k, std::string field, std::string sql,
                  std::vector<PathRef> paths, double selv = 0, std::string mode = "") {
        OpDef o;
        o.id = std::move(id); o.tipo = std::move(tipo); o.seletividade = std::move(sel_label);
        o.kind = k; o.field = std::move(field); o.sql = std::move(sql);
        o.paths = std::move(paths); o.selectivity = selv; o.range_mode = std::move(mode);
        return o;
    };
    const std::string pts = "SELECT a.* FROM lote l JOIN aluno a ON a.{f} = l.k";
    // igualdade em UNIQUE: nível próprio ("pontual"); 0,1% e 50% são o caso 3 (170)
    s.ops.push_back(sel("sel_pt_dre", "igualdade_unique", "pontual", OpKind::Points, "dre", pts,
                        {{"aluno_heap", PK::Scan, "", "a"}, {"aluno_bt_dre", PK::PrimaryBTree, "dre", "b"},
                         {"aluno_h_dre", PK::PrimaryHash, "dre", "b"}}));
    s.ops.push_back(sel("sel_pt_cpf", "igualdade_unique", "pontual", OpKind::Points, "cpf", pts,
                        {{"aluno_heap", PK::Scan, "", "a"}, {"aluno_heap", PK::SecondaryHash, "cpf", "c"}}));
    const std::string rg = "SELECT a.* FROM aluno a WHERE a.{f} >= {lo} AND a.{f} <= {hi}";
    for (auto [suf, sv, lbl] : {std::tuple{"baixa", 0.001, "0,1%"}, std::tuple{"alta", 0.5, "50%"}}) {
        // faixa em UNIQUE: o hash primário sai como caso 4; o índice secundário (c) é o caso 2 (170)
        s.ops.push_back(sel(std::string("sel_rg_dre_") + suf, "faixa_unique", lbl, OpKind::Range, "dre", rg,
                            {{"aluno_heap", PK::Scan, "", "a"}, {"aluno_bt_dre", PK::PrimaryBTree, "dre", "b"},
                             {"aluno_h_dre", PK::PrimaryHash, "dre", "b"}}, sv, "meio"));
        s.ops.push_back(sel(std::string("sel_rg_cr_") + suf, "faixa_not_unique", lbl, OpKind::Range, "cr", rg,
                            {{"aluno_heap", PK::Scan, "", "a"}, {"aluno_heap", PK::SecondaryBTreeNonClustering, "cr", "c"}},
                            sv, sv < 0.01 ? "topo" : "base"));
        s.ops.push_back(sel(std::string("sel_rg_curso_") + suf, "faixa_not_unique", lbl, OpKind::Range, "curso", rg,
                            {{"aluno_heap", PK::Scan, "", "a"}, {"aluno_bt_curso", PK::PrimaryBTree, "curso", "b"},
                             {"aluno_bt_curso", PK::SecondaryBTreeClustering, "curso", "c"},
                             {"aluno_h_curso", PK::SecondaryBTreeClustering, "curso", "c"},
                             {"aluno_h_curso", PK::PrimaryHash, "curso", "b"}}, sv, sv < 0.01 ? "valor" : "base"));
    }
    auto join = [](std::string id, std::string tipo, std::string tamanho, std::string sql, std::string f0, std::string f1,
                   std::vector<JoinCase> inputs, bool swapped = false) {
        OpDef o;
        o.id = std::move(id); o.tipo = std::move(tipo); o.tamanho = std::move(tamanho);
        o.kind = OpKind::Join; o.sql = std::move(sql);
        o.field0 = std::move(f0); o.field1 = std::move(f1); o.join_inputs = std::move(inputs);
        o.also_swapped = swapped;
        return o;
    };
    s.ops.push_back(join("j_mestre_detalhe", "natural_join", "maior_maior",
                         "SELECT x.*, y.* FROM aluno x JOIN inscricao y ON x.dre = y.dre", "dre", "dre",
                         {{{"aluno_heap", PK::Scan, ""}, {"inscricao_heap", PK::Scan, ""}, "a"},
                          {{"aluno_bt_dre", PK::PrimaryBTree, "dre"}, {"inscricao_heap", PK::Scan, ""}, "b"},
                          {{"aluno_bt_dre", PK::PrimaryBTree, "dre"}, {"inscricao_bt_dre", PK::PrimaryBTree, "dre"}, "c"},
                          {{"aluno_h_dre", PK::PrimaryHash, "dre"}, {"inscricao_h_dre", PK::PrimaryHash, "dre"}, "c"},
                          {{"aluno_bt_dre", PK::PrimaryBTree, "dre"}, {"inscricao_heap", PK::SecondaryBTreeNonClustering, "dre"}, "d"}},
                         true));
    s.ops.push_back(join("j_pequenas", "natural_join", "menor_menor",
                         "SELECT x.*, y.* FROM curso x JOIN disciplina y ON x.codigo = y.curso", "codigo", "curso",
                         {{{"curso_heap", PK::Scan, ""}, {"disciplina_heap", PK::Scan, ""}, "a"}}));
    s.ops.push_back(join("j_grande_pequena", "natural_join", "menor_maior",
                         "SELECT x.*, y.* FROM curso x JOIN aluno y ON x.codigo = y.curso", "codigo", "curso",
                         {{{"curso_heap", PK::Scan, ""}, {"aluno_heap", PK::Scan, ""}, "a"},
                          {{"curso_heap", PK::Scan, ""}, {"aluno_bt_curso", PK::PrimaryBTree, "curso"}, "b"}}));
    // tipos 2 e 3: só as configurações a e e existem; b, c e d são o caso 1 (170)
    s.ops.push_back(join("j_unique", "equijoin_unique", "maior_maior",
                         "SELECT x.*, y.* FROM aluno x JOIN detalhes y ON x.cpf = y.cpf", "cpf", "cpf",
                         {{{"aluno_heap", PK::Scan, ""}, {"detalhes_heap", PK::Scan, ""}, "a"}}));
    s.ops.push_back(join("j_not_unique", "equijoin_not_unique", "maior_maior",
                         "SELECT x.*, y.* FROM aluno x JOIN turma y ON x.professor_orientador = y.professor",
                         "professor_orientador", "professor",
                         {{{"aluno_heap", PK::SecondaryBTreeNonClustering, "professor_orientador"},
                           {"turma_heap", PK::SecondaryBTreeNonClustering, "professor"}, "e"}}));
    return s;
}

// ======================================================================= montagem de uma configuração
struct Built {
    std::map<std::string, TableId> tables;
    std::map<std::string, std::string> base_of;
};

std::uint64_t distinct_count(const BaseTable& t, std::size_t col) {
    std::set<std::string> seen;
    for (const auto& r : t.rows) seen.insert(value_text(r[col]));
    return seen.size();
}

Built build_all(Monitor& mon, const Spec& spec, const std::map<std::string, BaseTable>& db) {
    Built b;
    for (const auto& v : spec.variants) {
        const BaseTable& base = db.at(v.base);
        const Schema schema = base.schema();
        PrimarySpec p;
        p.kind = v.kind;
        p.fill = spec.fill;
        std::vector<std::size_t> order(base.rows.size());
        std::iota(order.begin(), order.end(), 0);
        if (v.kind != StructureKind::Heap) {
            p.key = schema.find(v.key);
            const std::size_t col = base.col(v.key);
            if (v.kind == StructureKind::Hash)
                p.buckets = mon.suggest_buckets(schema, base.rows.size(), distinct_count(base, col), 0.8);   // (39, 163)
            if (v.kind == StructureKind::BTree)
                std::stable_sort(order.begin(), order.end(), [&](std::size_t x, std::size_t y) {
                    return value_cmp(base.rows[x][col], base.rows[y][col]) < 0;
                });
        }
        const TableId t = mon.create_table(v.name, schema, p);
        for (const auto& ix : v.indexes) mon.create_index(t, IndexSpec{ix.kind, schema.find(ix.field), ix.clustering});
        RowsStream rs(mon, t, base.rows, std::move(order));
        mon.bulk_load(t, rs);
        b.tables[v.name] = t;
        b.base_of[v.name] = v.base;
    }
    return b;
}

AccessPath resolve(Monitor& mon, const Built& b, const PathRef& ref) {
    const TableId t = b.tables.at(ref.table);
    for (const auto& p : mon.access_paths(t)) {
        if (p.kind != ref.kind) continue;
        if (ref.kind == PathKind::Scan || mon.schema(t).field(p.field).name == ref.field) return p;
    }
    throw std::invalid_argument("caminho inexistente: " + ref.table + ":" + to_string(ref.kind) + ":" + ref.field);
}

// ======================================================================= constantes e lotes (125, 130)
struct RangeConst { Value lo, hi; std::uint64_t expected = 0; };

RangeConst resolve_range(const BaseTable& t, const std::string& field, double sel, const std::string& mode) {
    const std::size_t col = t.col(field);
    std::vector<Value> v;
    for (const auto& r : t.rows)
        if (!std::holds_alternative<std::monostate>(r[col])) v.push_back(r[col]);
    std::sort(v.begin(), v.end(), [](const Value& a, const Value& c) { return value_cmp(a, c) < 0; });
    const std::size_t n = v.size();
    const std::size_t k = std::max<std::size_t>(1, static_cast<std::size_t>(std::llround(sel * static_cast<double>(n))));
    RangeConst rc;
    if (mode == "meio") { rc.lo = v[n / 2 - k / 2]; rc.hi = v[std::min(n - 1, n / 2 - k / 2 + k - 1)]; }
    else if (mode == "topo") { rc.lo = v[n - k]; rc.hi = v[n - 1]; }
    else if (mode == "base") { rc.lo = v[0]; rc.hi = v[k - 1]; }
    else { rc.lo = v[n / 2]; rc.hi = v[n / 2]; }   // "valor": um único valor (≈ 1/d)
    for (const auto& x : v)
        if (value_cmp(x, rc.lo) >= 0 && value_cmp(x, rc.hi) <= 0) ++rc.expected;
    return rc;
}

std::string replace_all(std::string s, const std::string& from, const std::string& to) {
    for (std::size_t p = 0; (p = s.find(from, p)) != std::string::npos; p += to.size()) s.replace(p, from.size(), to);
    return s;
}
std::string sql_literal(const Value& v) {
    if (auto p = std::get_if<std::int64_t>(&v)) return std::to_string(*p);
    return "'" + std::get<std::string>(v) + "'";
}

// ======================================================================= execução
struct Measured {
    bool ok = false;
    bool infeasible = false;   // inviável em M (170); senão, não aplicável
    std::string reason;
    OpResult r;
};
struct Cell { std::string tipo, tamanho, config, seletividade; };   // posição na matriz do enunciado

Measured run_one(const std::function<Outcome()>& f) {
    Outcome o = f();
    Measured m;
    if (auto na = std::get_if<NotApplicable>(&o)) {
        m.reason = na->reason;
        m.infeasible = na->kind == NotApplicable::Kind::Inviavel;
        return m;
    }
    m.ok = true;
    m.r = std::get<OpResult>(std::move(o));
    return m;
}

std::vector<std::string> result_header() {
    return {"politica", "operacao", "tipo", "tamanho", "config", "seletividade", "variante", "algoritmo", "externa",
            "interna", "m", "rep", "status", "motivo",
            "linhas", "checksum", "requisicoes", "acertos", "faltas", "taxa_acerto", "leituras_seq", "leituras_aleat",
            "gravacoes_seq", "gravacoes_aleat", "leituras", "gravacoes", "expulsoes_sujas", "gravacoes_forcadas",
            "alocacoes", "leituras_indice", "leituras_temp", "gravacoes_temp", "estimado_transf", "estimado_seeks",
            "flush_writes", "tempo_ms"};
}

std::string status_of(const Measured& x) { return x.ok ? "ok" : (x.infeasible ? "inviavel" : "nao_aplicavel"); }

std::vector<std::string> result_row(const std::string& pol, const std::string& op, const Cell& cell,
                                    const std::string& variant, const std::string& alg, const std::string& outer,
                                    const std::string& inner, std::size_t m, int rep, const Measured& x) {
    std::vector<std::string> c{pol, op, cell.tipo, cell.tamanho, cell.config, cell.seletividade, variant, alg, outer,
                               inner, std::to_string(m), std::to_string(rep), status_of(x), x.reason};
    if (!x.ok) {
        c.resize(result_header().size());
        return c;
    }
    const auto& g = x.r.delta.global;
    const auto& ix = x.r.delta.per_kind[static_cast<std::size_t>(FileKind::Index)];
    const auto& tp = x.r.delta.per_kind[static_cast<std::size_t>(FileKind::Temp)];
    for (auto v : {x.r.rows, x.r.checksum, g.requests, g.hits, g.misses}) c.push_back(std::to_string(v));
    c.push_back(fmt(g.hit_rate(), 4));
    for (auto v : {g.reads_seq, g.reads_rand, g.writes_seq, g.writes_rand, g.reads(), g.writes(), g.dirty_evictions,
                   g.forced_writes, g.allocations, ix.reads(), tp.reads(), tp.writes()})
        c.push_back(std::to_string(v));
    c.push_back(fmt(x.r.estimate.transfers, 1));
    c.push_back(fmt(x.r.estimate.seeks, 1));
    c.push_back(std::to_string(x.r.flush_writes));
    c.push_back(fmt(static_cast<double>(x.r.wall_time.count()) / 1e6, 3));
    return c;
}

// repete até a variação ficar abaixo da tolerância (mín. min_reps, máx. max_reps) (132, emendada):
// as contagens de blocos são determinísticas fora dos lotes, então a regra vale também para o tempo
void repeat(const Spec& spec, const std::function<Measured(int rep)>& once,
            const std::function<void(int, const Measured&)>& record) {
    std::vector<double> io, ms;
    auto spread = [&](const std::vector<double>& v) {
        const auto [mn, mx] = std::minmax_element(v.end() - spec.min_reps, v.end());
        const double mean = std::accumulate(v.end() - spec.min_reps, v.end(), 0.0) / spec.min_reps;
        return mean == 0 ? 0.0 : (*mx - *mn) / mean;
    };
    for (int rep = 1; rep <= spec.max_reps; ++rep) {
        const Measured m = once(rep);
        record(rep, m);
        if (!m.ok) return;
        io.push_back(static_cast<double>(m.r.delta.global.reads() + m.r.delta.global.writes()));
        ms.push_back(static_cast<double>(m.r.wall_time.count()));
        if (rep >= spec.min_reps && spread(io) < spec.tolerance && spread(ms) < spec.tolerance) return;
    }
}

void write_occupancy(Monitor& mon, const Built& b, Csv& out, const std::string& pol, const std::string& when) {
    for (const auto& [name, t] : b.tables) {
        for (StructureId sid : mon.structures(t)) {
            const Occupancy o = mon.occupancy(sid);
            std::string levels;
            for (auto l : o.index_levels) levels += (levels.empty() ? "" : "|") + std::to_string(l);
            std::uint32_t total = o.data + o.overflow + o.header + o.directory;
            for (auto l : o.index_levels) total += l;
            out.row({pol, when, name, mon.structure_name(sid), std::to_string(o.data), std::to_string(o.overflow),
                     std::to_string(o.header), std::to_string(o.directory), levels, std::to_string(total)});
        }
    }
}

void write_stats(Monitor& mon, const Built& b, Csv& out, const std::string& pol) {
    for (const auto& [name, t] : b.tables) {
        const TableStats s = mon.stats(t);
        for (const auto& st : s.structures) {
            auto opt = [](const auto& o) { return o ? std::to_string(*o) : std::string(); };
            auto optd = [](const std::optional<double>& o) { return o ? fmt(*o, 3) : std::string(); };
            out.row({pol, name, mon.structure_name(st.id), std::to_string(s.n), std::to_string(s.r),
                     std::to_string(s.bfr), std::to_string(s.b), std::to_string(st.blocks), opt(st.height),
                     opt(st.internal_blocks), opt(st.leaves), opt(st.distinct), opt(st.buckets),
                     opt(st.overflow_blocks), optd(st.load_factor), optd(st.chain_avg), opt(st.chain_max)});
        }
    }
}

// Casos 1 a 3 (170): células que o enunciado pede mas que não existem; entram no resultado sem executar.
void declare_not_applicable(Csv& out, const std::string& pol, const Spec& spec) {
    auto row = [&](const std::string& op, const Cell& cell, const std::string& alg, const std::string& why) {
        if (!spec.only.empty() && op.find(spec.only) == std::string::npos) return;
        std::vector<std::string> c{pol, op, cell.tipo, cell.tamanho, cell.config, cell.seletividade, "", alg, "", "", "",
                                   "", "nao_aplicavel", why};
        c.resize(result_header().size());
        out.row(c);
    };
    const char* algs[] = {"bnl", "sort_merge", "hash"};
    const std::string caso1 =
        "caso 1: o tipo exige campos que não ditam a organização primária, e a configuração exige que um "
        "ou os dois campos a ditem (§5.1; 170)";
    for (auto [op, tipo] : {std::pair{"j_unique", "equijoin_unique"}, std::pair{"j_not_unique", "equijoin_not_unique"}})
        for (const char* tam : {"menor_menor", "menor_maior", "maior_maior"})
            for (const char* cfg : {"b", "c", "d"})
                for (const char* alg : algs) row(op, Cell{tipo, tam, cfg, ""}, alg, caso1);
    for (auto [op, lbl] : {std::pair{"sel_rg_dre_baixa", "0,1%"}, std::pair{"sel_rg_dre_alta", "50%"}})
        row(op, Cell{"faixa_unique", "", "c", lbl}, "",
            "caso 2: o §4.2 só admite hash como secundário de campo UNIQUE (heap) e só clustering NOT UNIQUE "
            "nas outras organizações; hash não responde a faixa (170)");
    for (const char* lbl : {"0,1%", "50%"})
        for (const char* cfg : {"a", "b", "c"})
            row("sel_igualdade_unique", Cell{"igualdade_unique", "", cfg, lbl}, "",
                "caso 3: igualdade em UNIQUE devolve 1 registro (s = 1/N); medida no nível pontual (170)");
}

int usage() {
    std::cerr << "uso: bancada [--saida DIR] [--escala X] [--reps-min N] [--reps-max N] [--tolerancia X]\n"
                 "               [--politicas lru,mru] [--m-juncao N] [--m-selecao N] [--lote N] [--so TEXTO]\n"
                 "               [--rapido] [--tuplas]\n";
    return 2;
}

}  // namespace

int main(int argc, char** argv) {
    Spec spec = spec_padrao();
    fs::path out_dir = "saida";
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto val = [&]() -> std::string {
            if (i + 1 >= argc) throw std::invalid_argument("falta valor para " + a);
            return argv[++i];
        };
        if (a == "--saida") out_dir = val();
        else if (a == "--escala") spec.scale = std::stod(val());
        else if (a == "--reps-min") spec.min_reps = std::stoi(val());
        else if (a == "--reps-max") spec.max_reps = std::stoi(val());
        else if (a == "--tolerancia") spec.tolerance = std::stod(val());
        else if (a == "--m-juncao") spec.m_join = std::stoul(val());
        else if (a == "--m-selecao") spec.m_select = std::stoul(val());
        else if (a == "--lote") spec.batch = std::stoul(val());
        else if (a == "--so") spec.only = val();
        else if (a == "--tuplas") spec.tuples = true;
        else if (a == "--politicas") {
            spec.policies.clear();
            const std::string v = val();
            if (v.find("lru") != std::string::npos) spec.policies.push_back(Policy::Lru);
            if (v.find("mru") != std::string::npos) spec.policies.push_back(Policy::Mru);
        } else if (a == "--rapido") {
            spec.scale = 0.1; spec.min_reps = 1; spec.max_reps = 1; spec.batch = 100;
        } else return usage();
    }
    if (spec.min_reps < 1 || spec.max_reps < spec.min_reps) return usage();

    // load_spec: validações (129)
    const std::uint32_t work = Config{}.area_frames[3];
    if (spec.m_join > work || spec.m_select > work) {
        std::cerr << "m maior que a área de trabalho (" << work << ")\n";
        return 2;
    }

    fs::create_directories(out_dir);
    const auto t_start = std::chrono::steady_clock::now();
    std::cout << "gerando a base (escala " << spec.scale << ")...\n";
    const auto db = generate_base(spec.scale, spec.seed);
    write_base(db, out_dir);

    // constantes, lotes e consultas equivalentes (125, 130, 147)
    Csv opcsv(out_dir / "operacoes.csv");
    opcsv.row({"operacao", "tipo", "sql", "lote", "tipo_lote"});
    Csv consts(out_dir / "constantes.csv");
    consts.row({"operacao", "lo", "hi", "linhas_esperadas"});
    fs::create_directories(out_dir / "lotes");
    std::map<std::string, RangeConst> ranges;
    std::map<std::pair<std::string, int>, std::vector<Value>> batches;
    for (const auto& op : spec.ops) {
        if (op.kind == OpKind::Join) {
            opcsv.row({op.id, "juncao", op.sql, "", ""});
        } else if (op.kind == OpKind::Range) {
            const auto rc = resolve_range(db.at("aluno"), op.field, op.selectivity, op.range_mode);
            ranges[op.id] = rc;
            std::string sql = replace_all(op.sql, "{f}", op.field);
            sql = replace_all(sql, "{lo}", sql_literal(rc.lo));
            sql = replace_all(sql, "{hi}", sql_literal(rc.hi));
            opcsv.row({op.id, "faixa", sql, "", ""});
            consts.row({op.id, value_text(rc.lo), value_text(rc.hi), std::to_string(rc.expected)});
        } else {
            const BaseTable& a = db.at("aluno");
            const std::size_t col = a.col(op.field);
            const bool text = a.fields[col].type == FieldType::Char || a.fields[col].type == FieldType::VarChar;
            opcsv.row({op.id, "pontual", replace_all(op.sql, "{f}", op.field), "lotes/" + op.id, text ? "text" : "int"});
            for (int rep = 1; rep <= spec.max_reps; ++rep) {   // a semente muda a cada repetição (132)
                Rng r(spec.seed ^ str_hash(op.id) ^ (static_cast<std::uint64_t>(rep) * 0x9E37ull));
                std::vector<Value> keys;
                for (std::size_t i = 0; i < spec.batch; ++i) keys.push_back(a.rows[r.next() % a.rows.size()][col]);
                Csv lote(out_dir / "lotes" / (op.id + "_" + std::to_string(rep) + ".csv"));
                lote.row({"k"});
                for (const auto& k : keys) lote.row({value_text(k)});
                batches[{op.id, rep}] = std::move(keys);
            }
        }
    }
    opcsv.flush();
    consts.flush();

    Csv results(out_dir / "resultados.csv");
    results.row(result_header());
    Csv occ(out_dir / "ocupacao.csv");
    occ.row({"politica", "momento", "tabela", "estrutura", "dados", "overflow", "cabecalho", "diretorio", "niveis_indice", "total"});
    Csv stats(out_dir / "estatisticas.csv");
    stats.row({"politica", "tabela", "estrutura", "n", "r", "bfr", "b", "blocos", "altura", "blocos_internos", "folhas",
               "distintos", "buckets", "overflow", "fator_carga", "cadeia_media", "cadeia_max"});
    Csv phases(out_dir / "fases.csv");
    phases.row({"politica", "operacao", "variante", "algoritmo", "rep", "fase", "leituras", "gravacoes", "requisicoes"});

    for (Policy pol : spec.policies) {
        const std::string pname = to_string(pol);
        Config cfg;
        cfg.policy = pol;
        cfg.data_dir = out_dir / ("motor_" + pname);
        fs::remove_all(cfg.data_dir);   // cada configuração começa num data_dir vazio (161)
        std::cout << "[" << pname << "] construindo as tabelas...\n";
        Monitor mon(cfg);
        const Built built = build_all(mon, spec, db);
        mon.flush_all();   // faz parte da carga; não é medido (14)
        write_occupancy(mon, built, occ, pname, "antes");
        write_stats(mon, built, stats, pname);
        declare_not_applicable(results, pname, spec);

        auto shape_for = [&](const std::string& op, const std::string& variant, const std::string& alg, int rep) {
            ResultShape sh;
            if (!spec.tuples) return sh;
            std::string v = variant;
            for (char& ch : v) if (!std::isalnum(static_cast<unsigned char>(ch))) ch = '_';
            fs::create_directories(out_dir / "tuplas_motor");
            sh.dump = out_dir / "tuplas_motor" / (pname + "__" + op + "__" + v + "_" + alg + "__" + std::to_string(rep) + ".txt");
            fs::remove(*sh.dump);
            return sh;
        };

        auto prepare = [&] {   // estado entre repetições (110)
            if (const auto w = mon.flush_all(); w != 0) std::cerr << "aviso: flush antes da repetição gravou " << w << "\n";
            mon.invalidate_all();
        };

        for (const auto& op : spec.ops) {
            if (!spec.only.empty() && op.id.find(spec.only) == std::string::npos) continue;
            std::cout << "[" << pname << "] " << op.id << "\n";
            if (op.kind != OpKind::Join) {
                for (const auto& ref : op.paths) {
                    const AccessPath path = resolve(mon, built, ref);
                    const TableId t = built.tables.at(ref.table);
                    const FieldId f = mon.schema(t).find(op.field);
                    const FieldType ft = mon.schema(t).field(f).type;
                    const std::string variant = ref.table + ":" + to_string(ref.kind);
                    const Cell cell{op.tipo, op.tamanho, ref.config, op.seletividade};
                    SelectParams sp;
                    sp.m = spec.m_select;
                    repeat(spec,
                           [&](int rep) {
                               prepare();
                               if (op.kind == OpKind::Points) {
                                   std::vector<Point> pts;
                                   for (const auto& v : batches.at({op.id, rep})) pts.push_back(Point{f, to_key(v, ft)});
                                   sp.expected_rows = static_cast<double>(pts.size());
                                   return run_one([&] { return mon.select_points(path, pts, shape_for(op.id, variant, "sel", rep), sp); });
                               }
                               const RangeConst& rc = ranges.at(op.id);
                               KeyRange kr{f, to_key(rc.lo, ft), to_key(rc.hi, ft)};
                               sp.expected_rows = static_cast<double>(rc.expected);
                               return run_one([&] { return mon.select_range(path, kr, shape_for(op.id, variant, "sel", rep), sp); });
                           },
                           [&](int rep, const Measured& m) {
                               results.row(result_row(pname, op.id, cell, variant, "", ref.table, "", spec.m_select, rep, m));
                               if (m.ok)
                                   for (const auto& ph : m.r.phases)
                                       phases.row({pname, op.id, variant, "", std::to_string(rep), ph.name,
                                                   std::to_string(ph.delta.global.reads()),
                                                   std::to_string(ph.delta.global.writes()),
                                                   std::to_string(ph.delta.global.requests)});
                           });
                }
                continue;
            }
            for (const auto& jc : op.join_inputs) {
                const PathRef& r0 = jc.p0;
                const PathRef& r1 = jc.p1;
                JoinInput in0{resolve(mon, built, r0), 0, 0}, in1{resolve(mon, built, r1), 0, 1};
                in0.field = mon.schema(in0.path.table).find(op.field0);
                in1.field = mon.schema(in1.path.table).find(op.field1);
                // tamanho calculado (emenda da 108): menor é a tabela com b ≤ m − 2
                const auto is_small = [&](const JoinInput& in) { return mon.stats(in.path.table).b + 2 <= spec.m_join; };
                const int smalls = (is_small(in0) ? 1 : 0) + (is_small(in1) ? 1 : 0);
                const std::string tamanho = smalls == 2 ? "menor_menor" : (smalls == 1 ? "menor_maior" : "maior_maior");
                if (tamanho != op.tamanho)
                    std::cerr << "aviso: " << op.id << " foi declarada " << op.tamanho << ", mas as tabelas dão " << tamanho << "\n";
                const Cell cell{op.tipo, tamanho, jc.config, ""};
                const std::string variant = r0.table + ":" + to_string(r0.kind) + " x " + r1.table + ":" + to_string(r1.kind);
                for (JoinAlgorithm alg : {JoinAlgorithm::Bnl, JoinAlgorithm::SortMerge, JoinAlgorithm::Hash}) {
                    // externa e interna (122): BNL e hash, a menor por fora; sort-merge, a UNIQUE por dentro
                    const auto b0 = mon.stats(in0.path.table).b, b1 = mon.stats(in1.path.table).b;
                    bool first_outer = b0 <= b1;
                    if (alg == JoinAlgorithm::SortMerge) {
                        const bool u0 = mon.schema(in0.path.table).field(in0.field).unique;
                        const bool u1 = mon.schema(in1.path.table).field(in1.field).unique;
                        if (u0 != u1) first_outer = u1;
                    }
                    std::vector<bool> orders{first_outer};
                    if (op.also_swapped && r0.kind == PathKind::Scan && r1.kind == PathKind::Scan) orders.push_back(!first_outer);
                    for (bool fo : orders) {
                        const JoinInput& outer = fo ? in0 : in1;
                        const JoinInput& inner = fo ? in1 : in0;
                        const std::string vname = variant + (fo == first_outer ? "" : " (papeis trocados)");
                        repeat(spec,
                               [&](int rep) {
                                   prepare();
                                   const ResultShape sh = shape_for(op.id, vname, to_string(alg), rep);
                                   return run_one([&] { return mon.join(alg, outer, inner, spec.m_join, sh); });
                               },
                               [&](int rep, const Measured& m) {
                                   const auto& to = fo ? r0.table : r1.table;
                                   const auto& ti = fo ? r1.table : r0.table;
                                   results.row(result_row(pname, op.id, cell, vname, to_string(alg), to, ti, spec.m_join, rep, m));
                                   if (m.ok)
                                       for (const auto& ph : m.r.phases)
                                           phases.row({pname, op.id, vname, to_string(alg), std::to_string(rep), ph.name,
                                                       std::to_string(ph.delta.global.reads()),
                                                       std::to_string(ph.delta.global.writes()),
                                                       std::to_string(ph.delta.global.requests)});
                               });
                    }
                }
            }
            results.flush();
        }

        // carga de manutenção (131): 5% de inserções e 1% de remoções nas variantes de aluno
        std::cout << "[" << pname << "] carga de manutenção...\n";
        const BaseTable& a = db.at("aluno");
        Rng mr(spec.seed + 7);
        const auto n_ins = static_cast<std::size_t>(a.rows.size() * 0.05);
        const auto n_del = static_cast<std::size_t>(a.rows.size() * 0.01);
        std::vector<Value> victims;
        for (std::size_t i = 0; i < n_del; ++i) victims.push_back(a.rows[mr.next() % a.rows.size()][0]);
        std::sort(victims.begin(), victims.end(), [](const Value& x, const Value& y) { return value_cmp(x, y) < 0; });
        victims.erase(std::unique(victims.begin(), victims.end(),
                                  [](const Value& x, const Value& y) { return value_cmp(x, y) == 0; }), victims.end());
        for (const auto& [name, t] : built.tables) {
            if (built.base_of.at(name) != "aluno") continue;
            for (std::size_t i = 0; i < n_ins; ++i) {
                Row row = a.rows[i % a.rows.size()];
                const std::int64_t id = 900000000 + static_cast<std::int64_t>(i);
                row[0] = id;
                char cpf[16];
                std::snprintf(cpf, sizeof cpf, "9%010lld", static_cast<long long>(i));
                row[1] = std::string(cpf);
                mon.insert(t, mon.encode(t, row));
            }
            for (const auto& v : victims) mon.erase_key(t, 0, to_key(v, FieldType::Int32));
            if (const auto err = mon.verify(t); !err.empty()) std::cerr << "verificação falhou em " << name << ": " << err << "\n";
        }
        mon.flush_all();
        write_occupancy(mon, built, occ, pname, "depois");
    }

    // manifesto (133)
    std::ofstream man(out_dir / "manifesto.txt");
    const std::time_t now = std::time(nullptr);
    man << "data: " << std::put_time(std::localtime(&now), "%Y-%m-%d %H:%M:%S") << "\n";
    man << "compilador: " << __VERSION__ << "\n";
#ifdef CBD_FLAGS
    man << "flags: " << CBD_FLAGS << "\n";
#endif
    man << "commit: " << (std::getenv("CBD_COMMIT") ? std::getenv("CBD_COMMIT") : "sem git") << "\n";
    man << "escala: " << spec.scale << "\nsemente: " << spec.seed << "\nreps: " << spec.min_reps << ".." << spec.max_reps
        << "\ntolerancia: " << spec.tolerance << "\nm_juncao: " << spec.m_join << "\nm_selecao: " << spec.m_select
        << "\nlote: " << spec.batch << "\n";
    std::string desc;
    for (const auto& op : spec.ops) desc += op.id + ";" + op.sql + "\n";
    for (const auto& v : spec.variants) desc += v.name + ";" + to_string(v.kind) + ";" + v.key + "\n";
    man << "hash_especificacao: " << std::hex << str_hash(desc) << std::dec << "\n";
    const auto secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t_start).count();
    man << "duracao_s: " << fmt(secs, 1) << "\n";
    std::cout << "pronto em " << fmt(secs, 1) << " s. Resultados em " << out_dir << "\n";
    return 0;
}
