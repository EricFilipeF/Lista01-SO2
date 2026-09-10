/*
 * Questao 1 - Corrida de cavalos usando threads (pthreads)
 *
 * Como funciona o programa:
 *  - Cada cavalo eh uma thread diferente correndo ao mesmo tempo
 *  - O usuario escolhe um cavalo antes da corrida comecar
 *  - Usamos barreiras pra garantir que todos os cavalos larguem juntos
 *  - Usamos um mutex pra evitar erros ao atualizar o placar
 *  - No final, mostramos quem venceu e se a aposta deu certo
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

#define NUM_CAVALOS 5
#define DISTANCIA 100        /* Tamanho da pista (100 passos ate a chegada) */
#define PASSO_MAX 6          /* O cavalo anda no maximo 6 passos por vez */
#define PISTA_LARGURA 40     /* Quantos caracteres a pista ocupa na tela */
#define ATRASO_RODADA 200000 /* Pausa de 0.2 segundos pra dar pra ver a animacao */

/* Nomes dos cavalos pra mostrar no terminal */
const char *nomes[NUM_CAVALOS] = {
    "Relampago", "Trovao", "Furacao", "Foguete", "Vento"};

/* Cores ANSI para cada cavalo ter uma cor diferente */
const char *cavalo = "\U0001F40E";
const char *cor[NUM_CAVALOS] = {
    "\033[31m", /* Vermelho */
    "\033[32m", /* Verde    */
    "\033[33m", /* Amarelo  */
    "\033[34m", /* Azul     */
    "\033[35m"  /* Magenta  */
};
const char *cor_reset = "\033[0m";

/* ---- Variaveis compartilhadas por todas as threads ---- */
int posicao[NUM_CAVALOS]; /* Guarda em qual passo cada cavalo esta */
int vencedor = -1;        /* Comeca em -1 porque ninguem venceu ainda */
int corrida_terminou = 0; /* Avisa quando a corrida chega ao fim */

/* Ferramentas do pthread para sincronizar o programa */
pthread_mutex_t mutex_placar;        /* Trava de seguranca para alterar os dados sem dar erro */
pthread_barrier_t barreira_largada;  /* Segura todos na largada */
pthread_barrier_t barreira_rodada;   /* Espera todos terminarem a rodada */
pthread_barrier_t barreira_checagem; /* Espera a conferencia do resultado */

/* Desenha a pista de corrida na tela a cada rodada */
void imprimir_placar(int rodada)
{
    /* Limpa a tela do terminal antes de desenhar a nova rodada */
    printf("\033[H\033[J");

    printf("=== CORRIDA DE CAVALOS - Rodada %d ===\n\n", rodada);

    for (int i = 0; i < NUM_CAVALOS; i++)
    {
        int pos = posicao[i] > DISTANCIA ? DISTANCIA : posicao[i];

        /* Converte a posicao (0 a 100) para o tamanho da tela (0 a 40) */
        int coluna = (pos * PISTA_LARGURA) / DISTANCIA;

        printf("%s%-10s%s |", cor[i], nomes[i], cor_reset);
        for (int col = 0; col < PISTA_LARGURA; col++)
        {
            if (pos >= DISTANCIA && col == PISTA_LARGURA - 1)
            {
                printf("%s%s%s", cor[i], cavalo, cor_reset); /* Cavalo na linha de chegada */
            }
            else if (col == coluna)
            {
                printf("%s%s%s", cor[i], cavalo, cor_reset); /* Posição atual do cavalo */
            }
            else if (col < coluna)
            {
                putchar('-'); /* Caminho ja percorrido */
            }
            else
            {
                putchar('.'); /* Caminho que falta percorrer */
            }
        }
        printf("| %3d/%d", pos, DISTANCIA);
        if (pos >= DISTANCIA)
        {
            printf("  CHEGOU!");
        }
        printf("\n");
    }
    printf("\n");
    fflush(stdout); /* Forca a atualizacao imediata da tela */
}

