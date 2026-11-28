#!/usr/bin/env bash
set -euo pipefail

repo='Feharia/Grupo-2---Avaliacao-2---SBD---UFRJ'
worktree='/home/carlos/Documentos/UFRJ/cbd/sistema-banco-dados/pr19-slot-prefetch'
expected='a502a5f573626623b6fd2b71195902aeceeadb24'
base='f3d12311bfece737fa2f2c3b6decedcccfa5696f'
fail() { printf 'Interrompido: %s\n' "$*" >&2; exit 1; }
for command in gh git python3; do
  command -v "$command" >/dev/null || fail "Comando ausente: $command"
done
cd "$worktree"
files=$(mktemp -d /tmp/pr21-ci-XXXXXX)
printf 'Evidências: %s\n' "$files"
validate_pr() {
  gh api "repos/$repo/pulls/21" > "$1"
  python3 - "$1" "$expected" "$base" "$repo" <<'PY'
import json, sys
from pathlib import Path
p=json.loads(Path(sys.argv[1]).read_text())
if p['state']!='open' or p.get('merged'):
    raise SystemExit('O PR não está aberto; confira seu estado. Nenhuma integração foi feita pelo script.')
if p['head']['sha']!=sys.argv[2] or p['base']['sha']!=sys.argv[3]:
    raise SystemExit('O HEAD ou a base do PR mudou desde a revisão; confira os novos commits antes de integrar.')
if p['base']['ref']!='main' or p['head']['repo']['full_name']!=sys.argv[4]:
    raise SystemExit('Origem ou destino inesperado do PR.')
PY
}
validate_pr "$files/pr-before.json"
gh pr checks 21 --repo "$repo" --watch --interval 10
gh pr checks 21 --repo "$repo" --json name,bucket,state,link > "$files/checks.json"
gh run list --repo "$repo" --commit "$expected" --event pull_request \
  --workflow correcao.yml --limit 20 \
  --json databaseId,headSha,status,conclusion,url > "$files/runs.json"
validate_pr "$files/pr-after.json"
python3 - "$files" "$worktree" "$expected" "$base" <<'PY'
import json, sys, shutil
from datetime import datetime, timezone
from pathlib import Path
root, worktree = map(Path, sys.argv[1:3])
checks=json.loads((root/'checks.json').read_text())
runs=json.loads((root/'runs.json').read_text())
if not checks or any(c['bucket']!='pass' for c in checks):
    raise SystemExit('Há checks ausentes, pendentes, ignorados ou não aprovados; consulte checks.json.')
if not runs or runs[0]['headSha']!=sys.argv[3] or runs[0]['status']!='completed' or runs[0]['conclusion']!='success':
    raise SystemExit('O workflow Correção de pull_request não confirmou sucesso no SHA revisado; consulte runs.json.')
ci=runs[0]
result={'pr':21,'headSha':sys.argv[3],'baseSha':sys.argv[4], 'ci':ci,
        'checks':checks,'checkedAtUtc':datetime.now(timezone.utc).isoformat(),
        'evidenceDir':str(root),'mergedByThisScript':False}
(root/'confirmacao.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
destination=worktree/'evidencias/PR-21'/root.name
shutil.copytree(root,destination)
marker='### CI remoto confirmado para o PR #21 — '+sys.argv[3][:7]
post=worktree/'POST_MORTEM.md'
if marker not in post.read_text():
    with post.open('a') as f:
        f.write('\n'+marker+'\n\n- O usuário executou `/tmp/verificar-ci-pr21.sh`. PR aberto, destino main,\n'
                '  HEAD e base iguais aos revisados; todos os checks aprovados.\n'
                '- Workflow Correção (pull_request): '+ci['url']+'\n'
                '- Evidência: `'+str(destination.relative_to(worktree))+'/`.\n'
                '- Esta etapa não faz merge, push, commit ou tag; código e alterações locais preservados.\n')
print(json.dumps({'pr':21,'headSha':sys.argv[3],'ci':ci,'evidenceDir':str(destination)},ensure_ascii=False,indent=2))
PY
printf '\nCI do PR #21 confirmado para o SHA revisado. Merge e tag não realizados.\n'
