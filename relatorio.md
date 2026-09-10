## Questão 1 - Corrida de cavalos com threads

### Enunciado

Cada cavalo é uma thread que anda em passos aleatórios até a linha de
chegada. O usuário aposta antes da largada. Todas as threads começam juntas.
O placar é atualizado com mutex. No final, mostra o vencedor e se a aposta
acertou. Empates precisam ter resultado sempre igual (determinístico) e não
pode dar condição de corrida ao definir o vencedor.

### Ideia da solução

Dividi a corrida em **rodadas**:

1. Todo cavalo anda um passo.
2. Todos esperam uns aos outros terminar de andar.
3. Só uma thread (a do cavalo 0) confere o placar e decide se alguém venceu.
4. Todos esperam essa checagem terminar antes da próxima rodada.

Como só uma thread decide o vencedor, e sempre na mesma ordem, o resultado
não depende de sorte do processador.

### Sincronização usada

- **Mutex** (`mutex_placar`): protege o vetor de posições, a variável do
  vencedor e os prints. Só uma thread mexe nisso por vez.
- **Barreira de largada**: todas as threads esperam aqui antes de andar.
  Garante que ninguém saia na frente só porque foi criada primeiro.
- **Barreira de rodada**: espera todos terminarem de andar antes de checar o
  vencedor.
- **Barreira de checagem**: espera a apuração do cavalo 0 terminar antes de
  liberar a próxima rodada.

### Cavalo correndo na tela

No começo eu só imprimia uma barra de progresso por rodada, uma embaixo da
outra, e ficava tudo rolando na tela sem parecer uma corrida de verdade.
Troquei por uma pista fixa: a cada rodada eu limpo o terminal com o código
ANSI `\033[H\033[J` e redesenho tudo de novo, marcando a posição atual de
cada cavalo na pista. Como isso se repete a cada rodada, dá a impressão de o
cavalo estar "andando" na tela.

Primeiro usei uma letra (A, B, C...) pra marcar cada cavalo, mas ficou sem
graça - não parecia um cavalo de verdade. Troquei pelo emoji de cavalo
(🐎). Como o emoji é igual pra todo mundo, não dava pra saber qual cavalo
era qual só olhando, então usei cores ANSI diferentes (vermelho, verde,
amarelo, azul, magenta) pro nome e pro emoji de cada cavalo, uma cor fixa
por cavalo do início ao fim da corrida.

Também adicionei uma pequena pausa (`usleep`) entre uma rodada e outra, senão
a corrida passava rápido demais e não dava pra acompanhar.

### Como resolvi o empate

Depois de cada rodada, só o cavalo 0 roda este laço:

```c
for (int i = 0; i < NUM_CAVALOS; i++) {
    if (posicao[i] >= DISTANCIA && vencedor == -1) {
        vencedor = i;
    }
}
```

O laço sempre passa pelos cavalos na mesma ordem (0, 1, 2...) e só grava o
vencedor uma vez. Se dois cavalos cruzarem a linha na mesma rodada, o de
menor número sempre ganha. Isso dá sempre o mesmo resultado, não muda entre
execuções. E como só existe um lugar no código que escreve o vencedor,
protegido por mutex, não tem condição de corrida.

### Por que usei `rand_r` e não `rand()`

`rand()` usa um estado global e não é seguro entre threads. Troquei por
`rand_r(&seed)`, com uma semente própria para cada cavalo, para os sorteios
serem independentes.

### Resumo do código

1. `main()` mostra os cavalos e pede a aposta (valida o número digitado).
2. Cria o mutex, as barreiras e uma thread por cavalo.
3. Cada thread: espera a largada, anda em rodadas (com mutex), e só a
   thread 0 checa o vencedor e imprime o placar a cada rodada.
4. `main()` espera todas as threads terminarem, mostra o vencedor e diz se a
   aposta acertou.
5. No final, libera o mutex e as barreiras.

### Como compilar e executar

```bash
gcc questao1.c -o questao1
./questao1
```

## Questão 2 - Buffer circular com produtores e consumidores

### Enunciado

Buffer circular de tamanho N acessado por vários produtores e vários
consumidores ao mesmo tempo. Precisa de mutex e semáforos ou variáveis de
condição, com espera ativa zero (nenhuma thread pode ficar girando em loop
só checando uma condição). Os itens são gerados com tempos aleatórios. O
programa precisa medir vazão (throughput) e tempo médio de espera, e mostrar
por experimento como o tamanho do buffer muda o desempenho.

