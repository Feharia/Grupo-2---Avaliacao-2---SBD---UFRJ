from pathlib import Path
import json,os,subprocess,tempfile
root=Path(tempfile.mkdtemp(prefix='pr21-integracao-validacao-'))
script=Path('/tmp/integrar-pr21.sh').read_text()
pr=json.loads(Path('/tmp/pr21-ci-6eNkkE/pr-after.json').read_text())
head=pr['head']['sha']; merged='1'*40
bin_dir=root/'bin'; bin_dir.mkdir()
(bin_dir/'gh').write_text('''#!/usr/bin/env python3
import os,json,sys
from pathlib import Path
r=Path(os.environ['FIXTURE_DIR']); a=sys.argv[1:]
head='a502a5f573626623b6fd2b71195902aeceeadb24'; merged='1'*40
with (r/'gh-calls.jsonl').open('a') as f: f.write(json.dumps(a)+'\\n')
if a[:2]==['pr','checks']: sys.exit(0)
if a[0]=='api':
 if 'graphql' in a:
  print(json.dumps({'data':{'repository':{'pullRequest':{'closingIssuesReferences':{'nodes':[],'pageInfo':{'hasNextPage':False}}}}}})); sys.exit(0)
 if any(s.endswith('/issues/18') for s in a): print('{"state":"open"}'); sys.exit(0)
 if any(s.endswith('/pulls/21/merge') for s in a):
  payload=json.loads(Path(a[a.index('--input')+1]).read_text())
  assert payload['sha']==head and payload['merge_method']=='squash'
  (r/'merge-called').touch()
  p=json.loads((r/'pr.json').read_text()); p.update(merged=True,state='closed',merge_commit_sha=merged)
  (r/'pr.json').write_text(json.dumps(p)); print(json.dumps({'merged':True,'sha':merged})); sys.exit(0)
 if any(s.endswith('/pulls/21') for s in a): print((r/'pr.json').read_text()); sys.exit(0)
if a[:2]==['run','list']:
 if '--branch' in a: print('123'); sys.exit(0)
 print(json.dumps([{'headSha':head,'status':'completed','conclusion':'failure' if os.environ['SCENARIO']=='ci-failure' else 'success','url':'fixture://pr'}])); sys.exit(0)
if a[:2]==['run','watch']: sys.exit(0)
if a[:2]==['run','view']: print(json.dumps({'status':'completed','conclusion':'success','headSha':merged,'url':'fixture://main'})); sys.exit(0)
raise SystemExit('Unexpected gh invocation: '+repr(a))
''')
(bin_dir/'git').write_text('''#!/usr/bin/env python3
import json,os,sys
from pathlib import Path
a=sys.argv[1:]; r=Path(os.environ['FIXTURE_DIR'])
with (r/'git-calls.jsonl').open('a') as f: f.write(json.dumps(a)+'\\n')
if a[:3]==['remote','get-url','origin']: print('git@github.com:Feharia/Grupo-2---Avaliacao-2---SBD---UFRJ.git')
elif a[:1]==['cat-file'] or a[:1]==['fetch'] or a[:1]==['merge-base']: pass
elif a[:1]==['status']: print(' M POST_MORTEM.md')
elif a[:1]==['rev-parse']: print('2cd1696f1bb42d5f3fbed453ef7827d125140266' if a[-1]=='HEAD' else 'a'*40)
else: raise SystemExit('Unexpected git mutation or command: '+repr(a))
''')
for p in bin_dir.iterdir(): p.chmod(0o755)
results=[]
for scenario in ['open-success','resume-success','wrong-sha','ci-failure']:
 d=root/scenario; d.mkdir(); work=d/'repo'; work.mkdir(); (work/'POST_MORTEM.md').write_text('fixture only\n')
 p=json.loads(json.dumps(pr))
 if scenario=='resume-success': p.update(merged=True,state='closed',merge_commit_sha=merged)
 if scenario=='wrong-sha': p['head']['sha']='0'*40
 (d/'pr.json').write_text(json.dumps(p))
 s=script.replace("worktree='/home/carlos/Documentos/UFRJ/cbd/sistema-banco-dados/pr19-slot-prefetch'",'worktree='+repr(str(work))).replace("checkpoint='/tmp/pr21-integrado.json'",'checkpoint='+repr(str(d/'checkpoint.json')))
 path=d/'run.sh'; path.write_text(s)
 env=os.environ|{'FIXTURE_DIR':str(d),'SCENARIO':scenario,'PATH':str(bin_dir)+os.pathsep+os.environ['PATH']}
 result=subprocess.run(['bash',str(path)],text=True,capture_output=True,env=env)
 (d/'stdout.log').write_text(result.stdout); (d/'stderr.log').write_text(result.stderr)
 success=scenario in ['open-success','resume-success']
 assert (result.returncode==0)==success,(scenario,result.returncode,result.stderr)
 assert (d/'merge-called').exists()==(scenario=='open-success')
 if success: assert json.loads((d/'checkpoint.json').read_text())['ciMain']['conclusion']=='success'
 else: assert (work/'POST_MORTEM.md').read_text()=='fixture only\n'
 results.append({'scenario':scenario,'exit_code':result.returncode,'merge_called':(d/'merge-called').exists()})
(root/'resultados.json').write_text(json.dumps({'simulated_only':True,'results':results},indent=2)+'\n')
Path('/tmp/pr21-integracao-validacao-current.txt').write_text(str(root)+'\n')
print(json.dumps({'simulation_directory':str(root),'results':results},indent=2))
