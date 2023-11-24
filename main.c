// para compilar: gcc prodcons_n_threads_sem.c -o prodcons_n_threads_sem -lpthread
// para executar: prodcons_n_threads_sem

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h> 
#include <unistd.h>
#include <sys/types.h>
#include <time.h> 

#define S 5
#define C 1
#define A 10
#define P 30
sem_t pessoas; 
sem_t global_mutex; 
int total_pessoas; 
int stay; 



typedef struct t_Onibus t_Onibus;
typedef struct t_Passageiros t_Passageiros;

typedef struct t_PontoOnibus{
    int num_pessoas; 
    int id; 
    pthread_mutex_t mutex; // mutex para impedir que haja mais de um onibus em um mesmo ponto.  
    pthread_mutex_t mutex_passageiro; // mutex para controlar acesso dos passageiros 
    pthread_mutex_t mutex_ob; // mutex para priorizar a entrada do onibus no ponto. 
    t_Onibus *onibus; // ponteiro que indica qual ônibus está esperando no ponto 
    int primeiro_passageiros; // ponteiro para o primeiro passageiro da fila
    // pthread_mutex_t mutex_busAcesso; // mutex para que o ponto apenas fique acordado quando o onibus estiver no ponto
}t_PontoOnibus; 

struct t_Onibus{
    int num_pessoas; // espaço ocupado no buffer
    int assentos; // numero total de espaço no buffer
    int id; 
    t_PontoOnibus *pontos; // um ponteiro para a array de pontos de ônibus 
    int partida;  // ponto de partida para o ônibus
    pthread_mutex_t mutex_passageiros; 
    pthread_mutex_t sleep_passageiros;
    pthread_mutex_t subida_passageiro;
    pthread_mutex_t sleep_onibus;
    pthread_mutex_t sleep_onibus2;
    sem_t desce_passageiros; 
};

struct t_Passageiros{     
    int id; 
    int flag;
    int ponto_origem; 
    int ponto_saida;
    time_t tempo_comeco; 
    time_t tempo_fim; 
    t_Onibus *Onibus;
    int prox; 
    t_Passageiros *proximo; 
    pthread_mutex_t mutex_embarque; 
};
t_PontoOnibus conjunto_pontos[S];

// ----------------------------------------------------------------------------------------
/*
void *pontoOnibus(void *arg){
    
    t_PontoOnibus *ponto = (t_PontoOnibus *)arg; 
    printf("Thread do Ponto:(%d) iniciada e com %d passageiros esperando\n, vou mimir até chegar um ônibus",ponto->id,ponto->num_pessoas);
    do{
        pthread_mutex_lock(&ponto->mutex_busAcesso);
        embarquePassageiro(&ponto);
    } while(stay);
    
    pthread_exit(0); 
}
*/


void *onibus(void *arg){
    t_Onibus *onibus = (t_Onibus *)arg; 
    printf("Thread do Onibus:(%d) iniciada\n",onibus->id);
    do {
        if(pthread_mutex_trylock(&onibus->pontos[(onibus->partida)%S].mutex)==0 ){ // verificando se já há um ônibus no ponto 
            printf("Onibus %d está no ponto %d com %d pessoas \n",onibus->id,(onibus->partida)%S,onibus->num_pessoas);
            if(total_pessoas<=0){
                stay=0;
            }
            total_pessoas--;
            onibus->num_pessoas--;
            pthread_mutex_unlock(&onibus->pontos[(onibus->partida)%S].mutex); // liberando mutex que dá acesso ao ponto de ônibus
        }
        onibus->partida++; 
    } while (stay);
    printf("Onibus %d saindo\n\n",onibus->id);
    pthread_exit(0); 
}

void *passageiro(void *arg){
    int stay=1;
    t_Passageiros *passageiro = (t_Passageiros *)arg; 
    int tmp_start = rand()%S;
    int tmp_end = rand()%S;
    passageiro->ponto_origem = tmp_start;
    passageiro->ponto_saida = tmp_end; 
    // entrada no ônibus   
    pthread_mutex_lock(&conjunto_pontos[tmp_start].mutex_passageiro);
    pthread_mutex_lock(&conjunto_pontos[tmp_start].mutex_ob);
    adicionarPassageiroPonto(passageiro,&conjunto_pontos[tmp_start],passageiro->id);
    pthread_mutex_unlock(&conjunto_pontos[tmp_start].mutex_ob);
    pthread_mutex_unlock(&conjunto_pontos[tmp_start].mutex_passageiro);
    // espera subida pelo ônibus 
    passageiro->tempo_comeco = time(NULL);
    passageiro->tempo_fim = time( NULL);
    do {
        pthread_mutex_lock(&passageiro->mutex_embarque);
        if((conjunto_pontos[tmp_start].onibus->num_pessoas-conjunto_pontos[tmp_start].onibus->assentos)==0){
            
        }
    }while(stay);
    // 

    pthread_exit(0);
}


void adicionarPassageiroPonto(t_PontoOnibus *Ponto, t_Passageiros *Passageiros, int id){
    t_PontoOnibus *ponto =  Ponto; 
    t_Passageiros *conjunto_passageiro = Passageiros;
    printf("Ponto aqui foi recebido normalmente: %d\n",ponto->id);
    printf("Passageiro foi recebido normalmente: %d\n",id);
    if (ponto->primeiro_passageiros == -1) {
        ponto->primeiro_passageiros = id;
    } 
    else {
        // Caso contrário, percorra a lista e insira no final
        int atual = ponto->primeiro_passageiros;
        while (conjunto_passageiro[atual].prox != -1) {
            atual = conjunto_passageiro[atual].prox;
        }
        conjunto_passageiro[atual].prox = id;
    }
    // Incrementa o contador de passageiros
    ponto->num_pessoas++;
    printf("numero de pessoas no ponto %d após arrumar:%d\n",id, ponto->num_pessoas);        
}

// ----------------------------------------------------------------------------------------
 void main(int argc, char *argv[]){
    // usando valores pré-definido
    
    stay=1;
    total_pessoas = P; 
    pid_t pid = fork();

    // PROCESSO ONIBUS
    if (pid == 0) {// Processo filho - Processo 'ônibus'
        
        // DECLARANDO THREADS E AS ESTRUTURAS DE DADOS
        t_Passageiros conjunto_passageiro[P];
        pthread_t Passageiro_h[P];
        pthread_t PontoOnibus_h[S];  
        t_Onibus conjunto_onibus[C];
        pthread_t Onibus_h[C];
        // INICIANDO PONTO DE ONIBUS ------------------------------------------------------
        
        int tmp_start; 
        int tmp_end;
        for(int i=0;i<S;i++){
            conjunto_pontos[i].id = i; 
            conjunto_pontos[i].primeiro_passageiros = -1;
            conjunto_pontos[i].num_pessoas = 0;
            pthread_mutex_init(&conjunto_pontos[i].mutex,NULL);
        }
        // INICIANDO ONIBUS ---------------------------------------------------------------
        for(int i=0;i<C;i++){
            conjunto_onibus[i].id = i;
            conjunto_onibus[i].pontos = conjunto_pontos;
            conjunto_onibus[i].partida = rand()%S; 
            pthread_mutex_init(&conjunto_onibus[i].mutex_passageiros,NULL);  

        }

        // INICIANDO PASSAGEIRO -----------------------------------------------------------
        for(int i=0;i<P;i++){
            conjunto_passageiro[i].flag = 0;
            conjunto_passageiro[i].id = i;
            conjunto_passageiro[i].prox = -1;
        }

        // -----------------------RUNNING THREADS -----------------------------------------
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