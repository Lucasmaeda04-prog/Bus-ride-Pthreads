// para compilar: gcc prodcons_n_threads_sem.c -o prodcons_n_threads_sem -lpthread
// para executar: prodcons_n_threads_sem

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h> 
#include <unistd.h>
#include <sys/types.h>
#include <time.h> 

sem_t onibus_pessoas; 
int S,C,A,P;
int total_pessoas; 

typedef struct {
    int num_pessoas;
    int id; 
    sem_t pessoas; // mutex para alterar o número de pessoas dentro.
    pthread_mutex_t mutex; // mutex para impedir que haja mais de um onibus em um mesmo ponto  
}t_PontoOnibus; 

typedef struct{
    int num_pessoas; // espaço ocupado no buffer
    int assentos; // numero total de espaço no buffer
    int id;
    t_PontoOnibus *pontos; // Adicionando um ponteiro para a array de pontos de ônibus 
    int partida; 
}t_Onibus;

typedef struct{
    int id; 
    int flag;
    int ponto_origem; 
    int ponto_saida;
    time_t tempo_comeco; 
    time_t tempo_fim; 
    t_Onibus Onibus;
}t_Passageiros;

// ----------------------------------------------------------------------------------------

void *pontoOnibus(void *arg){
    t_PontoOnibus *ponto = (t_PontoOnibus *)arg; 
    printf("Thread do Ponto:(%d) iniciada e com %d passageiros esperando\n",ponto->id,ponto->num_pessoas);
    pthread_exit(0); 
}

void *onibus(void *arg){
    int stay = 1; 
    t_Onibus *onibus = (t_Onibus *)arg; 
    printf("Thread do Onibus:(%d) iniciada\n",onibus->id);
    do {
        if(pthread_mutex_trylock(&onibus->pontos[(onibus->partida)%S].mutex)==0 ){
            sem_wait(&onibus_pessoas);
            printf("Onibus %d está no ponto %d com %d pessoas total de pessoas:%d\n",onibus->id,(onibus->partida)%S,onibus->num_pessoas,total_pessoas);
            if(total_pessoas<=0){
                stay=0;
            }
            total_pessoas--;
            onibus->num_pessoas--;
            sem_post(&onibus_pessoas);
            pthread_mutex_unlock(&onibus->pontos[(onibus->partida)%S].mutex);
        }
        onibus->partida++; 
    } while (stay);
    printf("Onibus %d saindo\n\n",onibus->id);
    pthread_exit(0); 
}

void *passageiro(void *arg){
    t_Passageiros *passageiro = (t_Passageiros *)arg; 
    passageiro->tempo_fim = time(NULL);
    passageiro->tempo_fim = time( NULL);
    pthread_exit(0);
}
// ----------------------------------------------------------------------------------------
 void main(int argc, char *argv[]){
    // usando valores pré-definido
    S = 10; // atoi(argv[1]);
    C = 5; // atoi(argv[2]);
    P = 100; // atoi(argv[3]);
    A = 0; // atoi(argv[4]);
    total_pessoas = P; 
    sem_init(&onibus_pessoas,0,1); 
    pid_t pid = fork();
    // PROCESSO ONIBUS
    if (pid == 0) {// Processo filho - Processo 'ônibus'
        
        // DECLARANDO THREADS E AS ESTRUTURAS DE DADOS
        t_Passageiros conjunto_passageiro[P];
        pthread_t Passageiro_h[P];
        t_PontoOnibus conjunto_pontos[S];
        pthread_t PontoOnibus_h[S];  
        t_Onibus conjunto_onibus[C];
        pthread_t Onibus_h[C];
        
        // INICIANDO PONTO DE ONIBUS ------------------------------------------------------
        int tmp_start; 
        int tmp_end;
        for(int i=0;i<S;i++){
            conjunto_pontos[i].id = i; 
            sem_init(&conjunto_pontos[i].pessoas,0,1);
            pthread_mutex_init(&conjunto_pontos[i].mutex,NULL);
        }
        // INICIANDO ONIBUS ---------------------------------------------------------------
        for(int i=0;i<C;i++){
            conjunto_onibus[i].id = i;
            conjunto_onibus[i].pontos = conjunto_pontos;
            conjunto_onibus[i].num_pessoas = 10; // APENAS PARA TESTE 
            conjunto_onibus[i].partida = rand()%S; 
        }
        // INICIANDO PASSAGEIRO -----------------------------------------------------------
        for(int i=0;i<P;i++){
            conjunto_passageiro[i].flag = 0;
            conjunto_passageiro[i].id = i;
            tmp_start = rand()%S;
            tmp_end = rand()%S;
            conjunto_passageiro[i].ponto_origem = tmp_start;
            conjunto_passageiro[i].ponto_saida = tmp_end;
            conjunto_pontos[tmp_start].num_pessoas++; 
        }

        // -----------------------RUNNING THREADS -----------------------------------------
        for(int i=0;i<S;i++){
            if (pthread_create(&PontoOnibus_h[i],0,pontoOnibus,(void *)&conjunto_pontos[i])!=0){
                printf("Falha ao criar o ponto de ônibus");
                fflush(0);
                exit(0);
            };
        }
         for(int i=0;i<C;i++){
            if (pthread_create(&Onibus_h[i],0,onibus,(void *)&conjunto_onibus[i])!=0){
                printf("Falha ao criar o ônibus");
                fflush(0);
                exit(0);
            }; 
        }
         for(int i=0;i<P;i++){
            if (pthread_create(&Passageiro_h[i],0,passageiro,(void *)&conjunto_passageiro[i])!=0){
                printf("Falha ao criar o passageiro");
                fflush(0);
                exit(0);
            }; 
        }
        for (int i=0; i < S; i++) {
		    pthread_join(PontoOnibus_h[ i ], 0);
        }
        for (int i=0; i < C; i++) {
            pthread_join(Onibus_h[ i ], 0);
        }
        for (int i=0; i < P; i++) {
            pthread_join(Passageiro_h[ i ], 0);
        }

    }else if (pid > 0) {
        // Processo pai
        wait(NULL); // Esperar o processo filho terminar

    }else {
        // Erro ao criar o processo
        perror("fork");
        return -1;
    }
    exit(0); 
}