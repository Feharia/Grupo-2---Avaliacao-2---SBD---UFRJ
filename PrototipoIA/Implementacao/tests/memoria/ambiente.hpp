// Ambiente comum dos testes de memória das junções: as mesmas tabelas num Monitor, que planeja, e
// num StorageManager, onde o algoritmo roda direto com o contador armado só nele.
#pragma once

#include "contador.hpp"
#include "helpers.hpp"
#include "layer2/algorithms.hpp"

#include <map>
#include <string>
#include <vector>

namespace memoria {

using namespace cbd;

inline std::uint64_t lcg(std::uint64_t& s) { return s = s * 6364136223846793005ull + 1442695040888963407ull, s >> 33; }

class Linhas final : public RecordStream {
public:
    Linhas(const StorageManager& sm, TableId t, const std::vector<std::vector<Value>>& rows) : sm_(sm), t_(t), rows_(rows) {}
    std::optional<std::span<const std::byte>> next() override {
        if (i_ >= rows_.size()) return std::nullopt;
        buf_ = sm_.encode(t_, rows_[i_++]);
        return std::span<const std::byte>(buf_);
    }
private:
    const StorageManager& sm_;
    TableId t_;
    const std::vector<std::vector<Value>>& rows_;
    std::size_t i_ = 0;
    std::vector<std::byte> buf_;
};

struct Caso {
    const char* nome;
    Schema schema;
    std::vector<std::vector<Value>> externa, interna;
};

// Carrega as duas tabelas (heap) num StorageManager e num Monitor com os mesmos dados: o Monitor
// planeja, e o bnl_join roda direto no StorageManager, com o contador armado só nele.
struct Ambiente {
    Monitor mon;
    StorageManager sm;
    TableId me, mi, se, si;
    Ambiente(const std::string& nome, const Caso& c)
        : mon(test_config(nome + "_mon")), sm(test_config(nome + "_sm")) {
        me = mon.create_table("externa", c.schema, PrimarySpec{});
        mi = mon.create_table("interna", c.schema, PrimarySpec{});
        se = sm.create_table("externa", c.schema, PrimarySpec{});
        si = sm.create_table("interna", c.schema, PrimarySpec{});
        struct S final : RecordStream {
            S(const Monitor& m, TableId t, const std::vector<std::vector<Value>>& r) : m_(m), t_(t), r_(r) {}
            std::optional<std::span<const std::byte>> next() override {
                if (i_ >= r_.size()) return std::nullopt;
                buf_ = m_.encode(t_, r_[i_++]);
                return std::span<const std::byte>(buf_);
            }
            const Monitor& m_; TableId t_; const std::vector<std::vector<Value>>& r_; std::size_t i_ = 0; std::vector<std::byte> buf_;
        };
        S a(mon, me, c.externa), b(mon, mi, c.interna);
        mon.bulk_load(me, a);
        mon.bulk_load(mi, b);
        Linhas x(sm, se, c.externa), y(sm, si, c.interna);
        sm.bulk_load(se, x);
        sm.bulk_load(si, y);
        mon.flush_all();
        sm.flush_all();
    }
    AccessPath scan(Monitor& m, TableId t) {
        for (const auto& p : m.access_paths(t)) if (p.kind == PathKind::Scan) return p;
        throw std::logic_error("sem varredura");
    }
    AccessPath scan(StorageManager& s, TableId t) {
        for (const auto& p : s.access_paths(t)) if (p.kind == PathKind::Scan) return p;
        throw std::logic_error("sem varredura");
    }
};

inline std::uint64_t pares_esperados(const std::vector<std::vector<Value>>& a, const std::vector<std::vector<Value>>& b) {
    std::map<std::string, std::uint64_t> ca, cb;
    auto chave = [](const Value& v) {
        if (auto s = std::get_if<std::string>(&v)) return "s" + *s;
        return "i" + std::to_string(std::get<std::int64_t>(v));
    };
    for (const auto& r : a) ++ca[chave(r[0])];
    for (const auto& r : b) ++cb[chave(r[0])];
    std::uint64_t n = 0;
    for (const auto& [k, x] : ca) n += cb.count(k) ? x * cb[k] : 0;
    return n;
}

}  // namespace memoria
