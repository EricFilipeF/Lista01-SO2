"""Questão 5: pool fixo de threads para tarefas de primalidade."""

import argparse
import math
import queue
import sys
import threading
from dataclasses import dataclass

POISON = object()


@dataclass(frozen=True)
class Tarefa:
    id: int
    numero: int


def eh_primo(numero: int) -> bool:
    if numero < 2:
        return False
    if numero == 2:
        return True
    if numero % 2 == 0:
        return False
    limite = math.isqrt(numero)
    return all(numero % divisor for divisor in range(3, limite + 1, 2))


def executar(n_threads: int, linhas) -> list[tuple[int, int, bool]]:
    tarefas: queue.Queue = queue.Queue()
    resultados: queue.Queue = queue.Queue()
    enviados = 0

    def trabalhador() -> None:
        while True:
            tarefa = tarefas.get()
            try:
                if tarefa is POISON:
                    return
                resultados.put((tarefa.id, tarefa.numero, eh_primo(tarefa.numero)))
            finally:
                tarefas.task_done()

    workers = [threading.Thread(target=trabalhador) for _ in range(n_threads)]
    for worker in workers:
        worker.start()

    for linha in linhas:
        linha = linha.strip()
        if not linha:
            continue
        tarefas.put(Tarefa(enviados, int(linha)))
        enviados += 1

    for _ in workers:
        tarefas.put(POISON)
    tarefas.join()
    for worker in workers:
        worker.join()

    saida = [resultados.get() for _ in range(enviados)]
    saida.sort()  # ordem determinística pela identificação da entrada
    assert len(saida) == enviados
    assert [item[0] for item in saida] == list(range(enviados)), "Tarefa perdida/duplicada"
    return saida


numeros = [
    "2",
    "17",
    "18",
    "7919",
    "1"
]

resultados = executar(n_threads=4, linhas=numeros)

for _, numero, primo in resultados:
    print(f"{numero}: {'primo' if primo else 'não primo'}")