### Ideia da solução

Usei o problema clássico do produtor-consumidor: um vetor circular
(`buffer[N]`) com três variáveis de controle (`inicio`, `fim`, `contagem`),
protegido por um mutex e duas variáveis de condição:

- `cond_nao_cheio`: o produtor dorme aqui se o buffer está cheio.
- `cond_nao_vazio`: o consumidor dorme aqui se o buffer está vazio.

Cada item guarda o horário (`clock_gettime`) em que entrou no buffer. Quando
um consumidor tira o item, calcula a diferença entre agora e esse horário -
isso dá o tempo de espera daquele item.

### Por que não tem espera ativa

A parte importante é usar `pthread_cond_wait` em vez de um `while` vazio
checando a condição sem parar. `pthread_cond_wait` faz duas coisas: solta o
mutex e coloca a thread pra **dormir** até alguém chamar `pthread_cond_signal`
ou `pthread_cond_broadcast`. Enquanto está dormindo, a thread não gasta CPU
nenhuma - diferente de um loop `while (buffer_cheio) { }` que ficaria
consumindo processador o tempo todo só esperando.

```c
while (contagem == tamanho_buffer) {
    pthread_cond_wait(&cond_nao_cheio, &mutex_buffer);
}
```

Uso `while` e não `if` porque, quando a thread acorda, alguma outra thread
pode ter mexido no buffer de novo antes dela conseguir o mutex - então
preciso checar a condição de novo, não só confiar que "se acordou, é porque
pode continuar".

### Encerrando as threads direito

Cada produtor produz uma quantidade fixa de itens e depois termina. O
problema é avisar os consumidores quando não vai ter mais item nenhum,
senão eles ficam dormindo pra sempre esperando. Resolvi com um contador
`produtores_ativos`: quando o último produtor termina, ele dá um
`pthread_cond_broadcast` (acorda todo mundo, não só um) pra todos os
consumidores que estejam dormindo perceberem que acabou e possam sair do
laço.

### Estatísticas

- **Tempo médio de espera**: soma o tempo de espera de cada item
  (calculado no consumidor) e divide pela quantidade de itens consumidos.
- **Vazão (throughput)**: mede o tempo total da simulação do início ao fim
  (com `clock_gettime`) e divide a quantidade de itens consumidos por esse
  tempo, dando itens processados por segundo.

### O experimento com tamanhos de buffer diferentes

Rodei a simulação inteira várias vezes, cada vez com um `N` diferente
(1, 2, 4, 8, 16, 32), sempre com os mesmos 6 produtores e 2 consumidores.

Na primeira tentativa os consumidores eram rápidos demais e o buffer quase
nunca enchia - dava praticamente o mesmo resultado pra qualquer tamanho de
buffer, o que não mostrava nada interessante. Ajustei o experimento pra os
consumidores serem propositalmente mais lentos que os produtores (produtor
demora até 3ms por item, consumidor demora até 15ms), criando uma fila de
verdade no buffer.

Resultado do experimento:

| Tamanho N | Vazão (itens/s) | Espera média (ms) |
|-----------|------------------|--------------------|
| 1         | 241,85           | 3,89               |
| 2         | 229,64           | 8,39               |
| 4         | 226,86           | 16,91              |
| 8         | 255,37           | 29,27              |
| 16        | 248,01           | 58,72              |
| 32        | 239,46           | 116,11             |

O que isso mostra: como os consumidores são o gargalo (são mais lentos que
os produtores), a **vazão fica praticamente igual** em todos os tamanhos de
buffer - ela é limitada pela velocidade dos consumidores, não pelo tamanho
do buffer. Só que o **tempo médio de espera cresce muito** conforme o
buffer aumenta: com buffer pequeno, o item espera pouco porque tem poucos
itens na fila na frente dele; com buffer grande, cabem muito mais itens
esperando ao mesmo tempo, então cada item fica mais tempo parado até
chegar a sua vez. Ou seja, um buffer maior não deixa o sistema mais rápido
quando o consumidor é o gargalo - só deixa mais itens acumulados esperando,
com tempo de espera maior.

### Como compilar e executar

