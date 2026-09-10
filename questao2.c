/*
 * Questao 2 - Buffer circular com produtores e consumidores (pthreads)
 *
 * Como funciona o programa:
 *  - Um grupo de threads cria itens (produtores) e outro grupo retira itens (consumidores)
 *  - Todos usam um buffer circular compartilhado de tamanho N
 *  - Usamos mutex e variaveis de condicao para que as threads "durmam" enquanto esperam
 *    (evita gasto inutil de CPU enquanto o buffer esta cheio ou vazio)
 *  - O programa mede o tempo que cada item fica esperando no buffer e a velocidade total
 *  - No final, testamos varios tamanhos de buffer para comparar a diferenca de desempenho
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

#define NUM_PRODUTORES 6
#define NUM_CONSUMIDORES 2
#define ITENS_POR_PRODUTOR 25
#define TOTAL_ITENS (NUM_PRODUTORES * ITENS_POR_PRODUTOR)
#define ESPERA_PRODUCAO_MAX_MS 3 /* Produtores sao mais rapidos para fabricar itens */
#define ESPERA_CONSUMO_MAX_MS 15 /* Consumidores sao mais lentos de proposito, para formar fila */

/* Estrutura do item: guarda o ID e a hora exata em que foi criado */
typedef struct
{
    int id;
    struct timespec criado_em;
} Item;

/* Dados enviados para identificar cada thread produtora */
typedef struct
{
    int id;
    int itens_a_produzir;
} ArgProdutor;

/* Estrutura para guardar o resultado do teste de cada tamanho de buffer */
typedef struct
{
    int tamanho_buffer;
    double vazao_itens_seg;
    double espera_media_ms;
} Resultado;

/* ---- Variaveis do buffer circular (compartilhadas por todas as threads) ---- */
Item *buffer;              /* Vetor do buffer alocado dinamicamente */
int tamanho_buffer;        /* Capacidade N do buffer no teste atual */
int inicio, fim, contagem; /* Indices para controlar o buffer circular */
int produtores_ativos;     /* Contador de quantos produtores ainda estao trabalhando */

pthread_mutex_t mutex_buffer;  /* Trava de seguranca para alterar o buffer sem dar conflito */
pthread_cond_t cond_nao_vazio; /* Avisa os consumidores quando entra um novo item */
pthread_cond_t cond_nao_cheio; /* Avisa os produtores quando abre espaco no buffer */

/* ---- Estatisticas do teste ---- */
double soma_espera_ms;
int itens_consumidos;

/* Funcao auxiliar para calcular a diferenca de tempo em milissegundos */
double diff_ms(struct timespec inicio_t, struct timespec fim_t)
{
    return (fim_t.tv_sec - inicio_t.tv_sec) * 1000.0 + (fim_t.tv_nsec - inicio_t.tv_nsec) / 1e6;
}

/* -------------------- Thread Produtora -------------------- */
void *produtor(void *arg)
{
    ArgProdutor *a = (ArgProdutor *)arg;
    unsigned int seed = (unsigned int)time(NULL) ^ (a->id * 104729u) ^ (unsigned int)pthread_self();

    for (int i = 0; i < a->itens_a_produzir; i++)
    {
        /* Simula o tempo gasto para fabricar o item */
        int tempo_ms = (rand_r(&seed) % ESPERA_PRODUCAO_MAX_MS) + 1;
        usleep(tempo_ms * 1000);

        pthread_mutex_lock(&mutex_buffer);

        /* Se o buffer estiver cheio, a thread dorme ate um consumidor tirar um item */
        while (contagem == tamanho_buffer)
        {
            pthread_cond_wait(&cond_nao_cheio, &mutex_buffer);
        }

        Item novo;
        novo.id = a->id * 1000 + i;
        clock_gettime(CLOCK_MONOTONIC, &novo.criado_em); /* Marca o momento de criacao */

        buffer[fim] = novo;
        fim = (fim + 1) % tamanho_buffer;
        contagem++;

        pthread_cond_signal(&cond_nao_vazio); /* Acorda um consumidor que esteja esperando */
        pthread_mutex_unlock(&mutex_buffer);
    }

    /* Atualiza o contador de produtores ativos ao encerrar o trabalho */
    pthread_mutex_lock(&mutex_buffer);
    produtores_ativos--;
    if (produtores_ativos == 0)
    {
        /* Se for o ultimo produtor, acorda todos os consumidores para encerrarem tambem */
        pthread_cond_broadcast(&cond_nao_vazio);
    }
    pthread_mutex_unlock(&mutex_buffer);

    free(a);
    return NULL;
}

