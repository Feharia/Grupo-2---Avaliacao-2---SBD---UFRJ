#!/usr/bin/env bash
set -euo pipefail
repo='Feharia/Grupo-2---Avaliacao-2---SBD---UFRJ'
worktree='/home/carlos/Documentos/UFRJ/cbd/sistema-banco-dados/pr19-slot-prefetch'
candidate='a502a5f573626623b6fd2b71195902aeceeadb24'
base='f3d12311bfece737fa2f2c3b6decedcccfa5696f'
checkpoint='/tmp/pr21-integrado.json'
fail() { printf 'Interrompido: %s\n' "$*" >&2; exit 1; }
for command in git gh python3; do command -v "$command" >/dev/null || fail "Comando ausente: $command"; done
cd "$worktree"
case "$(git remote get-url origin)" in
  "git@github.com:$repo.git"|"https://github.com/$repo.git"|"https://github.com/$repo") ;;
  *) fail 'O origin não corresponde ao repositório do projeto.' ;;
esac
git cat-file -e "$candidate^{commit}"
files=$(mktemp -d /tmp/pr21-integracao-XXXXXX)
printf 'Evidências: %s\n' "$files"
git status --porcelain=v1 > "$files/status-before.txt"
git rev-parse HEAD > "$files/head-before.txt"
read_pr() {
  gh api "repos/$repo/pulls/21" > "$files/pr-current.json"
  python3 - "$files/pr-current.json" "$candidate" "$base" "$repo" <<'PY'
import json,sys
from pathlib import Path
p=json.loads(Path(sys.argv[1]).read_text())
if (p['head']['sha']!=sys.argv[2] or p['head']['repo']['full_name']!=sys.argv[4]
    or p['base']['ref']!='main' or p['base']['repo']['full_name']!=sys.argv[4]):
    raise SystemExit('HEAD, origem ou destino do PR mudou desde a revisão.')
if p.get('merged'):
    print(p['merge_commit_sha'])
elif p['state']=='open' and p['base']['sha']==sys.argv[3]:
    print('open')
else:
    raise SystemExit('PR fechado sem merge ou base diferente da revisada; conferir antes de integrar.')
PY
}
state=$(read_pr)
cp "$files/pr-current.json" "$files/pr-before.json"
if [[ "$state" == open ]]; then
  gh pr checks 21 --repo "$repo" --watch --interval 10
  gh run list --repo "$repo" --commit "$candidate" --event pull_request \
    --workflow correcao.yml --limit 20 --json headSha,status,conclusion,url > "$files/ci-pr.json"
  python3 - "$files/ci-pr.json" "$candidate" <<'PY'
import json,sys
from pathlib import Path
runs=json.loads(Path(sys.argv[1]).read_text())
if not runs or runs[0]['headSha']!=sys.argv[2] or runs[0]['status']!='completed' or runs[0]['conclusion']!='success':
    raise SystemExit('O workflow do SHA revisado não está aprovado; merge não realizado.')
PY
  gh api "repos/$repo/issues/18" > "$files/issue18-before.json"
  gh api graphql -f owner=Feharia -f name=Grupo-2---Avaliacao-2---SBD---UFRJ \
    -f query='query($owner: String!, $name: String!) { repository(owner: $owner, name: $name) { pullRequest(number: 21) { closingIssuesReferences(first: 100) { nodes { number } pageInfo { hasNextPage } } } } }' > "$files/linked-issues.json"
  python3 - "$files" "$candidate" <<'PY'
import json,sys
from pathlib import Path
p=Path(sys.argv[1]); j=json.loads((p/'linked-issues.json').read_text())
if j.get('errors'): raise SystemExit('Falha ao consultar vínculos de fechamento do PR.')
c=j['data']['repository']['pullRequest']['closingIssuesReferences']
if c['pageInfo']['hasNextPage'] or any(n['number']==18 for n in c['nodes']):
    raise SystemExit('Há vínculo de fechamento da #18 ou consulta incompleta. Conferir antes de integrar: os gráficos não foram resolvidos por este PR.')
body=('Registra bloco, extent, molduras por área, prefetch e toss-immediate no manifesto; '
      'exibe bloco/extent nas tabelas e atualiza o diagrama para as interfaces reais e o callback on_moved.\n\n'
      'Revisão local: 114 testes C++ e 69 Python aprovados; 168 cenários de E/S sem deltas, '
      'com tuplas conferidas contra SQLite.\n\n'
      'Refs #18: configuração física registrada; gráficos permanecem fora desta alteração.')
(p/'merge-payload.json').write_text(json.dumps({'sha':sys.argv[2],'merge_method':'squash',
    'commit_title':'chore: registra configuração física e atualiza diagrama (PR #21)',
    'commit_message':body},ensure_ascii=False)+'\n')