```bash
gcc questao2.c -o questao2
./questao2
```

## Questão 3 - Transferências bancárias com M contas e T threads

### Enunciado

M contas e T threads fazendo transferências aleatórias entre elas. Os
saldos precisam ser protegidos com travas adequadas (um mutex por conta, ou
travas por partição). É preciso provar, por asserção, que a soma global do
dinheiro nunca muda. E é preciso comparar uma execução correta (com trava)
com uma execução incorreta (sem trava) para mostrar a condição de corrida
acontecendo de verdade.

### Ideia da solução

Cada conta tem seu próprio saldo e seu próprio mutex (`mutex_conta[i]`).
Uma transferência mexe em duas contas (origem e destino), então preciso
travar as duas antes de mexer nos saldos.

A função `transferir()` recebe um parâmetro `usar_trava`: se for 1, ela usa
os mutexes certinho (versão correta); se for 0, ela mexe direto nos saldos
sem proteção nenhuma (versão incorreta, só pra comparação). O `main()`
roda a simulação inteira duas vezes: uma com trava, outra sem.

### Evitando deadlock ao travar duas contas

Se cada thread travasse a conta de origem primeiro e depois a de destino,
dava pra acontecer isso: a thread 1 quer transferir da conta 3 pra conta 7 e
trava a conta 3; ao mesmo tempo, a thread 2 quer transferir da conta 7 pra
conta 3 e trava a conta 7. Agora as duas ficam esperando a conta que a
outra já travou, pra sempre (deadlock).

A solução foi sempre travar a conta de **menor índice primeiro**, não
importa se ela é a origem ou o destino:

```c
int primeiro = (origem < destino) ? origem : destino;
int segundo  = (origem < destino) ? destino : origem;

pthread_mutex_lock(&mutex_conta[primeiro]);
pthread_mutex_lock(&mutex_conta[segundo]);
```

Como todas as threads seguem essa mesma ordem, nunca existe uma esperando a
outra em círculo.

### Provando a invariante com assert

Depois de todas as threads terminarem, eu somo todos os saldos de novo e
comparo com a soma do começo:

```c
assert(soma_final == soma_inicial);
```

Na versão com trava, essa asserção **sempre passa** - rodei várias vezes e
nunca falhou, mesmo com 16 mil transferências concorrentes. Isso prova que
a exclusão mútua está protegendo o dinheiro direito: ele só troca de conta,
nunca aparece nem desaparece.

Na versão sem trava eu não coloquei `assert`, porque ali a intenção é
justamente mostrar o erro acontecendo - se fosse `assert`, o programa
abortaria de repente e eu não veria o comparativo completo. Em vez disso, só
comparo e imprimo se a soma mudou ou não.

### Como forcei a condição de corrida aparecer

Na primeira versão eu fiz a transferência direto (`saldo[origem] -= valor;
saldo[destino] += valor;`) sem trava nenhuma, e testei várias vezes... só
que a soma sempre batia! O motivo é que essas duas linhas são rápidas
demais - a chance de duas threads se cruzarem bem no meio dessas poucas
instruções é muito baixa, então a corrida quase nunca "pegava no flagra"
mesmo sem proteção nenhuma.

Pra deixar o problema visível, separei a operação em passos e coloquei uma
pausa propositalmente no meio, entre ler o saldo e escrever o resultado de
volta:

```c
long saldo_origem = saldo[origem];
long saldo_destino = saldo[destino];

usleep(1); /* pausa pequena para aumentar a chance de conflito quando estiver sem travas */

saldo_origem -= valor;
saldo_destino += valor;

saldo[origem] = saldo_origem;
saldo[destino] = saldo_destino;
```

Com essa pausa, fica bem mais fácil de duas threads lerem o mesmo saldo
antigo ao mesmo tempo e uma escrita "engolir" a outra (lost update). Na
versão com trava essa mesma pausa não causa nenhum problema - ela só deixa
a execução mais lenta, porque as outras threads ficam esperando o mutex
liberar antes de poder mexer na mesma conta.

### Resultado dos testes

Rodei o programa várias vezes seguidas. Alguns resultados:

