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
