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