| Execução | Soma inicial | Soma final | Diferença |
|----------|--------------|------------|-----------|
| Com trava (rodada 1) | 10000 | 10000 | 0 |
| Sem trava (rodada 1) | 10000 | 7693  | -2307 |
| Com trava (rodada 2) | 10000 | 10000 | 0 |
| Sem trava (rodada 2) | 10000 | 9836  | -164 |
| Com trava (rodada 3) | 10000 | 10000 | 0 |
| Sem trava (rodada 3) | 10000 | 5389  | -4611 |

A versão com trava nunca falhou - a soma bate sempre, exatamente. A versão
sem trava erra toda vez, e o tamanho do erro varia bastante de uma execução
pra outra (porque depende de exatamente como o sistema operacional
intercalou as threads naquela execução específica) - o que é a própria
definição de condição de corrida: um resultado que muda de forma
imprevisível dependendo do escalonamento.

### Como compilar e executar

```bash
gcc questao3.c -o questao3
./questao3
```
## Questões 4, 5 e 6 — Soluções em Python

As questões foram desenvolvidas em Python e executadas no Google Colab. 

## Questão 4 — Pipeline com três threads

### Descrição da solução

A questão 4 implementa uma linha de processamento composta por três threads:

1. **Captura:** produz os itens que serão processados;
2. **Processamento:** recebe cada item e calcula seu quadrado;
3. **Gravação:** recebe e armazena os resultados processados.

Duas instâncias de `queue.Queue(maxsize=capacidade)` conectam os três estágios e funcionam como filas limitadas. A classe `Queue` implementa internamente mecanismos equivalentes a mutex e variáveis de condição. `put()` bloqueia quando a fila está cheia e `get()` quando está vazia, sem espera ativa.

O objeto sentinela `POISON` é utilizado como protocolo de encerramento. Depois de capturar todos os itens, a primeira thread coloca a sentinela na fila. A thread de processamento a recebe, encaminha-a para a próxima fila e encerra sua execução. Por fim, a thread de gravação recebe a sentinela e também termina.

A thread principal executa `join()` nas três threads. A asserção final compara os resultados gravados aos esperados, permitindo detectar perda, duplicação ou alteração da ordem.

### Execução no Google Colab

```python
_ = executar(n=1000, capacidade=8)
```

O `_` recebe a lista retornada e evita que o Colab mostre todos os resultados na tela.

### Resultado obtido

```text
Processados 1000/1000 itens em 0.020316s; sem perdas.
```

O programa processou corretamente os 1.000 itens em aproximadamente 0,020316 segundo. Cada resultado é uma tupla no formato `(item capturado, resultado processado)`. Por exemplo, `(5, 25)` significa que o item 5 foi capturado, seu quadrado foi calculado e o valor 25 foi gravado.

A mensagem `sem perdas` confirma que todos os itens chegaram ao último estágio exatamente uma vez. O encerramento das três threads demonstra que o protocolo de poison pill funcionou sem deadlock.

## Questão 5 — Pool fixo de threads

### Descrição da solução

A questão 5 implementa um pool fixo de `N` threads para processar uma fila concorrente de tarefas CPU-bound. Cada tarefa verifica se um número inteiro é primo.

As threads trabalhadoras são criadas uma única vez. A principal coloca na fila objetos `Tarefa`, contendo um identificador sequencial e o número analisado. Cada worker retira uma tarefa e executa o teste. `queue.Queue` torna thread-safe as operações de inserção e remoção.

Depois das entradas, a principal insere uma poison pill para cada worker. `tarefas.join()` aguarda o processamento e `worker.join()` aguarda o encerramento das threads. Os identificadores confirmam que nenhuma tarefa desapareceu ou foi processada mais de uma vez. Os resultados são ordenados pelo ID original, produzindo uma saída determinística.

### Execução no Google Colab

```python
numeros = ["2", "17", "18", "7919", "1"]
resultados = executar(n_threads=4, linhas=numeros)

for _, numero, primo in resultados:
    print(f"{numero}: {'primo' if primo else 'não primo'}")
```

### Resultado obtido

```text
2: primo
17: primo
18: não primo
7919: primo
1: não primo
```

Os resultados estão corretos: 2, 17 e 7919 são primos; 18 não é primo; e 1 não é primo, pois possui apenas um divisor positivo.

Foram processadas cinco tarefas por quatro threads. A verificação dos identificadores confirmou que todas foram executadas exatamente uma vez, sem perdas ou duplicações. Embora possam terminar internamente em ordens diferentes, são apresentadas na ordem de entrada.

