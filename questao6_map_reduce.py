"""Questão 6: soma e histograma paralelo com map local e reduce principal."""

import argparse
import random
import time
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path


def gerar_arquivo(caminho: Path, quantidade: int, maximo: int, seed: int = 42) -> None:
    rng = random.Random(seed)
    with caminho.open("w", encoding="utf-8") as arquivo:
        for _ in range(quantidade):
            arquivo.write(f"{rng.randint(0, maximo)}\n")


def ler_inteiros(caminho: Path) -> list[int]:
    with caminho.open(encoding="utf-8") as arquivo:
        return [int(token) for linha in arquivo for token in linha.split()]


def mapear(bloco: list[int]) -> tuple[int, Counter]:
    # Trabalho independente: não há estado global nem lock entre os processos.
    return sum(bloco), Counter(bloco)


def particionar(valores: list[int], partes: int) -> list[list[int]]:
    tamanho = max(1, (len(valores) + partes - 1) // partes)
    return [valores[i:i + tamanho] for i in range(0, len(valores), tamanho)]


def calcular(valores: list[int], processos: int) -> tuple[int, Counter, float]:
    blocos = particionar(valores, processos)
    inicio = time.perf_counter()
    if processos == 1:
        mapas = map(mapear, blocos)
    else:
        with ThreadPoolExecutor(max_workers=processos) as executor:
            mapas = executor.map(mapear, blocos)
            mapas = list(mapas)
    soma_total = 0
    histograma = Counter()
    for soma_local, histograma_local in mapas:  # reduce na principal
        soma_total += soma_local
        histograma.update(histograma_local)
    return soma_total, histograma, time.perf_counter() - inicio


def benchmark(caminho: Path) -> None:
    valores = ler_inteiros(caminho)
    soma_referencia, hist_referencia = sum(valores), Counter(valores)
    tempos = {}
    print(f"Inteiros lidos: {len(valores):,}")
    print("P | tempo (s) | speedup")
    print("--|-----------|--------")
    for p in (1, 2, 4, 8):
        soma, histograma, duracao = calcular(valores, p)
        assert soma == soma_referencia and histograma == hist_referencia
        tempos[p] = duracao
        print(f"{p} | {duracao:9.6f} | {tempos[1] / duracao:7.3f}x")
    print(f"Soma total: {soma_referencia}")
    print("Histograma (valor: frequência):")
    print(dict(sorted(hist_referencia.items())))

"""Executar em outra celula no colab"""
from pathlib import Path

arquivo = Path("inteiros.txt")

gerar_arquivo(
    caminho=arquivo,
    quantidade=1_000_000,
    maximo=100
)

"""Executar em outra celula no colab"""
benchmark(arquivo)