PY
  state=$(read_pr)
  if [[ "$state" == open ]]; then
    printf '\nIntegrando PR #21 por squash, preservando as branches.\n'
    gh api --method PUT "repos/$repo/pulls/21/merge" --input "$files/merge-payload.json" > "$files/merge-result.json"
    merge_sha=$(python3 - "$files/merge-result.json" <<'PY'
import json,sys
from pathlib import Path
r=json.loads(Path(sys.argv[1]).read_text())
if not r.get('merged'): raise SystemExit('Merge não confirmado: '+r.get('message','sem mensagem'))
print(r['sha'])
PY
    )
  else
    merge_sha=$state
  fi
else
  merge_sha=$state
  printf '\nPR já integrado; retomando a verificação do commit %s.\n' "$merge_sha"
fi
printf '\nMerge confirmado: %s. Se uma etapa posterior falhar, pode repetir este script para retomar as verificações.\n' "$merge_sha"
python3 - "$checkpoint" "$files" "$candidate" "$merge_sha" <<'PY'
import json,sys
from pathlib import Path
r={'pr':21,'sourceHead':sys.argv[3],'mergeSha':sys.argv[4],'ciMain':'pendente','evidenceDir':sys.argv[2]}
Path(sys.argv[1]).write_text(json.dumps(r,ensure_ascii=False,indent=2)+'\n')
PY
# Fetch atualiza a referência remota, sem checkout/merge na main local ou no worktree sujo.
git fetch origin refs/heads/main:refs/remotes/origin/main
git merge-base --is-ancestor "$merge_sha" refs/remotes/origin/main || fail 'Commit integrado não encontrado na main buscada.'
[[ "$(git rev-parse "$merge_sha^{tree}")" == "$(git rev-parse "$candidate^{tree}")" ]] || fail 'A árvore integrada difere do candidato revisado; conferir antes do congelamento.'
gh api "repos/$repo/issues/18" > "$files/issue18-after.json"
python3 - "$files" <<'PY'
import json,sys
from pathlib import Path
p=Path(sys.argv[1]); before=p/'issue18-before.json'
if before.exists() and json.loads(before.read_text())['state']=='open' and json.loads((p/'issue18-after.json').read_text())['state']!='open':
    raise SystemExit('O merge ocorreu, mas a #18 foi fechada; conferir antes de continuar.')
PY
run_id=''
for ((attempt=1; attempt<=36; attempt++)); do
  run_id=$(gh run list --repo "$repo" --branch main --commit "$merge_sha" --event push \
    --workflow correcao.yml --limit 20 --json databaseId --jq '.[0].databaseId // empty')
  [[ -n "$run_id" ]] && break
  printf 'Aguardando o workflow da main aparecer (%s/36)...\n' "$attempt"
  sleep 5
done
[[ -n "$run_id" ]] || fail "Merge já realizado em $merge_sha; CI ainda não encontrado. Repita o script para retomar."
gh run watch "$run_id" --repo "$repo" --interval 10 --exit-status
gh run view "$run_id" --repo "$repo" --json status,conclusion,headSha,url > "$files/ci-main.json"
git status --porcelain=v1 > "$files/status-after.txt"
git rev-parse HEAD > "$files/head-after.txt"
python3 - "$files" "$candidate" "$merge_sha" "$worktree" "$checkpoint" <<'PY'
import json,sys,shutil
from pathlib import Path
p=Path(sys.argv[1]); repo=Path(sys.argv[4]); ci=json.loads((p/'ci-main.json').read_text())
if ci['status']!='completed' or ci['conclusion']!='success' or ci['headSha']!=sys.argv[3]:
    raise SystemExit('Merge realizado, mas CI da main não confirmou sucesso nesse SHA.')
if (p/'status-before.txt').read_bytes()!=(p/'status-after.txt').read_bytes() or (p/'head-before.txt').read_bytes()!=(p/'head-after.txt').read_bytes():
    raise SystemExit('O estado do worktree mudou durante a operação; conferir antes de registrar a conclusão.')
result={'pr':21,'sourceHead':sys.argv[2],'mergeSha':sys.argv[3],'ciMain':ci,
        'issue18':json.loads((p/'issue18-after.json').read_text())['state'],'evidenceDir':str(p)}
(p/'confirmacao.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
Path(sys.argv[5]).write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
dest=repo/'evidencias/PR-21'/p.name
shutil.copytree(p,dest)
post=repo/'POST_MORTEM.md'; marker='### PR #21 integrado — '+sys.argv[3]
if marker not in post.read_text():
    with post.open('a') as f:
        f.write('\n'+marker+'\n\n- Integração por squash do head revisado `'+sys.argv[2]+'`.\n'
                '- Árvore integrada idêntica à do candidato. CI da main aprovado: '+ci['url']+'\n'
                '- Evidências: `'+str(dest.relative_to(repo))+'/`.\n'
                '- Branches, HEAD e alterações locais preservados. Rodada oficial no SHA integrado\n'
                '  e tag de entrega ainda pendentes; este script não cria tag.\n')
print(json.dumps(result,ensure_ascii=False,indent=2))
PY
printf '\nPR #21 integrado e CI da main aprovado. Rodada oficial e tag serão a próxima etapa.\n'
