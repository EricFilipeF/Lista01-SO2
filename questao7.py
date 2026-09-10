import threading
import time
import random

NUM_FILOSOFOS = 5
REFEICOES_POR_FILOSOFO = 10



class Filosofo:
    def __init__(self, id):
        self.id = id
        self.refeicoes = 0
        self.maior_espera = 0.0



# SOLUÇÃO A
# ORDEM GLOBAL DE AQUISIÇÃO DOS GARFOS

def solucao_ordem_global():

    print("SOLUÇÃO A - ORDEM GLOBAL DE AQUISIÇÃO")
  
    garfos = [threading.Lock() for _ in range(NUM_FILOSOFOS)]
    filosofos = [Filosofo(i) for i in range(NUM_FILOSOFOS)]

    def executar(f):

        esquerdo = f.id
        direito = (f.id + 1) % NUM_FILOSOFOS

        # Sempre pega primeiro o garfo de menor número
        primeiro = min(esquerdo, direito)
        segundo = max(esquerdo, direito)

        for _ in range(REFEICOES_POR_FILOSOFO):

            # Pensando
            time.sleep(random.uniform(0.01, 0.05))

            inicio_espera = time.monotonic()

            # Aquisição ordenada
            garfos[primeiro].acquire()
            garfos[segundo].acquire()

            fim_espera = time.monotonic()

            espera = fim_espera - inicio_espera

            if espera > f.maior_espera:
                f.maior_espera = espera

            # Comendo
            time.sleep(random.uniform(0.01, 0.03))
            f.refeicoes += 1

            garfos[segundo].release()
            garfos[primeiro].release()

    threads = []

    for f in filosofos:
        t = threading.Thread(target=executar, args=(f,))
        threads.append(t)
        t.start()

    for t in threads:
        t.join()

    print("\nMétricas:")
    for f in filosofos:
        print(
            f"Filósofo {f.id}: "
            f"refeições = {f.refeicoes}, "
            f"maior espera = {f.maior_espera:.4f}s"
        )


# SOLUÇÃO B
# SEMÁFORO LIMITANDO PARA 4 FILÓSOFOS

def solucao_semaforo():

    print("SOLUÇÃO B - SEMÁFORO COM 4 FILÓSOFOS")

    garfos = [threading.Lock() for _ in range(NUM_FILOSOFOS)]

    # Permite no máximo 4 filósofos tentando comer
    limite = threading.Semaphore(4)

    filosofos = [Filosofo(i) for i in range(NUM_FILOSOFOS)]

    def executar(f):

        esquerdo = f.id
        direito = (f.id + 1) % NUM_FILOSOFOS

        for _ in range(REFEICOES_POR_FILOSOFO):

            # Pensando
            time.sleep(random.uniform(0.01, 0.05))

            inicio_espera = time.monotonic()

          
            limite.acquire()

            # Pega os dois garfos
            garfos[esquerdo].acquire()
            garfos[direito].acquire()

            fim_espera = time.monotonic()

            espera = fim_espera - inicio_espera

            if espera > f.maior_espera:
                f.maior_espera = espera

            # Comendo
            time.sleep(random.uniform(0.01, 0.03))
            f.refeicoes += 1

            garfos[direito].release()
            garfos[esquerdo].release()

            # Libera uma vaga
            limite.release()

    threads = []

    for f in filosofos:
        t = threading.Thread(target=executar, args=(f,))
        threads.append(t)
        t.start()

    for t in threads:
        t.join()

    print("\nMétricas:")
    for f in filosofos:
        print(
            f"Filósofo {f.id}: "
            f"refeições = {f.refeicoes}, "
            f"maior espera = {f.maior_espera:.4f}s"
        )




if __name__ == "__main__":

    solucao_ordem_global()
    solucao_semaforo()