/* Funcao que cada thread de cavalo executa */
void *corredor(void *arg)
{
    int id = *(int *)arg;

    /* Semente para gerar numeros aleatorios proprios para cada thread */
    unsigned int seed = (unsigned int)time(NULL) ^ (id * 7919u) ^ (unsigned int)pthread_self();

    /* Espera todas as threads estarem prontas para dar a largada juntas */
    pthread_barrier_wait(&barreira_largada);

    if (id == 0)
    {
        printf("\n=== A corrida comecou! ===\n\n");
    }

    int rodada = 0;
    while (1)
    {
        /* Se a corrida acabou na rodada anterior, encerra o loop */
        if (corrida_terminou)
        {
            break;
        }

        rodada++;
        int passo = (rand_r(&seed) % PASSO_MAX) + 1;

        /* Usa o mutex para atualizar a posicao sem conflito entre as threads */
        pthread_mutex_lock(&mutex_placar);
        posicao[id] += passo;
        if (posicao[id] > DISTANCIA)
        {
            posicao[id] = DISTANCIA; /* Nao deixa passar de 100 */
        }
        pthread_mutex_unlock(&mutex_placar);

        /* Espera todos os cavalos andarem antes de checar quem ganhou */
        pthread_barrier_wait(&barreira_rodada);

        /* Apenas o cavalo 0 desenha o placar e confere o vencedor */
        if (id == 0)
        {
            pthread_mutex_lock(&mutex_placar);
            imprimir_placar(rodada);

            /* Procura quem cruzou a linha de chegada primeiro */
            for (int i = 0; i < NUM_CAVALOS; i++)
            {
                if (posicao[i] >= DISTANCIA && vencedor == -1)
                {
                    vencedor = i;
                }
            }

            if (vencedor != -1)
            {
                corrida_terminou = 1;
            }
            pthread_mutex_unlock(&mutex_placar);

            /* Pausa para dar tempo do usuario ver a animacao */
            usleep(ATRASO_RODADA);
        }

        /* Espera a verificacao do cavalo 0 terminar antes de ir pra proxima rodada */
        pthread_barrier_wait(&barreira_checagem);

        if (corrida_terminou)
        {
            break;
        }
    }

    return NULL;
}

int main(void)
{
    pthread_t threads[NUM_CAVALOS];
    int ids[NUM_CAVALOS];
    int aposta;

    srand((unsigned int)time(NULL));

    printf("=== CORRIDA DE CAVALOS ===\n\n");
    printf("Cavalos disponiveis:\n");
    for (int i = 0; i < NUM_CAVALOS; i++)
    {
        printf("  %d - %s\n", i + 1, nomes[i]);
    }

    /* Pede a aposta do usuario e valida se o numero eh valido */
    do
    {
        printf("\nEm qual cavalo voce aposta? (1 a %d): ", NUM_CAVALOS);
        if (scanf("%d", &aposta) != 1)
        {
            /* Limpa letras ou caracteres invalidos que o usuario digitar */
            while (getchar() != '\n')
                ;
            aposta = -1;
        }
    } while (aposta < 1 || aposta > NUM_CAVALOS);
    aposta--; /* Ajusta de (1 a 5) para o indice do vetor (0 a 4) */

    printf("\nVoce apostou no %s. Boa sorte!\n", nomes[aposta]);

    /* Inicializa o mutex e as barreiras */
    pthread_mutex_init(&mutex_placar, NULL);
    pthread_barrier_init(&barreira_largada, NULL, NUM_CAVALOS);
    pthread_barrier_init(&barreira_rodada, NULL, NUM_CAVALOS);
    pthread_barrier_init(&barreira_checagem, NULL, NUM_CAVALOS);

    for (int i = 0; i < NUM_CAVALOS; i++)
    {
        posicao[i] = 0;
        ids[i] = i;
    }

    /* Cria uma thread para cada cavalo */
    for (int i = 0; i < NUM_CAVALOS; i++)
    {
        pthread_create(&threads[i], NULL, corredor, &ids[i]);
    }

    /* Espera todas as threads terminarem a corrida */
    for (int i = 0; i < NUM_CAVALOS; i++)
    {
        pthread_join(threads[i], NULL);
    }

    printf("=== FIM DA CORRIDA ===\n");
    printf("O vencedor foi: %s!\n", nomes[vencedor]);

    if (vencedor == aposta)
    {
        printf("Parabens, sua aposta em %s estava CORRETA! Voce ganhou.\n", nomes[aposta]);
    }
    else
    {
        printf("Que pena, voce apostou em %s e o vencedor foi %s. Voce perdeu.\n",
               nomes[aposta], nomes[vencedor]);
    }

    /* Libera a memoria das ferramentas de sincronizacao */
    pthread_mutex_destroy(&mutex_placar);
    pthread_barrier_destroy(&barreira_largada);
    pthread_barrier_destroy(&barreira_rodada);
    pthread_barrier_destroy(&barreira_checagem);

    return 0;
}