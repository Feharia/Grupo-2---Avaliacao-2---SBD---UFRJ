#!/usr/bin/env bash
set -euo pipefail
repo='Feharia/Grupo-2---Avaliacao-2---SBD---UFRJ'
worktree='/home/carlos/Documentos/UFRJ/cbd/sistema-banco-dados/pr19-slot-prefetch'
sha='d95acc612b7885e6d35839f15c227c1cd527fca0'
tag='entrega-programa-2026-11-27'
evidence="$worktree/evidencias/CONGELAMENTO/2026-11-27-entrega"
fail() { printf 'Interrompido: %s\n' "$*" >&2; exit 1; }
for command in git gh python3 sha256sum; do command -v "$command" >/dev/null || fail "Comando ausente: $command"; done
cd "$worktree"
case "$(git remote get-url --push origin)" in
  "git@github.com:$repo.git"|"https://github.com/$repo.git"|"https://github.com/$repo") ;;
  *) fail 'O origin não corresponde ao repositório do projeto.' ;;
esac
[[ -f "$evidence/ENTREGA.sha256" ]] || fail 'Evidências da rodada oficial não encontradas.'
(cd "$evidence" && sha256sum --check ENTREGA.sha256 > /dev/null)
files=$(mktemp -d /tmp/cbd-tag-entrega-XXXXXX)
printf 'Evidências da publicação: %s\n' "$files"
python3 - "$evidence/aceitacao.json" "$sha" "$files/mensagem.txt" <<'PY'
import json,sys
from pathlib import Path
r=json.loads(Path(sys.argv[1]).read_text())
if (r['commit']!=sys.argv[2] or not r['clean_checkout'] or r['combinations']!=168
    or r['required_executed']!=108 or r['required_not_applicable']!=12
    or any(r[k]!=0 for k in ('required_missing','required_infeasible','sqlite_missing','sqlite_different','io_not_converged','final_flush_nonzero'))):
    raise SystemExit('Aceitação oficial incompleta ou diferente do SHA a publicar.')
Path(sys.argv[3]).write_text('Entrega do programa CBD/COS480 — 27/11/2026\n\n'
    'Commit: '+sys.argv[2]+'\n'
    'Rodada oficial: escala 1, LRU/MRU, M=32/2, lote 1000, semente 20261122, 3..10 repetições.\n'
    +str(r['full_rows'])+' linhas medidas em 168 combinações; SQLite e matriz obrigatória aprovados.\n'
    'E/S convergiu em todos os grupos; '+str(r['time_not_converged'])+' grupos sem convergência temporal (tempo complementar).\n'
    'Evidências locais: evidencias/CONGELAMENTO/2026-11-27-entrega/.\n')
PY
main_sha=$(gh api "repos/$repo/git/ref/heads/main" --jq '.object.sha')
[[ "$main_sha" == "$sha" ]] || fail 'A main mudou após a aceitação; conferir os novos commits antes de marcar a entrega.'
gh run list --repo "$repo" --branch main --commit "$sha" --event push \
  --workflow correcao.yml --limit 20 --json status,conclusion,headSha,url > "$files/ci-main.json"
python3 - "$files/ci-main.json" "$sha" <<'PY'
import json,sys
from pathlib import Path
r=json.loads(Path(sys.argv[1]).read_text())
if not r or r[0]['status']!='completed' or r[0]['conclusion']!='success' or r[0]['headSha']!=sys.argv[2]:
    raise SystemExit('CI da main não confirma sucesso no SHA da entrega.')
PY
git cat-file -e "$sha^{commit}"
git ls-remote --tags origin "refs/tags/$tag" "refs/tags/$tag^{}" > "$files/tag-before.txt"
if [[ -s "$files/tag-before.txt" ]]; then
  remote_target=$(awk '$2 ~ /\^\{\}$/ {print $1}' "$files/tag-before.txt")
  [[ "$remote_target" == "$sha" ]] || fail 'Já existe uma tag remota com esse nome e outro alvo/formato; nada será sobrescrito.'
  if ! git show-ref --verify --quiet "refs/tags/$tag"; then
    git fetch origin "refs/tags/$tag:refs/tags/$tag"
  fi
  remote_object=$(awk '$2 !~ /\^\{\}$/ {print $1}' "$files/tag-before.txt")
  [[ "$(git rev-parse "refs/tags/$tag")" == "$remote_object" ]] || fail 'Tag local difere da remota; nada será sobrescrito.'
fi
if git show-ref --verify --quiet "refs/tags/$tag"; then
  [[ "$(git cat-file -t "refs/tags/$tag")" == tag ]] || fail 'A tag local existente não é anotada.'
  [[ "$(git rev-parse "$tag^{commit}")" == "$sha" ]] || fail 'A tag local existente aponta para outro commit.'
else
  git tag -a "$tag" "$sha" --file "$files/mensagem.txt"
fi
# Publica somente esta tag; não envia branches, commits novos ou outras tags.
git push origin "refs/tags/$tag:refs/tags/$tag"
git ls-remote --tags origin "refs/tags/$tag" "refs/tags/$tag^{}" > "$files/tag-after.txt"
python3 - "$files" "$tag" "$sha" "$evidence" "$worktree" <<'PY'
import json,sys,shutil
from pathlib import Path
p=Path(sys.argv[1]); tag,sha=sys.argv[2:4]; evidence=Path(sys.argv[4]); worktree=Path(sys.argv[5])
refs={line.split()[1]:line.split()[0] for line in (p/'tag-after.txt').read_text().splitlines()}
if refs.get('refs/tags/'+tag+'^{}')!=sha or not refs.get('refs/tags/'+tag):
    raise SystemExit('Push terminou, mas o alvo remoto da tag não foi confirmado; conferir antes de declarar congelado.')
result={'tag':tag,'commit':sha,'tag_object':refs['refs/tags/'+tag],'ciMain':json.loads((p/'ci-main.json').read_text())[0],
        'evidenceDir':str(evidence),'publicationEvidenceDir':str(p),'published':True}
(p/'publicacao.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
Path('/tmp/cbd-programa-congelado.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
shutil.copytree(p,evidence/'publicacao'/p.name)
marker='### Programa marcado para entrega — '+tag
post=worktree/'POST_MORTEM.md'
if marker not in post.read_text():
    with post.open('a') as f:
        f.write('\n'+marker+'\n\n- Tag anotada `'+tag+'` publicada no SHA `'+sha+'`, após rodada oficial\n'
                '  aprovada e confirmação do CI da main. Alvo remoto conferido via ls-remote.\n'
                '- Evidências: `'+str(evidence.relative_to(worktree))+'/`.\n'
                '- O envio publica somente a tag; branches e alterações locais preservadas.\n')
print(json.dumps(result,ensure_ascii=False,indent=2))
PY
printf '\nPrograma marcado para entrega: %s (%s).\n' "$tag" "$sha"
