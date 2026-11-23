#!/usr/bin/env bash
set -euo pipefail

repo='Feharia/Grupo-2---Avaliacao-2---SBD---UFRJ'
worktree='/home/carlos/Documentos/UFRJ/cbd/sistema-banco-dados/pr19-slot-prefetch'
candidate='4339845c74815bf51a29fe85425d68af30095c42'
base='4091762d65a52495c02c9cf63da5bc630998ff4d'
body_file='/tmp/pr19-merge-body.txt'
fail() { printf 'Interrompido: %s\n' "$*" >&2; exit 1; }
for command in git gh python3; do command -v "$command" >/dev/null || fail "comando ausente: $command"; done
cd "$worktree"
[[ "$(git rev-parse HEAD)" == "$candidate" ]] || fail 'O HEAD local mudou; conferir a versão antes de integrar.'
[[ -f "$body_file" ]] || fail 'A mensagem explícita do squash está ausente.'
files=$(mktemp -d /tmp/pr19-integracao-XXXXXX)
printf 'Evidências: %s\n' "$files"
prepare_merge() {
gh api "repos/$repo/pulls/19" > "$files/pr-before.json"
gh api "repos/$repo/issues/18" > "$files/issue18-before.json"
gh api graphql -f owner=Feharia -f name=Grupo-2---Avaliacao-2---SBD---UFRJ \
  -f query='query($owner: String!, $name: String!) { repository(owner: $owner, name: $name) { pullRequest(number: 19) { closingIssuesReferences(first: 100) { nodes { number } pageInfo { hasNextPage } } } } }' \
  > "$files/linked-issues.json"
python3 - "$files" "$candidate" "$base" "$body_file" <<'PY'
import json,re,sys
from pathlib import Path
p=Path(sys.argv[1]); head,base=sys.argv[2:4]
pr=json.loads((p/'pr-before.json').read_text()); issue=json.loads((p/'issue18-before.json').read_text())
if pr['state']!='open' or pr.get('merged') or pr['head']['sha']!=head or pr['base']['sha']!=base:
    raise SystemExit('O estado, HEAD ou base do PR mudou. Conferir antes do merge; nada foi integrado.')
if pr['base']['ref']!='main' or pr['head']['ref']!='fix/auditoria-p01-p15' or pr['head']['repo']['full_name']!='Feharia/Grupo-2---Avaliacao-2---SBD---UFRJ':
    raise SystemExit('Origem/destino inesperado do PR.')
if issue['state']!='open': raise SystemExit('A #18 não está aberta; conferir seu estado antes do merge.')
linked=json.loads((p/'linked-issues.json').read_text())
if linked.get('errors'): raise SystemExit('Falha ao consultar os vínculos de fechamento do PR.')
connection=linked['data']['repository']['pullRequest']['closingIssuesReferences']
if connection['pageInfo']['hasNextPage']: raise SystemExit('Consulta de vínculos incompleta; conferir antes do merge.')
if any(n['number']==18 for n in connection['nodes']):
    raise SystemExit('A #18 ainda está vinculada ao fechamento do PR. Remova esse vínculo antes de integrar.')
body=Path(sys.argv[4]).read_text()
closing=r'\b(?:close[sd]?|fix(?:es|ed)?|resolve[sd]?)\s*:?\s+(?:Feharia/Grupo-2---Avaliacao-2---SBD---UFRJ)?#18\b'
if re.search(closing,body,re.I) or re.search(closing,pr.get('body') or '',re.I):
    raise SystemExit('Há uma instrução de fechamento da #18 na mensagem/descrição; conferir antes do merge.')
(p/'merge-payload.json').write_text(json.dumps({'sha':head,'merge_method':'squash',
 'commit_title':'fix: integra revisão funcional da bancada (PR #19)','commit_message':body},ensure_ascii=False)+'\n')
PY
}
prepare_merge

