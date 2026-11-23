#!/usr/bin/env bash
set -euo pipefail

repo='Feharia/Grupo-2---Avaliacao-2---SBD---UFRJ'
worktree='/home/carlos/Documentos/UFRJ/cbd/sistema-banco-dados/pr19-slot-prefetch'
branch='fix/pr19-slot-prefetch'
candidate='4339845c74815bf51a29fe85425d68af30095c42'
workflow='correcao.yml'

fail() { printf 'Interrompido: %s\n' "$*" >&2; exit 1; }
for command in git gh python3; do
  command -v "$command" >/dev/null || fail "instale $command antes de continuar."
done
cd "$worktree"
[[ "$(git branch --show-current)" == "$branch" ]] || fail 'a branch local mudou.'
[[ "$(git rev-parse HEAD)" == "$candidate" ]] || fail 'o HEAD local mudou; confira os novos commits.'
origin_url=$(git remote get-url --push origin)
case "$origin_url" in
  "git@github.com:$repo.git"|"https://github.com/$repo.git"|"https://github.com/$repo") ;;
  *) fail "origin inesperado: $origin_url" ;;
esac

previous='a6b3d48dd913e12f6aa1623954edd34ed13031ae'
pr_branch='fix/auditoria-p01-p15'
pr_files=$(mktemp -d /tmp/pr19-continuacao-XXXXXX)
printf '\nEvidências da continuação: %s\n' "$pr_files"

wait_pr_head() {
  local destination=$1 attempt status=''
  for ((attempt=1; attempt<=30; attempt++)); do
    gh api "repos/$repo/pulls/19" > "$destination"
    status=$(python3 - "$destination" "$candidate" "$previous" <<'PYHEAD'
import json
from pathlib import Path
import sys

pr = json.loads(Path(sys.argv[1]).read_text())
expected, previous = sys.argv[2:4]
if (pr['state'] != 'open' or pr['base']['ref'] != 'main'
        or pr['head']['ref'] != 'fix/auditoria-p01-p15'
        or pr['head']['repo']['full_name'] != 'Feharia/Grupo-2---Avaliacao-2---SBD---UFRJ'):
    raise SystemExit('O estado ou destino do PR mudou; conferir antes de editar.')
head = pr['head']['sha']
if head == expected:
    print('pronto')
elif head == previous:
    print('aguardando')
else:
    raise SystemExit(f'O PR apresenta outro SHA ({head}); continuidade interrompida.')
PYHEAD
    )
    [[ "$status" == pronto ]] && return 0
    printf 'API ainda informa o SHA anterior; aguardando atualização (%s/30).\n' "$attempt"
    if ((attempt<30)); then sleep 2; fi
  done
  fail 'A API não atualizou o HEAD no prazo de 58 segundos. Operação interrompida.'
}

wait_pr_head "$pr_files/pull-before.json"
gh api "repos/$repo" > "$pr_files/repo.json"
gh api "repos/$repo/issues/18" > "$pr_files/issue18.json"
python3 - "$pr_files" "$candidate" <<'PYBODY'
import json
from pathlib import Path
import re
import sys

root = Path(sys.argv[1])
pr = json.loads((root / 'pull-before.json').read_text())
repo = json.loads((root / 'repo.json').read_text())
issue = json.loads((root / 'issue18.json').read_text())
if pr['head']['sha'] != sys.argv[2]:
    raise SystemExit('O PR mudou durante a preparação; execute novamente.')
if not repo.get('allow_squash_merge'):
    raise SystemExit('Squash indisponível; conferir a estratégia de integração antes de publicar a orientação.')
if issue['state'] != 'open':
    raise SystemExit('A issue #18 não está aberta. Conferir o estado antes de continuar.')

body = pr.get('body') or ''
(root / 'body-before.md').write_text(body)
keyword = r'(?:close[sd]?|fix(?:es|ed)?|resolve[sd]?)'
reference = r'(?:Feharia/Grupo-2---Avaliacao-2---SBD---UFRJ)?#18\b'
pattern = re.compile(r'\b' + keyword + r'\s*:?\s+(?:' + reference +
                     r'|https://github\.com/Feharia/Grupo-2---Avaliacao-2---SBD---UFRJ/issues/18\b)', re.I)


def revise(text):
    for line in text.splitlines():
        matches = list(re.finditer(r'\b' + keyword + r'\b', line, re.I))
        for i, match in enumerate(matches):
            end = matches[i + 1].start() if i + 1 < len(matches) else len(line)
            clause = line[match.start():end].replace('Referência pendente: #18', '')
            refs = [int(a or b) for a, b in re.findall(r'#(\d+)\b|/issues/(\d+)\b', clause)]
            if 18 not in refs:
                continue
            if any(n != 18 for n in refs) or not pattern.search(clause):
                raise SystemExit('Referência ambígua à #18; revisar body-before.md antes de editar o PR.')
    return pattern.sub('Referência pendente: #18', text)