Por causa do GIL do CPython, threads não necessariamente aceleram cálculos CPU-bound. O objetivo desta questão é demonstrar a implementação correta do pool fixo, da fila thread-safe e do encerramento.

## Questão 6 — MapReduce paralelo

### Descrição da solução

A questão 6 lê um arquivo de inteiros e calcula a soma total e o histograma de frequências. O arquivo é dividido em blocos, e o `map` é executado com 1, 2, 4 e 8 threads.

Cada thread processa somente seu bloco e calcula localmente a soma e um `Counter`. As threads não alteram diretamente um resultado global, reduzindo a sincronização e evitando condições de corrida. Após o `map`, a thread principal realiza o `reduce`, somando os resultados e combinando os histogramas. Não existem locks explícitos durante o `map`, portanto a exclusão mútua é mínima.

Para cada número de threads, asserções comparam a soma e o histograma paralelos aos resultados sequenciais. O speedup é calculado por:

$$
S_p = \frac{T_1}{T_p}
$$

Em que $T_1$ é o tempo com uma thread e $T_p$ é o tempo com $p$ threads.

### Execução no Google Colab

```python
from pathlib import Path

arquivo = Path("inteiros.txt")
gerar_arquivo(arquivo, quantidade=1_000_000, maximo=100)
benchmark(arquivo)
```

### Resultado obtido

Foram lidos 1.000.000 de inteiros.

| Número de threads | Tempo | Speedup |
|---:|---:|---:|
| 1 | 0,092198 s | 1,000× |
| 2 | 0,072937 s | 1,264× |
| 4 | 0,076186 s | 1,210× |
| 8 | 0,076245 s | 1,209× |

A soma total encontrada foi:

```text
49.995.219
```

O melhor resultado ocorreu com duas threads: 0,072937 segundo e speedup de 1,264×, uma melhoria aproximada de 26,4% em relação a uma thread.

Quatro threads obtiveram speedup de 1,210× e oito threads, 1,209×. Aumentar o número acima de duas não trouxe ganho adicional. Isso pode decorrer do gerenciamento das threads, divisão e combinação dos blocos, GIL do CPython, núcleos disponíveis e carga do Colab.

### Histograma de frequências

Cada entrada possui o formato `valor: frequência`. Alguns exemplos obtidos foram:

```text
0: 10087
1: 9849
2: 9838
67: 10143
78: 10205
90: 9672
100: 9952
```

Como foram gerados valores de 0 a 100, existem 101 possibilidades. A frequência média esperada é:

$$
\frac{1.000.000}{101} \approx 9.901
$$

As frequências ficaram próximas desse valor. Todas as execuções produziram a mesma soma e o mesmo histograma, e as asserções terminaram sem erros. Os tempos podem variar em novas execuções conforme o ambiente do Colab.

## Garantias de correção

### Questão 4

- As filas bloqueantes eliminam a espera ativa;
- a poison pill encerra todos os estágios;
- `join()` aguarda as três threads;
- a comparação integral confirma ausência de perdas, duplicações e alterações de ordem;
- os 1.000 itens foram processados corretamente.

### Questão 5

- A fila sincronizada permite acesso concorrente seguro;
- o pool possui quantidade fixa de threads;
- uma poison pill por worker garante o encerramento;
- os identificadores comprovam que nenhuma tarefa foi perdida ou duplicada;
- os cinco números foram classificados corretamente.

### Questão 6

- Cada thread trabalha com dados locais e independentes;
- a redução é realizada pela thread principal;
- não existem locks explícitos no `map`;
- as asserções comparam os resultados paralelos aos sequenciais;
- todas as configurações produziram a soma 49.995.219 e histogramas iguais;
- duas threads obtiveram o melhor desempenho, com speedup de 1,264×.

## Questão 7 — Jantar dos filósofos com mutex

### Enunciado

Cada filósofo possui dois garfos compartilhados com seus vizinhos. Os garfos são representados por `Lock` (mutex). Foram implementadas duas soluções para evitar deadlock:

- **a. Ordem global de aquisição:** cada filósofo sempre adquire primeiro o garfo de menor número e depois o de maior número.
- **b. Semáforo:** um semáforo limita para quatro o número de filósofos que podem tentar adquirir os garfos simultaneamente.