/* -------------------- Thread Consumidora -------------------- */
void *consumidor(void *arg)
{
    int id = *(int *)arg;
    free(arg);

    unsigned int seed = (unsigned int)time(NULL) ^ (id * 65599u) ^ (unsigned int)pthread_self();

    while (1)
    {
        pthread_mutex_lock(&mutex_buffer);

        /* Dorme enquanto o buffer estiver vazio e ainda houver produtores ativos */
        while (contagem == 0 && produtores_ativos > 0)
        {
            pthread_cond_wait(&cond_nao_vazio, &mutex_buffer);
        }

        /* Se o buffer esvaziou e nenhum produtor vai fabricar mais nada, encerra */
        if (contagem == 0 && produtores_ativos == 0)
        {
            pthread_mutex_unlock(&mutex_buffer);
            break;
        }

        Item retirado = buffer[inicio];
        inicio = (inicio + 1) % tamanho_buffer;
        contagem--;

        /* Calcula quanto tempo o item ficou guardado no buffer */
        struct timespec agora;
        clock_gettime(CLOCK_MONOTONIC, &agora);
        double espera = diff_ms(retirado.criado_em, agora);
        soma_espera_ms += espera;
        itens_consumidos++;

        pthread_cond_signal(&cond_nao_cheio); /* Acorda um produtor que esteja esperando espaço */
        pthread_mutex_unlock(&mutex_buffer);

        /* Simula o tempo gasto para processar o item (fora do mutex para nao travar os outros) */
        int tempo_ms = (rand_r(&seed) % ESPERA_CONSUMO_MAX_MS) + 1;
        usleep(tempo_ms * 1000);
    }
    return NULL;
}

/* Executa um teste completo com um buffer de tamanho N e retorna os dados coletados */
Resultado rodar_experimento(int N)
{
    tamanho_buffer = N;
    buffer = malloc(sizeof(Item) * (size_t)N);
    inicio = 0;
    fim = 0;
    contagem = 0;
    produtores_ativos = NUM_PRODUTORES;
    soma_espera_ms = 0.0;
    itens_consumidos = 0;

    pthread_mutex_init(&mutex_buffer, NULL);
    pthread_cond_init(&cond_nao_vazio, NULL);
    pthread_cond_init(&cond_nao_cheio, NULL);

    pthread_t produtores[NUM_PRODUTORES];
    pthread_t consumidores[NUM_CONSUMIDORES];

    struct timespec t_inicio, t_fim;
    clock_gettime(CLOCK_MONOTONIC, &t_inicio);

    /* Cria as threads produtoras */
    for (int i = 0; i < NUM_PRODUTORES; i++)
    {
        ArgProdutor *a = malloc(sizeof(ArgProdutor));
        a->id = i;
        a->itens_a_produzir = ITENS_POR_PRODUTOR;
        pthread_create(&produtores[i], NULL, produtor, a);
    }

    /* Cria as threads consumidoras */
    for (int i = 0; i < NUM_CONSUMIDORES; i++)
    {
        int *id = malloc(sizeof(int));
        *id = i;
        pthread_create(&consumidores[i], NULL, consumidor, id);
    }

    /* Espera todas as threads terminarem */
    for (int i = 0; i < NUM_PRODUTORES; i++)
    {
        pthread_join(produtores[i], NULL);
    }
    for (int i = 0; i < NUM_CONSUMIDORES; i++)
    {
        pthread_join(consumidores[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &t_fim);
    double tempo_total_seg = diff_ms(t_inicio, t_fim) / 1000.0;

    Resultado r;
    r.tamanho_buffer = N;
    r.vazao_itens_seg = itens_consumidos / tempo_total_seg;
    r.espera_media_ms = soma_espera_ms / itens_consumidos;

    /* Libera os recursos e a memoria utilizada no teste */
    pthread_mutex_destroy(&mutex_buffer);
    pthread_cond_destroy(&cond_nao_vazio);
    pthread_cond_destroy(&cond_nao_cheio);
    free(buffer);

    return r;
}

int main(void)
{
    /* Lista com os tamanhos de buffer que serao testados */
    int tamanhos[] = {1, 2, 4, 8, 16, 32};
    int qtd_tamanhos = (int)(sizeof(tamanhos) / sizeof(tamanhos[0]));

    printf("=== BUFFER CIRCULAR - PRODUTORES E CONSUMIDORES ===\n\n");
    printf("%d produtores, %d consumidores, %d itens por produtor (%d itens no total)\n\n",
           NUM_PRODUTORES, NUM_CONSUMIDORES, ITENS_POR_PRODUTOR, TOTAL_ITENS);

    Resultado resultados[qtd_tamanhos];

    /* Executa a simulacao para cada tamanho de buffer */
    for (int i = 0; i < qtd_tamanhos; i++)
    {
        printf("Rodando com buffer de tamanho %d...\n", tamanhos[i]);
        resultados[i] = rodar_experimento(tamanhos[i]);
    }

    /* Imprime a tabela comparativa final */
    printf("\n%-12s | %-18s | %-20s\n", "Tamanho N", "Vazao (itens/s)", "Espera media (ms)");
    printf("-------------|--------------------|----------------------\n");
    for (int i = 0; i < qtd_tamanhos; i++)
    {
        printf("%-12d | %-18.2f | %-20.2f\n",
               resultados[i].tamanho_buffer,
               resultados[i].vazao_itens_seg,
               resultados[i].espera_media_ms);
    }

    return 0;
}