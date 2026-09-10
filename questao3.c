/*
 * Questao 3 - Transferencias bancarias com M contas e T threads
 *
 * Como funciona o programa:
 *  - Temos M contas bancarias e T threads fazendo transferencias ao mesmo tempo
 *  - Usamos um mutex para cada conta para que apenas a conta envolvida fique travada
 *  - Comprovamos via asserção (assert) que a soma total do dinheiro nunca muda
 *  - O programa faz o teste duas vezes: uma com travas (correto) e outra sem travas
 *    para mostrar que o dinheiro "some" ou "aparece" quando ocorre conflito entre threads
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>

#define M 10 /* Numero total de contas bancarias */
#define T 8  /* Numero de threads rodando ao mesmo tempo */
#define TRANSFERENCIAS_POR_THREAD 2000
#define SALDO_INICIAL 1000         /* Cada conta comeca com 1000 reais */
#define VALOR_MAX_TRANSFERENCIA 50 /* Valor maximo de cada transferencia */

/* Guardamos os saldos usando "long" para evitar problemas de limite numérico */
long saldo[M];

/* Um mutex para cada conta: permite que transferencias em contas diferentes
   acontecam ao mesmo tempo sem travar o sistema inteiro */
pthread_mutex_t mutex_conta[M];

typedef struct
{
    int id;
    int usar_trava; /* 1 usa mutex (correto), 0 nao usa (para demonstrar o erro) */
} ArgThread;

/* Faz a transferencia entre duas contas */
void transferir(int origem, int destino, long valor, int usar_trava)
{
    if (usar_trava)
    {
        /* Para evitar um travamento geral (deadlock), sempre travamos
           a conta de menor indice primeiro, independentemente de ser origem ou destino */
        int primeiro = (origem < destino) ? origem : destino;
        int segundo = (origem < destino) ? destino : origem;

        pthread_mutex_lock(&mutex_conta[primeiro]);
        pthread_mutex_lock(&mutex_conta[segundo]);
    }

    /* Le os valores das contas envolvidas */
    long saldo_origem = saldo[origem];
    long saldo_destino = saldo[destino];

    /* Pausa pequena para aumentar a chance de conflito quando estiver sem travas */
    usleep(1);

    /* Atualiza os saldos */
    saldo_origem -= valor;
    saldo_destino += valor;

    saldo[origem] = saldo_origem;
    saldo[destino] = saldo_destino;

    if (usar_trava)
    {
        int primeiro = (origem < destino) ? origem : destino;
        int segundo = (origem < destino) ? destino : origem;
        pthread_mutex_unlock(&mutex_conta[segundo]);
        pthread_mutex_unlock(&mutex_conta[primeiro]);
    }
}

/* Funcao que cada thread executa para realizar varias transferencias aleatorias */
void *trabalhador(void *arg)
{
    ArgThread *a = (ArgThread *)arg;
    unsigned int seed = (unsigned int)time(NULL) ^ (a->id * 2654435761u) ^ (unsigned int)pthread_self();

    for (int i = 0; i < TRANSFERENCIAS_POR_THREAD; i++)
    {
        int origem = (int)(rand_r(&seed) % M);
        int destino = (int)(rand_r(&seed) % M);

        /* Garante que a conta de destino seja diferente da origem */
        while (destino == origem)
        {
            destino = (int)(rand_r(&seed) % M);
        }
        long valor = (rand_r(&seed) % VALOR_MAX_TRANSFERENCIA) + 1;

        transferir(origem, destino, valor, a->usar_trava);
    }

    return NULL;
}

/* Soma o saldo de todas as contas para conferir se o total de dinheiro continua o mesmo */
long soma_saldos(void)
{
    long soma = 0;
    for (int i = 0; i < M; i++)
    {
        soma += saldo[i];
    }
    return soma;
}

/* Executa a simulacao completa (com ou sem trava) e exibe os resultados */
void rodar_experimento(int usar_trava)
{
    /* Reinicia o saldo de todas as contas e inicializa os mutexes */
    for (int i = 0; i < M; i++)
    {
        saldo[i] = SALDO_INICIAL;
        pthread_mutex_init(&mutex_conta[i], NULL);
    }

    long soma_inicial = soma_saldos();

    pthread_t threads[T];
    ArgThread args[T];

    /* Cria as threads */
    for (int i = 0; i < T; i++)
    {
        args[i].id = i;
        args[i].usar_trava = usar_trava;
        pthread_create(&threads[i], NULL, trabalhador, &args[i]);
    }

    /* Espera todas as threads terminarem */
    for (int i = 0; i < T; i++)
    {
        pthread_join(threads[i], NULL);
    }

    long soma_final = soma_saldos();

    printf("%s:\n", usar_trava ? "COM trava (versao correta)" : "SEM trava (versao incorreta, apenas demonstracao)");
    printf("  soma inicial = %ld\n", soma_inicial);
    printf("  soma final   = %ld\n", soma_final);
    printf("  diferenca    = %ld\n", soma_final - soma_inicial);

    if (usar_trava)
    {
        /* O assert garante que o programa fecha com erro se o valor final for diferente.
           Se ele passar, fica provado que nenhum dinheiro sumiu ou apareceu */
        assert(soma_final == soma_inicial);
        printf("  assert(soma_final == soma_inicial) passou -> Dinheiro totalmente preservado!\n");
    }
    else
    {
        if (soma_final != soma_inicial)
        {
            printf("  -> ERRO DE CONCORRENCIA CONFIRMADO: O saldo mudou porque duas threads\n");
            printf("     tentaram alterar a mesma conta ao mesmo tempo sem protecao.\n");
        }
        else
        {
            printf("  -> A soma bateu por coincidencia nesta execucao. Rode novamente\n");
            printf("     para observar a falha ocorrer.\n");
        }
    }

    /* Libera a memoria dos mutexes */
    for (int i = 0; i < M; i++)
    {
        pthread_mutex_destroy(&mutex_conta[i]);
    }
}

int main(void)
{
    printf("=== SIMULACAO DE TRANSFERENCIAS BANCARIAS ===\n");
    printf("%d contas, %d threads, %d transferencias por thread (%d no total)\n\n",
           M, T, TRANSFERENCIAS_POR_THREAD, T * TRANSFERENCIAS_POR_THREAD);

    printf("--- Execucao 1: COM trava (correta) ---\n");
    rodar_experimento(1);

    printf("\n--- Execucao 2: SEM trava (incorreta, para comparacao) ---\n");
    rodar_experimento(0);

    return 0;
}