Também são coletadas métricas individuais de cada filósofo: número de refeições e maior tempo de espera. Para reduzir starvation, o filósofo realiza tentativas em ordem controlada e usa uma pequena pausa após liberar os garfos, permitindo que outros filósofos tenham oportunidade de executar.

### Código em Python

```python
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


# ============================================================
# SOLUÇÃO A - ORDEM GLOBAL DE AQUISIÇÃO
# ============================================================

def solucao_ordem_global():

    print("\n==========================================")
    print("SOLUÇÃO A - ORDEM GLOBAL DE AQUISIÇÃO")
    print("==========================================")

    garfos = [threading.Lock() for _ in range(NUM_FILOSOFOS)]
    filosofos = [Filosofo(i) for i in range(NUM_FILOSOFOS)]

    def executar(f):

        esquerdo = f.id
        direito = (f.id + 1) % NUM_FILOSOFOS

        # Sempre adquire primeiro o garfo de menor número.
        primeiro = min(esquerdo, direito)
        segundo = max(esquerdo, direito)

        for _ in range(REFEICOES_POR_FILOSOFO):

            # Pensando
            time.sleep(random.uniform(0.01, 0.05))

            inicio_espera = time.monotonic()

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

            # Ajuda a reduzir starvation, dando oportunidade
            # para outras threads executarem.
            time.sleep(0.001)

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


# ============================================================
# SOLUÇÃO B - SEMÁFORO LIMITANDO A 4 FILÓSOFOS
# ============================================================

def solucao_semaforo():

    print("\n==========================================")
    print("SOLUÇÃO B - SEMÁFORO COM 4 FILÓSOFOS")
    print("==========================================")

    garfos = [threading.Lock() for _ in range(NUM_FILOSOFOS)]

    # No máximo quatro filósofos podem tentar
    # adquirir garfos simultaneamente.
    limite = threading.Semaphore(4)

    filosofos = [Filosofo(i) for i in range(NUM_FILOSOFOS)]

    def executar(f):

        esquerdo = f.id
        direito = (f.id + 1) % NUM_FILOSOFOS

        for _ in range(REFEICOES_POR_FILOSOFO):

            time.sleep(random.uniform(0.01, 0.05))

            inicio_espera = time.monotonic()

            limite.acquire()

            garfos[esquerdo].acquire()
            garfos[direito].acquire()

            fim_espera = time.monotonic()

            espera = fim_espera - inicio_espera

            if espera > f.maior_espera:
                f.maior_espera = espera

            time.sleep(random.uniform(0.01, 0.03))
            f.refeicoes += 1

            garfos[direito].release()
            garfos[esquerdo].release()

            limite.release()

            # Pequena pausa para reduzir a possibilidade
            # de uma mesma thread monopolizar os recursos.
            time.sleep(0.001)

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
```

### Explicação da solução A

Na primeira solução, os garfos são representados por `threading.Lock()`. O problema de deadlock é evitado estabelecendo uma **ordem global**: todos os filósofos adquirem primeiro o garfo de menor índice e depois o de maior índice.

Por exemplo, se o filósofo 0 utiliza os garfos 0 e 1, ele pega primeiro o 0. O filósofo 4 utiliza os garfos 4 e 0 e, seguindo a mesma regra, pega primeiro o 0 e depois o 4. Como todos respeitam a mesma ordem, não é possível criar um ciclo em que cada filósofo segura um recurso e espera pelo recurso seguinte.

### Explicação da solução B

Na segunda solução, foi utilizado:

```python
limite = threading.Semaphore(4)
```

Como existem cinco filósofos, o semáforo permite que no máximo quatro entrem simultaneamente na região em que tentam adquirir os garfos. Isso elimina a situação clássica em que os cinco filósofos pegam um garfo ao mesmo tempo e ficam esperando pelo segundo.

Depois que o filósofo termina de comer, ele libera os dois garfos e também uma vaga do semáforo.

### Métricas e starvation

Cada filósofo possui:

```python
self.refeicoes = 0
self.maior_espera = 0.0
```

`refeicoes` registra quantas vezes ele conseguiu comer. `maior_espera` registra o maior intervalo entre o início da tentativa de aquisição dos recursos e a obtenção dos dois garfos.

