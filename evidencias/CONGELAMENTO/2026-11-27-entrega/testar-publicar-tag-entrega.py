from pathlib import Path
import tempfile,subprocess,os,json,hashlib,shutil
r=Path(tempfile.mkdtemp(prefix='cbd-tag-local-validation-'))
realgit=shutil.which('git')
source='/home/carlos/Documentos/UFRJ/cbd/sistema-banco-dados/pr19-slot-prefetch'
sha='d95acc612b7885e6d35839f15c227c1cd527fca0'; tag='entrega-programa-2026-11-27'
def git(*args,cwd=None):
 return subprocess.run([realgit,*args],cwd=cwd,check=True,text=True,capture_output=True).stdout.strip()
git('clone','--bare','--shared',source,str(r/'remote.git'))
git('clone',str(r/'remote.git'),str(r/'repo'))
repo=r/'repo'; git('config','user.name','Fixture tag test',cwd=repo); git('config','user.email','fixture@example.invalid',cwd=repo)
evidence=repo/'evidencias/CONGELAMENTO/2026-11-27-entrega'; evidence.mkdir(parents=True)
(repo/'POST_MORTEM.md').write_text('Fixture only\n')
data={'commit':sha,'clean_checkout':True,'combinations':168,'required_executed':108,'required_not_applicable':12,'required_missing':0,'required_infeasible':0,'sqlite_missing':0,'sqlite_different':0,'io_not_converged':0,'final_flush_nonzero':0,'full_rows':504,'time_not_converged':0}
p=evidence/'aceitacao.json'; p.write_text(json.dumps(data))
(evidence/'ENTREGA.sha256').write_text(hashlib.sha256(p.read_bytes()).hexdigest()+'  aceitacao.json\n')
bin_dir=r/'bin'; bin_dir.mkdir()
(bin_dir/'git').write_text('''#!/usr/bin/env python3
import os,sys
args=sys.argv[1:]
if args==['remote','get-url','--push','origin']:
 print('git@github.com:Feharia/Grupo-2---Avaliacao-2---SBD---UFRJ.git');sys.exit(0)
os.execv(os.environ['REAL_GIT'],[os.environ['REAL_GIT'],*args])
''')
(bin_dir/'gh').write_text('''#!/usr/bin/env python3
import sys,json,os
args=sys.argv[1:]; sha='d95acc612b7885e6d35839f15c227c1cd527fca0'
if args[0]=='api': print(sha);sys.exit(0)
if args[:2]==['run','list']:
 print(json.dumps([{'headSha':sha,'status':'completed','conclusion':'failure' if os.environ['SCENARIO']=='failed-ci' else 'success','url':'fixture://main'}]));sys.exit(0)
raise SystemExit('Unexpected gh invocation')
''')
for p in bin_dir.iterdir(): p.chmod(0o755)
s=Path('/tmp/publicar-tag-entrega.sh').read_text().replace("worktree='/home/carlos/Documentos/UFRJ/cbd/sistema-banco-dados/pr19-slot-prefetch'",'worktree='+repr(str(repo))).replace("Path('/tmp/cbd-programa-congelado.json')",'Path('+repr(str(r/'checkpoint.json'))+')')
script=r/'script.sh'; script.write_text(s)
results=[]
for scenario in ['failed-ci','success','repeat-success']:
 env=os.environ|{'REAL_GIT':realgit,'PATH':str(bin_dir)+os.pathsep+os.environ['PATH'],'SCENARIO':scenario}
 p=subprocess.run(['bash',str(script)],env=env,text=True,capture_output=True)
 (r/(scenario+'.stdout')).write_text(p.stdout); (r/(scenario+'.stderr')).write_text(p.stderr)
 assert (p.returncode==0)==(scenario!='failed-ci'),(scenario,p.stderr)
 if scenario=='failed-ci':
  assert not git('tag','--list',tag,cwd=repo)
 else:
  assert git('rev-parse',tag+'^{commit}',cwd=repo)==sha
  assert git('cat-file','-t','refs/tags/'+tag,cwd=repo)=='tag'
  assert git('--git-dir='+str(r/'remote.git'),'rev-parse',tag+'^{commit}')==sha
 results.append({'scenario':scenario,'exit_code':p.returncode})
(r/'resultados.json').write_text(json.dumps({'simulated_ci':True,'git_remote':'local bare repository only','results':results},indent=2)+'\n')
Path('/tmp/cbd-tag-local-validation-current.txt').write_text(str(r)+'\n')
print(json.dumps({'validation_directory':str(r),'results':results},indent=2))
