import threading
import time
import random
from collections import deque


NUM_PRODUTORES = 6
NUM_CONSUMIDORES = 2

ITENS_POR_PRODUTOR = 40

TAMANHO_BUFFER = 10


BURST_MIN = 2
BURST_MAX = 6


OCIOSIDADE_MIN = 0.02
OCIOSIDADE_MAX = 0.08


CONSUMO_MIN = 0.03
CONSUMO_MAX = 0.12



buffer = deque()

mutex = threading.Lock()

cond_nao_vazio = threading.Condition(mutex)
cond_nao_cheio = threading.Condition(mutex)

produtores_ativos = NUM_PRODUTORES


# MÉTRICAS


ocupacoes = []

soma_espera = 0.0
itens_consumidos = 0

inicio_experimento = time.monotonic()


def registrar_ocupacao():

    tempo = time.monotonic() - inicio_experimento

    ocupacoes.append(
        (tempo, len(buffer))
    )


# PRODUTOR


def produtor(id_produtor):

    global produtores_ativos

    random.seed(time.time() + id_produtor)

    produzidos = 0

    while produzidos < ITENS_POR_PRODUTOR:


        # BURST

        tamanho_burst = random.randint(
            BURST_MIN,
            BURST_MAX
        )

        tamanho_burst = min(
            tamanho_burst,
            ITENS_POR_PRODUTOR - produzidos
        )

        print(
            f"[Produtor {id_produtor}] "
            f"Iniciando burst de {tamanho_burst} itens"
        )

        for _ in range(tamanho_burst):

            # Simula produção rápida
            time.sleep(
                random.uniform(0.001, 0.01)
            )

            with cond_nao_cheio:

      
                # BACKPRESSURE
           

                while len(buffer) >= TAMANHO_BUFFER:

                    print(
                        f"[Produtor {id_produtor}] "
                        f"BACKPRESSURE - buffer cheio"
                    )

                    cond_nao_cheio.wait()

                item = (
                    id_produtor,
                    produzidos,
                    time.monotonic()
                )

                buffer.append(item)

                produzidos += 1

                registrar_ocupacao()

                print(
                    f"[Produtor {id_produtor}] "
                    f"produziu item {produzidos} "
                    f"| buffer = {len(buffer)}/{TAMANHO_BUFFER}"
                )

                cond_nao_vazio.notify()

        tempo_ocioso = random.uniform(
            OCIOSIDADE_MIN,
            OCIOSIDADE_MAX
        )

        print(
            f"[Produtor {id_produtor}] "
            f"ocioso por {tempo_ocioso:.3f}s"
        )

        time.sleep(tempo_ocioso)


    with cond_nao_vazio:

        produtores_ativos -= 1

        if produtores_ativos == 0:
            cond_nao_vazio.notify_all()



# CONSUMIDOR


def consumidor(id_consumidor):

    global soma_espera
    global itens_consumidos

    random.seed(
        time.time() + id_consumidor + 1000
    )

    while True:

        with cond_nao_vazio:

           

            while (
                len(buffer) == 0
                and produtores_ativos > 0
            ):

                cond_nao_vazio.wait()

    

            if (
                len(buffer) == 0
                and produtores_ativos == 0
            ):

                break

            item = buffer.popleft()

            agora = time.monotonic()

            tempo_espera = (
                agora - item[2]
            )

            soma_espera += tempo_espera

            itens_consumidos += 1

            registrar_ocupacao()

            print(
                f"[Consumidor {id_consumidor}] "
                f"consumiu item "
                f"| buffer = "
                f"{len(buffer)}/{TAMANHO_BUFFER}"
            )

     

            cond_nao_cheio.notify()

 

        tempo_consumo = random.uniform(
            CONSUMO_MIN,
            CONSUMO_MAX
        )

        time.sleep(tempo_consumo)


# EXECUÇÃO DO EXPERIMENTO


def executar():

    global inicio_experimento

    inicio_experimento = time.monotonic()

    produtores = []
    consumidores = []


    print("QUESTÃO 8 - BUFFER COM BURSTS E BACKPRESSURE")

    print(
        f"\nProdutores: {NUM_PRODUTORES}"
    )

    print(
        f"Consumidores: {NUM_CONSUMIDORES}"
    )

    print(
        f"Tamanho do buffer: {TAMANHO_BUFFER}"
    )

    print(
        f"Itens por produtor: {ITENS_POR_PRODUTOR}"
    )

    print()



    for i in range(NUM_PRODUTORES):

        t = threading.Thread(
            target=produtor,
            args=(i,)
        )

        produtores.append(t)

        t.start()


    for i in range(NUM_CONSUMIDORES):

        t = threading.Thread(
            target=consumidor,
            args=(i,)
        )

        consumidores.append(t)

        t.start()


    for t in produtores:
        t.join()



    for t in consumidores:
        t.join()



    tempo_total = (
        time.monotonic() -
        inicio_experimento
    )

    ocupacao_media = (
        sum(o[1] for o in ocupacoes)
        / len(ocupacoes)
    )

    ocupacao_maxima = max(
        o[1] for o in ocupacoes
    )

    espera_media = (
        soma_espera /
        itens_consumidos
    )

    vazao = (
        itens_consumidos /
        tempo_total
    )


    print("RESULTADOS")


    print(
        f"Tempo total: {tempo_total:.2f} s"
    )

    print(
        f"Itens consumidos: {itens_consumidos}"
    )

    print(
        f"Vazão: {vazao:.2f} itens/s"
    )

    print(
        f"Espera média no buffer: "
        f"{espera_media * 1000:.2f} ms"
    )

    print(
        f"Ocupação média do buffer: "
        f"{ocupacao_media:.2f} itens"
    )

    print(
        f"Ocupação máxima do buffer: "
        f"{ocupacao_maxima} itens"
    )

if __name__ == "__main__":
    executar()