Para mitigar starvation, foi adicionada uma pequena pausa depois que os recursos são liberados:

```python
time.sleep(0.001)
```

Isso evita que uma thread tente imediatamente adquirir os mesmos recursos repetidamente e dá oportunidade de execução às demais threads. A comparação das métricas também permite verificar se algum filósofo está ficando com muito menos refeições ou uma espera muito maior que os outros.

---

## Questão 8 — Buffer com bursts, ociosidade e backpressure

### Enunciado

A questão 8 estende o problema do produtor-consumidor da Questão 2. Em vez de produzir os itens em uma taxa aproximadamente constante, os produtores trabalham em **rajadas (bursts)** e depois entram em períodos de ociosidade.

Quando os consumidores ficam mais lentos e o buffer se aproxima da capacidade máxima, é aplicado **backpressure**: os produtores precisam aguardar até que um consumidor retire itens e libere espaço.

Além disso, a ocupação do buffer é registrada ao longo do tempo para analisar a estabilidade do sistema.

A Questão 2 utiliza um buffer circular protegido por mutex e variáveis de condição. A condição `cond_nao_cheio` faz o produtor dormir quando o buffer está cheio, enquanto `cond_nao_vazio` faz o consumidor dormir quando o buffer está vazio. Essa estrutura evita espera ativa. fileciteturn0file0L94-L124

### Código em Python

```python
import threading
import time
import random
from collections import deque

NUM_PRODUTORES = 6
NUM_CONSUMIDORES = 2

ITENS_POR_PRODUTOR = 40
TAMANHO_BUFFER = 10

# Tamanho das rajadas
BURST_MIN = 2
BURST_MAX = 6

# Período de ociosidade entre rajadas
OCIOSIDADE_MIN = 0.02
OCIOSIDADE_MAX = 0.08

# Consumidores propositalmente mais lentos
CONSUMO_MIN = 0.03
CONSUMO_MAX = 0.12


# ============================================================
# BUFFER COMPARTILHADO
# ============================================================

buffer = deque()

mutex = threading.Lock()

cond_nao_vazio = threading.Condition(mutex)
cond_nao_cheio = threading.Condition(mutex)

produtores_ativos = NUM_PRODUTORES


# ============================================================
# MÉTRICAS
# ============================================================

ocupacoes = []

soma_espera = 0.0
itens_consumidos = 0

inicio_experimento = time.monotonic()


def registrar_ocupacao():
    """Registra tempo e quantidade atual de itens no buffer."""

    tempo = time.monotonic() - inicio_experimento

    ocupacoes.append(
        (tempo, len(buffer))
    )


# ============================================================
# PRODUTOR
# ============================================================

def produtor(id_produtor):

    global produtores_ativos

    random.seed(time.time() + id_produtor)

    produzidos = 0

    while produzidos < ITENS_POR_PRODUTOR:

        # ----------------------------------------------------
        # CRIA UMA RAJADA
        # ----------------------------------------------------

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

            # Produção rápida dentro do burst
            time.sleep(
                random.uniform(0.001, 0.01)
            )

            with cond_nao_cheio:

                # ------------------------------------------------
                # BACKPRESSURE
                #
                # Se o buffer estiver cheio, o produtor dorme
                # até que um consumidor libere espaço.
                # ------------------------------------------------

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
                    f"| buffer = "
                    f"{len(buffer)}/{TAMANHO_BUFFER}"
                )

                cond_nao_vazio.notify()

        # ----------------------------------------------------
        # PERÍODO DE OCIOSIDADE
        # ----------------------------------------------------

        tempo_ocioso = random.uniform(
            OCIOSIDADE_MIN,
            OCIOSIDADE_MAX
        )

        print(
            f"[Produtor {id_produtor}] "
            f"ocioso por {tempo_ocioso:.3f}s"
        )

        time.sleep(tempo_ocioso)

    # --------------------------------------------------------
    # PRODUTOR TERMINOU
    # --------------------------------------------------------

    with cond_nao_vazio:

        produtores_ativos -= 1

        if produtores_ativos == 0:
            cond_nao_vazio.notify_all()


# ============================================================
# CONSUMIDOR
# ============================================================

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

            # Todos os produtores terminaram
            # e não há mais itens.
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

            # Libera produtores bloqueados
            # pelo backpressure.
            cond_nao_cheio.notify()

        # Consumidor processa o item fora do mutex.
        tempo_consumo = random.uniform(
            CONSUMO_MIN,
            CONSUMO_MAX
        )

        time.sleep(tempo_consumo)


# ============================================================
# EXECUÇÃO DO EXPERIMENTO
# ============================================================

def executar():

    global inicio_experimento

    inicio_experimento = time.monotonic()

    produtores = []
    consumidores = []

    print("==============================================")
    print("QUESTÃO 8 - BUFFER COM BURSTS E BACKPRESSURE")
    print("==============================================")

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

    # Cria produtores
    for i in range(NUM_PRODUTORES):

        t = threading.Thread(
            target=produtor,
            args=(i,)
        )

        produtores.append(t)
        t.start()

    # Cria consumidores
    for i in range(NUM_CONSUMIDORES):

        t = threading.Thread(
            target=consumidor,
            args=(i,)
        )

        consumidores.append(t)
        t.start()

    # Espera os produtores
    for t in produtores:
        t.join()

    # Espera os consumidores
    for t in consumidores:
        t.join()

    # ========================================================
    # MÉTRICAS FINAIS
    # ========================================================

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

    print("\n==============================================")
    print("RESULTADOS")
    print("==============================================")

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
```

