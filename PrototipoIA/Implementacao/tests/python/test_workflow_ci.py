"""Regressão da disponibilidade de contextos no env de jobs do Actions.

Contrato: https://docs.github.com/en/actions/reference/workflows-and-actions/contexts
É uma verificação dirigida; não substitui actionlint nem execução hospedada.
"""
from pathlib import Path
import re
import unittest


class WorkflowContextsTest(unittest.TestCase):
    def test_artifacts_require_successful_path_initialization(self):
        workflow = Path(__file__).resolve().parents[4] / ".github/workflows/correcao.yml"
        text = workflow.read_text()
        initialization = text.split("- name: Inicializar diretórios do runner\n", 1)[1].split("\n      - name:", 1)[0]
        upload = text.split("- name: Preservar evidências\n", 1)[1].split("\n  gate-correcao:", 1)[0]
        self.assertRegex(initialization, r"(?m)^        id: diretorios$")
        self.assertRegex(upload, r"if:.*always\(\).*steps\.diretorios\.outcome == 'success'")

    def test_job_env_does_not_reference_runtime_contexts(self):
        workflow = Path(__file__).resolve().parents[4] / ".github/workflows/correcao.yml"
        text = workflow.read_text()
        # Blocos job.env da configuração atual; steps.env tem outra indentação.
        blocks = re.findall(r"(?m)^    env:\n((?:^      [^\n]*\n|^\n)*)", text)
        self.assertTrue(blocks, "nenhum bloco job.env encontrado")
        for block in blocks:
            for expression in re.findall(r"\$\{\{(.*?)\}\}", block, re.S):
                with self.subTest(expression=expression):
                    self.assertNotRegex(expression, r"\b(?:runner|env|steps|job)\s*\.")


if __name__ == "__main__":
    unittest.main()