body = revise(body)
title = revise(pr['title'])
start = '<!-- pr19-integracao-revisao -->'
end = '<!-- /pr19-integracao-revisao -->'
notice = '''Preparação da integração após revisão funcional:

- Evidências de I/O: 168 cenários, 29 alertas explicados pela cobrança de índices e auxiliares no orçamento M; sem divergências de tuplas SQLite.
- A issue #18 permanece pendente: gráficos e tamanho de bloco no manifesto.
- Integração prevista por **Squash and merge**, com mensagem final explícita revisada. Não copiar a mensagem automática dos commits: o commit histórico dc1b1ea contém uma instrução de fechamento da #18.
- Antes do merge, revisar também eventuais vínculos manuais com a #18. A edição desta descrição não altera a mensagem do commit histórico.
- Orientações e limites em docs/INTEGRACAO_PR_19.md. O merge na main ainda depende de autorização do usuário.'''
block = start + '\n' + notice + '\n' + end
if start in body:
    if end not in body:
        raise SystemExit('Marcador de integração incompleto; conferir a descrição.')
    body = re.sub(re.escape(start) + r'.*?' + re.escape(end), lambda _: block, body, flags=re.S)
else:
    body = body.rstrip() + '\n\n' + block + '\n'
(root / 'body-after.md').write_text(body)
(root / 'update.json').write_text(json.dumps({'body': body, 'title': title}, ensure_ascii=False) + '\n')
PYBODY

wait_pr_head "$pr_files/pull-before-patch.json"
python3 - "$pr_files" "$candidate" <<'PYCHECK'
import json,sys
from pathlib import Path
p=Path(sys.argv[1]); before=json.loads((p/'pull-before.json').read_text()); current=json.loads((p/'pull-before-patch.json').read_text())
if any(current.get(k)!=before.get(k) for k in ('body','title')):
    raise SystemExit('A descrição ou título foi editado durante a preparação. Nenhum texto foi sobrescrito; execute novamente para usar o texto atual.')
PYCHECK

gh api --method PATCH "repos/$repo/pulls/19" --input "$pr_files/update.json" > "$pr_files/pull-after.json"
python3 - "$pr_files" "$candidate" <<'PYAFTER'
import json,sys
from pathlib import Path
p=Path(sys.argv[1]); desired=json.loads((p/'update.json').read_text()); actual=json.loads((p/'pull-after.json').read_text())
if actual['head']['sha']!=sys.argv[2] or any(actual[k]!=desired[k] for k in ('body','title')):
    raise SystemExit('O estado do PR diverge da atualização solicitada; conferir os arquivos preservados.')
PYAFTER

wait_ci() {
  local ci_branch=$1 ci_event=$2 run_id='' conclusion='' attempt
  printf '\nAguardando Actions: branch=%s, evento=%s, SHA=%s\n' "$ci_branch" "$ci_event" "$candidate"
  for ((attempt=1; attempt<=36; attempt++)); do
    run_id=$(gh run list --repo "$repo" --branch "$ci_branch" --commit "$candidate" \
      --event "$ci_event" --workflow "$workflow" --limit 10 \
      --json databaseId --jq '.[0].databaseId // empty')
    [[ -n "$run_id" ]] && break
    sleep 5
  done
  [[ -n "$run_id" ]] || fail "Actions não apareceu para $ci_branch ($ci_event). Não avance sem conferir."
  gh run watch "$run_id" --repo "$repo" --interval 10 --exit-status
  conclusion=$(gh run view "$run_id" --repo "$repo" --json conclusion --jq .conclusion)
  [[ "$conclusion" == success ]] || fail "Actions $run_id terminou com $conclusion."
  gh run view "$run_id" --repo "$repo" --json headSha,conclusion,url
}

wait_ci "$pr_branch" push
wait_ci "$pr_branch" pull_request

wait_pr_head "$pr_files/pull-final.json"
current_pr_sha=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["head"]["sha"])' "$pr_files/pull-final.json")
[[ "$current_pr_sha" == "$candidate" ]] || fail 'o HEAD do PR mudou; confira os novos commits e seus checks.'
gh pr checks 19 --repo "$repo" --watch --interval 10
wait_pr_head "$pr_files/pull-final.json"
current_pr_sha=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["head"]["sha"])' "$pr_files/pull-final.json")
[[ "$current_pr_sha" == "$candidate" ]] || fail 'o HEAD do PR mudou durante os checks.'
printf '\nDescrição atualizada e CI aprovado para os registros já enviados. A #18 permanece pendente; o merge futuro deve ser por squash com mensagem revisada. Main não foi alterada.\n'
gh pr view 19 --repo "$repo" --json url,headRefName,headRefOid,baseRefName,state
