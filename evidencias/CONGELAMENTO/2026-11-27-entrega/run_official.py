import csv, hashlib, json, os, re, subprocess, sys, time
from pathlib import Path
root=Path(__file__).resolve().parent
checkout=root/'source'; source=checkout/'PrototipoIA/Implementacao'; build=root/'release'
commit='d95acc612b7885e6d35839f15c227c1cd527fca0'
prefix='/home/carlos/.local/cbd-deps/googletest-03597a01ee50ed33e9dfd640b249b4be3799d395'
phases=[]
def run(name,args):
    print('Iniciando '+name,flush=True); started=time.monotonic()
    with (root/(name+'.log')).open('w') as log:
        log.write(json.dumps(list(map(str,args)),ensure_ascii=False)+'\n'); log.flush()
        subprocess.run(list(map(str,args)),cwd=source,env=os.environ|{'CBD_COMMIT':commit},stdout=log,stderr=subprocess.STDOUT,check=True)
    phases.append({'phase':name,'seconds':round(time.monotonic()-started,3)})
    print(name+': aprovado',flush=True)
def rows(path):
    with path.open(newline='') as f: return list(csv.DictReader(f))
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=checkout,text=True).strip()==commit
assert not subprocess.check_output(['git','status','--porcelain'],cwd=checkout,text=True).strip()
run('configure',['cmake','-S',source,'-B',build,'-DCMAKE_BUILD_TYPE=Release','-DCBD_SANITIZE=OFF','-DCMAKE_REQUIRE_FIND_PACKAGE_GTest=TRUE','-DCMAKE_PREFIX_PATH='+prefix])
run('build',['cmake','--build',build,'--target','bancada','--parallel','2'])
full=root/'full'; assert not full.exists()
run('bancada',[build/'bancada','--escala','1','--politicas','lru,mru','--reps-min','3','--reps-max','10','--tolerancia','0.05','--m-juncao','32','--m-selecao','2','--lote','1000','--saida',full])
manifest=dict(line.split(': ',1) for line in (full/'manifesto.txt').read_text().splitlines())
expected={'commit':commit,'escala':'1','semente':'20261122','reps':'3..10','m_juncao':'32','m_selecao':'2','lote':'1000','bloco':'4096','extent':'8','molduras':'dados=32 indices=16 catalogo=4 trabalho=32','prefetch':'nao','toss_immediate':'nao'}
for k,v in expected.items(): assert manifest.get(k)==v,(k,manifest.get(k),v)
run('sqlite',[sys.executable,'-I',source/'tools/reference.py',full])
run('compare',[sys.executable,'-I',source/'tools/compare.py',full])
run('matrix',[sys.executable,'-I',source/'tools/verificar_matriz.py',full])
run('tables',[sys.executable,'-I',source/'tools/tabelas.py',full])
measured=[r for r in rows(full/'resultados.csv') if r['status']=='ok']
key=('politica','operacao','variante','algoritmo')
assert len({tuple(r[k] for k in key) for r in measured})==168
assert all(int(r['flush_writes'])==0 for r in measured)
all_convergence=rows(full/'convergencia.csv')
measured_keys={tuple(r[k] for k in key) for r in measured}
convergence=[r for r in all_convergence if tuple(r[k] for k in key) in measured_keys]
assert {tuple(r[k] for k in key) for r in convergence}==measured_keys
assert len(convergence)==168
assert all(r['parada']=='nao_executada' for r in all_convergence if tuple(r[k] for k in key) not in measured_keys)
assert all(3<=int(r['n_repeticoes'])<=10 for r in convergence)
assert all(r['convergiu_io']=='sim' for r in convergence)
text=(root/'matrix.log').read_text()
assert '108 executadas, 12 não aplicáveis, 0 inviáveis, 0 ausentes' in text
assert 'matriz: aprovada (0 falhas)' in text
assert 'ausentes: 0; divergências: 0' in (root/'compare.log').read_text()
assert 'verificação falhou' not in (root/'bancada.log').read_text()
assert not subprocess.check_output(['git','status','--porcelain'],cwd=checkout,text=True).strip()
result={'commit':commit,'clean_checkout':True,'full_rows':len(measured),'combinations':168,'required_executed':108,'required_not_applicable':12,'required_missing':0,'required_infeasible':0,'sqlite_missing':0,'sqlite_different':0,'io_not_converged':0,'time_not_converged':sum(r['convergiu_tempo']!='sim' for r in convergence),'final_flush_nonzero':0,'manifest_expected':expected,'phases':phases,'binary_sha256':hashlib.sha256((build/'bancada').read_bytes()).hexdigest(),'source_sha256':{str(p.relative_to(source)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted((source/'src').rglob('*')) if p.suffix in ('.cpp','.hpp')}}
(root/'aceitacao.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(result|{'source_sha256':'preservados em aceitacao.json'},ensure_ascii=False,indent=2),flush=True)
