import csv
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time

root = Path('/tmp/cbd-freeze-validacao-10reharx')
checkout = root / 'candidate'
source = checkout / 'PrototipoIA/Implementacao'
tools = source / 'tools'
release = root / 'release'
commit = '8569e0e9f1894b6c647bf72888b4e5c0e9c1de9c'
phases = []


def run(name, args, extra=None):
    print('Iniciando ' + name, flush=True)
    started = time.monotonic()
    env = os.environ | {'CBD_COMMIT': commit, 'BUILD_DIR': str(release)} | (extra or {})
    with (root / (name + '.log')).open('w') as log:
        log.write(json.dumps(list(map(str, args)), ensure_ascii=False) + '\n')
        log.flush()
        subprocess.run(list(map(str, args)), cwd=source, env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
    phases.append({'name': name, 'seconds': round(time.monotonic() - started, 2)})
    print(name + ': aprovado', flush=True)


assert subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=checkout, text=True).strip() == commit
assert not subprocess.check_output(['git', 'status', '--porcelain'], cwd=checkout, text=True).strip()
run('release-tests', ['ctest', '--test-dir', release, '--output-on-failure'],
    {'CBD_TEST_DIR': str(root / 'release-test-data')})
run('python-tests', [sys.executable, '-I', '-m', 'unittest', 'discover', '-s', source / 'tests/python', '-v'])
run('asan-tests', ['ctest', '--test-dir', root / 'asan', '--output-on-failure'],
    {'CBD_TEST_DIR': str(root / 'asan-test-data'), 'ASAN_OPTIONS': 'detect_leaks=0:halt_on_error=1',
     'UBSAN_OPTIONS': 'halt_on_error=1:print_stacktrace=1'})

# O perfil de I/O já inclui smoke com tuplas completas, SQLite e cobertura.
run('io-base', [sys.executable, '-I', tools / 'medir_io.py', '--source', root / 'base/PrototipoIA/Implementacao',
               '--build-dir', root / 'base-build', '--commit', 'a59f3336f23f4899667108447c57a8a8e9808544',
               '--saida', root / 'io-base'])
run('io-candidate', [sys.executable, '-I', tools / 'medir_io.py', '--source', source, '--build-dir', release,
                    '--commit', commit, '--saida', root / 'io-candidate'])
run('io-comparison', [sys.executable, '-I', tools / 'comparar_io.py', root / 'io-base', root / 'io-candidate',
                     root / 'io-comparison'])
io = json.loads((root / 'io-comparison/comparacao.json').read_text())
assert io['cenarios'] == 168
assert all(r[k]['delta'] == 0 for r in io['linhas'] for k in ('leituras', 'gravacoes', 'flush_writes'))

# Esta rodada começa após o término de builds, sanitizers e medições reduzidas.
full = root / 'full'
run('bench-full', [release / 'bancada', '--escala', '1', '--politicas', 'lru,mru', '--reps-min', '3',
                   '--reps-max', '10', '--m-juncao', '32', '--m-selecao', '2', '--lote', '1000', '--saida', full])
run('sqlite-full', [sys.executable, '-I', tools / 'reference.py', full])
run('compare-full', [sys.executable, '-I', tools / 'compare.py', full])
run('matrix-full', [sys.executable, '-I', tools / 'verificar_matriz.py', full])
matrix_text = (root / 'matrix-full.log').read_text()
assert re.search(r'\b0 inviáveis, 0 ausentes', matrix_text)
run('tables-full', [sys.executable, '-I', tools / 'tabelas.py', full])

with (full / 'resultados.csv').open(newline='') as file:
    measured = [r for r in csv.DictReader(file) if r['status'] == 'ok']
with (full / 'convergencia.csv').open(newline='') as file:
    convergence = list(csv.DictReader(file))
assert len({tuple(r[k] for k in ('politica','operacao','variante','algoritmo')) for r in measured}) == 168
assert all(int(r['flush_writes']) == 0 for r in measured)
assert 'verificação falhou' not in (root / 'bench-full.log').read_text()
assert not subprocess.check_output(['git', 'status', '--porcelain'], cwd=checkout, text=True).strip()
result = {'commit': commit, 'base': 'a59f3336f23f4899667108447c57a8a8e9808544',
          'full_rows': len(measured), 'full_combinations': 168, 'io_scenarios': io['cenarios'],
          'io_changed_counters': False, 'full_final_flush_nonzero': 0,
          'convergence_time_ceiling': sum(r['parada'] == 'teto' and r['convergiu_io'] == 'sim' for r in convergence),
          'convergence_io_ceiling': [r for r in convergence if r['parada'] == 'teto' and r['convergiu_io'] != 'sim'],
          'lsan_enabled': False, 'clean_checkout': True, 'phases': phases,
          'source_sha256': {str(p.relative_to(source)): hashlib.sha256(p.read_bytes()).hexdigest()
                            for p in sorted((source / 'src').rglob('*')) if p.suffix in ('.cpp', '.hpp')}}
(root / 'acceptance.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
print(json.dumps(result | {'source_sha256': 'preservado em acceptance.json'}, ensure_ascii=False, indent=2), flush=True)
