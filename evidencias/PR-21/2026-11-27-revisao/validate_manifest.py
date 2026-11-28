import csv
import importlib.util
import json
import os
from pathlib import Path
import struct
import subprocess
from types import SimpleNamespace

root = Path(__file__).resolve().parent
source = root / "source/PrototipoIA/Implementacao"
out = root / "native-manifest"
sha = "a502a5f573626623b6fd2b71195902aeceeadb24"
with (root / "native-manifest.log").open("w") as log:
    subprocess.run([str(root / "release/bancada"), "--rapido", "--so", "j_pequenas",
                    "--politicas", "lru,mru", "--saida", str(out)],
                   env=os.environ | {"CBD_COMMIT": sha}, stdout=log, stderr=subprocess.STDOUT, check=True)
manifest = dict(line.split(": ", 1) for line in (out / "manifesto.txt").read_text().splitlines())
expected = {"bloco": "4096", "extent": "8", "molduras": "dados=32 indices=16 catalogo=4 trabalho=32",
            "prefetch": "nao", "toss_immediate": "nao", "commit": sha}
for key, value in expected.items():
    assert manifest[key] == value, (key, manifest.get(key), value)
files = {}
for policy in ("LRU", "MRU"):
    headers = list((out / ("motor_" + policy)).iterdir())
    assert headers, policy
    for path in headers:
        with path.open("rb") as file:
            data = file.read(16)
        assert struct.unpack_from("<II", data, 8) == (4096, 8), str(path)
    files[policy] = len(headers)
with (out / "resultados.csv").open() as file:
    rows = list(csv.DictReader(file))
assert {row["politica"] for row in rows} == {"LRU", "MRU"}
assert any(row["status"] == "ok" for row in rows)
spec = importlib.util.spec_from_file_location("tabelas", source / "tools/tabelas.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
def summary(manifest):
    return module.t_resumo(SimpleNamespace(manifesto=manifest, nao_exec=[], politica="LRU",
                       politicas=["LRU", "MRU"], brutos=[], conferencia="", da_politica=lambda: [])).markdown()
assert "4096 B (extent de 8 blocos)" in summary(manifest)
legacy = dict(manifest)
for key in ("bloco", "extent", "molduras", "prefetch", "toss_immediate"):
    legacy.pop(key)
assert "não registrado" in summary(legacy)
(root / "manifest-validation.json").write_text(json.dumps({"commit": sha, "expected": expected,
    "file_headers_verified": files, "new_manifest_table": "passed", "legacy_manifest_table": "passed"}, indent=2) + "\n")
print("Manifesto nativo: campos corretos, cabeçalhos LRU/MRU consistentes; tabelas novas e legadas aprovadas.")