### Rajadas de produção

A produção é dividida em rajadas usando:

```python
tamanho_burst = random.randint(
    BURST_MIN,
    BURST_MAX
)
```

Assim, um produtor pode produzir rapidamente vários itens consecutivos. Depois da rajada, ele entra em um período de ociosidade:

```python
time.sleep(tempo_ocioso)
```

Esse comportamento representa uma aplicação em que a carga não chega de forma uniforme, mas em picos.

### Backpressure

O backpressure é implementado pela condição:

```python
while len(buffer) >= TAMANHO_BUFFER:
    cond_nao_cheio.wait()
```

Quando uma rajada faz o buffer atingir sua capacidade, o produtor não continua acumulando itens. Ele fica bloqueado até que um consumidor retire um item.

Quando isso acontece, o consumidor executa:

```python
cond_nao_cheio.notify()
```

liberando um produtor que estava aguardando.

Essa abordagem é uma extensão direta da lógica da Questão 2, que já utiliza uma variável de condição para fazer produtores aguardarem quando o buffer está cheio. fileciteturn0file0L107-L136

### Registro da ocupação

A ocupação é registrada sempre que um item entra ou sai:

```python
ocupacoes.append(
    (tempo, len(buffer))
)
```

Cada registro possui:

- o instante da medição;
- a quantidade de itens presentes no buffer.

Ao final são calculadas:

- ocupação média;
- ocupação máxima;
- tempo médio de espera;
- vazão total.

A Questão 2 já utiliza o tempo de entrada do item no buffer para calcular seu tempo de espera e utiliza a quantidade de itens processados dividida pelo tempo total para calcular a vazão. fileciteturn0file0L148-L154

### Análise da estabilidade

Se a ocupação do buffer permanece próxima da capacidade máxima durante grande parte da execução, significa que os produtores estão gerando itens mais rapidamente do que os consumidores conseguem processar.

Por exemplo:

```text
2 → 5 → 8 → 10 → 10 → 9 → 10 → 10
```

indica que o sistema está frequentemente saturado. Nesse caso, o backpressure entra em ação várias vezes.

Por outro lado, uma ocupação oscilando entre valores baixos e altos, como:

```text
2 → 6 → 9 → 5 → 2 → 7 → 10 → 4
```

indica que o sistema está alternando entre rajadas de produção e períodos de consumo.

A vazão continua limitada principalmente pela capacidade dos consumidores. Na Questão 2, os consumidores foram propositalmente configurados como mais lentos que os produtores, fazendo com que eles se tornassem o gargalo do sistema. fileciteturn0file0L161-L187

### Conclusão da Questão 8

A implementação demonstra que o backpressure impede que produtores continuem aumentando a fila indefinidamente quando a taxa de consumo diminui. Em vez disso, eles aguardam espaço no buffer. O registro da ocupação permite observar os momentos de saturação e de ociosidade e verificar se o sistema consegue retornar a níveis menores de ocupação após os bursts. Dessa forma, é possível analisar experimentalmente a estabilidade do produtor-consumidor sob uma carga variável.