# Os checks são conferidos no PR atual; a API de merge também exige o SHA exato.
gh pr checks 19 --repo "$repo" --watch --interval 10
gh run list --repo "$repo" --branch fix/auditoria-p01-p15 --commit "$candidate" \
  --event pull_request --workflow correcao.yml --limit 10 --json headSha,status,conclusion,url > "$files/ci-pr.json"
python3 - "$files/ci-pr.json" "$candidate" <<'PY'
import json,sys
from pathlib import Path
runs=json.loads(Path(sys.argv[1]).read_text())
if not runs or runs[0]['headSha']!=sys.argv[2] or runs[0]['status']!='completed' or runs[0]['conclusion']!='success':
    raise SystemExit('O workflow do HEAD exato não está aprovado; merge não executado.')
PY
prepare_merge
printf '\nIntegrando o PR #19 por squash, sem excluir suas branches.\n'
gh api --method PUT "repos/$repo/pulls/19/merge" --input "$files/merge-payload.json" > "$files/merge-result.json"
merge_sha=$(python3 - "$files/merge-result.json" <<'PY'
import json,sys
from pathlib import Path
r=json.loads(Path(sys.argv[1]).read_text())
if not r.get('merged'): raise SystemExit('GitHub não confirmou o merge: '+r.get('message','sem mensagem'))
print(r['sha'])
PY
)

printf '\nMerge realizado: %s\n' "$merge_sha"
python3 - "$files" "$candidate" "$merge_sha" <<'PYMERGED'
import json,sys
from pathlib import Path
Path('/tmp/pr19-integrado.json').write_text(json.dumps({'sourceHead':sys.argv[2],'mergeSha':sys.argv[3],
 'ciMain':'pendente','evidenceDir':sys.argv[1]},ensure_ascii=False,indent=2)+'\n')
PYMERGED

# Fetch não move a main local nem toca no POST_MORTEM já modificado em outro worktree.
git fetch origin refs/heads/main
git merge-base --is-ancestor "$merge_sha" FETCH_HEAD || fail 'O merge retornado não está na main buscada.'
[[ "$(git rev-parse "$merge_sha^{tree}")" == "$(git rev-parse "$candidate^{tree}")" ]] || fail 'O conteúdo do squash difere do candidato aprovado; conferir antes de continuar.'
gh api "repos/$repo/issues/18" > "$files/issue18-after.json"
python3 - "$files/issue18-after.json" <<'PY'
import json,sys
from pathlib import Path
if json.loads(Path(sys.argv[1]).read_text())['state']!='open':
    raise SystemExit('O merge ocorreu, mas a #18 não permanece aberta; conferir antes de continuar.')
PY

run_id=''
for ((attempt=1; attempt<=36; attempt++)); do
  run_id=$(gh run list --repo "$repo" --branch main --commit "$merge_sha" --event push \
    --workflow correcao.yml --limit 10 --json databaseId --jq '.[0].databaseId // empty')
  [[ -n "$run_id" ]] && break
  sleep 5
done
[[ -n "$run_id" ]] || fail "Merge realizado em $merge_sha, mas o Actions da main ainda não apareceu. Não repita o merge."
gh run watch "$run_id" --repo "$repo" --interval 10 --exit-status
gh run view "$run_id" --repo "$repo" --json conclusion,headSha,url > "$files/ci-main.json"
python3 - "$files" "$candidate" "$merge_sha" <<'PY'
import json,sys
from pathlib import Path
p=Path(sys.argv[1]); ci=json.loads((p/'ci-main.json').read_text())
if ci['conclusion']!='success' or ci['headSha']!=sys.argv[3]:
    raise SystemExit('Merge realizado, mas o CI da main não aprovou esse SHA; conferir antes das próximas correções.')
result={'sourceHead':sys.argv[2],'mergeSha':sys.argv[3],'issue18':'open','ciMain':ci,'evidenceDir':str(p)}
Path('/tmp/pr19-integrado.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(result,ensure_ascii=False,indent=2))
PY
printf '\nPR #19 integrado; CI da main aprovado. Alterações locais e branches preservadas.\n'
