"""Questão 4: pipeline captura -> processamento -> gravação."""

import argparse
import queue
import threading
import time

POISON = object()


def executar(n: int, capacidade: int = 8) -> list[tuple[int, int]]:
    fila_captura: queue.Queue = queue.Queue(maxsize=capacidade)
    fila_processados: queue.Queue = queue.Queue(maxsize=capacidade)
    gravados: list[tuple[int, int]] = []

    def captura() -> None:
        for item in range(n):
            fila_captura.put(item)
        fila_captura.put(POISON)

    def processamento() -> None:
        while True:
            item = fila_captura.get()
            try:
                if item is POISON:
                    fila_processados.put(POISON)
                    return
                fila_processados.put((item, item * item))
            finally:
                fila_captura.task_done()

    def gravacao() -> None:
        while True:
            resultado = fila_processados.get()
            try:
                if resultado is POISON:
                    return
                gravados.append(resultado)
            finally:
                fila_processados.task_done()

    threads = [
        threading.Thread(target=captura, name="captura"),
        threading.Thread(target=processamento, name="processamento"),
        threading.Thread(target=gravacao, name="gravacao"),
    ]
    inicio = time.perf_counter()
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join()
    duracao = time.perf_counter() - inicio

    esperados = [(i, i * i) for i in range(n)]
    assert gravados == esperados, "Houve perda, duplicação ou reordenação de itens"
    assert all(not thread.is_alive() for thread in threads), "Thread não finalizada"
    print(f"Processados {len(gravados)}/{n} itens em {duracao:.6f}s; sem perdas.")
    return gravados


executar(n=1000, capacidade=8)
