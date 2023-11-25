// para compilar: gcc prodcons_n_threads_sem.c -o prodcons_n_threads_sem -lpthread
// para executar: prodcons_n_threads_sem

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h> 
#include <unistd.h>
#include <sys/types.h>
#include <time.h> 

#define S 4
#define C 2
#define A 10
#define P 5

sem_t pessoas_inseridas; 
sem_t pessoas_sem; 
sem_t global_mutex; 
int total_pessoas; 
int total_pessoas_inseridas;
int stay; 

typedef struct t_Onibus t_Onibus;
typedef struct t_Passageiros t_Passageiros;

typedef struct t_PontoOnibus{
    int num_pessoas; 
    int id; 
    pthread_mutex_t mutex; // mutex para impedir que haja mais de um onibus em um mesmo ponto.  
    pthread_mutex_t mutex_passageiro; // mutex para controlar acesso dos passageiros 
    pthread_mutex_t mutex_ob; // mutex para priorizar a entrada do onibus no ponto invés de passageiros (passageiro e onibus não podem estarem alterando numero de pessoas no ponto no mesmo tempo). 
    int  id_onibus; // int que indica qual ônibus está esperando no ponto 
    int primeiro_passageiros; // id com id  para o primeiro passageiro da fila
}t_PontoOnibus; 

struct t_Onibus{
    int num_pessoas; // espaço ocupado no buffer
    int assentos; // numero total de espaço no buffer
    int id; 
    int id_ponto; // um ponteiro para a array de pontos de ônibus 
    int partida;  // ponto de partida para o ônibus
    pthread_mutex_t descida_passageiro; // mutex para acordar passageiro quando chegar no ponto
    pthread_mutex_t sleep_onibus_descida; // onibus esperando povo descer 
    pthread_mutex_t sleep_onibus_subida; // onibus esperando povo subir 
    sem_t desce_passageiros; 
};

struct t_Passageiros{     
    int id; 
    int ponto_origem; 
    int ponto_saida;
    time_t tempo_comeco; 
    time_t tempo_fim; 
    int id_onibus;
    int prox;  // int com endereço para próxima elemento da FIFO no ponto de onibus 
    pthread_mutex_t mutex_embarque; // mutex para hora de embarcar 
};

// DECLARANDO THREADS E AS ESTRUTURAS DE DADOS
t_Passageiros conjunto_passageiro[P];
pthread_t Passageiro_h[P];
t_Onibus conjunto_onibus[C];
pthread_t Onibus_h[C];
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

void adicionarPassageiroPonto(t_PontoOnibus *Ponto, int id){
    t_PontoOnibus *ponto =  Ponto; 
    int atual = ponto->primeiro_passageiros;
    if (ponto->primeiro_passageiros == -1) {
        printf("Primeiro:%d e prox:%d\n",atual,conjunto_passageiro[atual].prox);

        ponto->primeiro_passageiros = id;
    } 
    else {
        // Caso contrário, percorra a lista e insira no final
        while (conjunto_passageiro[atual].prox != -1) {
            atual = conjunto_passageiro[atual].prox;
        }
        conjunto_passageiro[atual].prox = id;
    }
        printf("Atual:%d e prox:%d\n",atual,conjunto_passageiro[atual].prox);
    // Incrementa o contador de passageiros
    ponto->num_pessoas++;
    printf("numero de pessoas no ponto %d após arrumar:%d\n",id,ponto->num_pessoas);        
}

void *onibus(void *arg){
    int stay=1; 
    t_Onibus *onibus = (t_Onibus *)arg; 
    printf("Thread do Onibus:(%d) iniciada\n",onibus->id);
    int p = onibus->partida;
    printf("p: %d\n",p);    

    // circulação do onibus enquanto houver passageiros no sistema global. 
    do {
        if(pthread_mutex_trylock(&conjunto_pontos[(p)%S].mutex)==0 ){ // verificando se já há um ônibus no ponto 
            sem_getvalue(&pessoas_sem,&total_pessoas);
            conjunto_pontos[p].id_onibus = onibus->id; 
            // chamando pessoas para descer.
            onibus->num_pessoas--;
            onibus->assentos++;

            // passageiros começam a subir no ônibus
            pthread_mutex_lock(&conjunto_pontos[p].mutex_ob); 
            if(conjunto_pontos[p].num_pessoas>0){ // verificando se ainda tem pessoas no ponto // TODO - ARRUMAR ISSO COM UM SEMÁFORO CONTADOR DEPOIS. 
                pthread_mutex_unlock(&conjunto_passageiro[conjunto_pontos[p].primeiro_passageiros].mutex_embarque); // acordando o primeiro passageiro do FIFO do Ponto 
                pthread_mutex_lock(&onibus->sleep_onibus_subida);
                printf("Onibus %d está no ponto %d com %d pessoas \n",onibus->id,p%S,onibus->num_pessoas);            
                /*if(total_pessoas<=0){
                    printf("\n\n\ncabou onibus\n\n\n");
                    stay=0;total_pessoas--;
                }*/
            }
            pthread_mutex_unlock(&conjunto_pontos[p].mutex_ob);
            pthread_mutex_unlock(&conjunto_pontos[(onibus->partida)%S].mutex); // liberando mutex que dá acesso ao ponto de ônibus

            if(sem_trywait(&pessoas_sem)!=0){
                printf("\n\n\npessoas no onibus: %d\n\n\n",onibus->num_pessoas);
                printf("\n\n\ncabou onibus\n\n\n");
                stay=0;
            }
        }
        printf("p: %d\n",p);    
        p++; 
        printf("\nONIBUS SAINDO\n\n");

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


    // ENTRADA NO PONTO DE ONIBUS --------------------------------------------------------------------  

    pthread_mutex_lock(&conjunto_pontos[tmp_start].mutex_passageiro); // necessário proteger a inserção do passageiro no ponto pois é necessário manter a ordem do FIFO
    pthread_mutex_lock(&conjunto_pontos[tmp_start].mutex_ob);  // utilizando a mesma lógica de escritor e leitor, entre os passageiros que entram no ponto e os que sobem para o onibus
    adicionarPassageiroPonto(&conjunto_pontos[tmp_start],passageiro->id);
    pthread_mutex_unlock(&conjunto_pontos[tmp_start].mutex_ob);
    pthread_mutex_unlock(&conjunto_pontos[tmp_start].mutex_passageiro);
    
    // EMBARQUE NO ONIBUS  ----------------------------------------------------------------------------

    passageiro->tempo_comeco = time(NULL); // marcando inicio da  espera no ponto 
    passageiro->tempo_fim = time( NULL); 

    do { //enquanto não conseguir embarcar 
        pthread_mutex_lock(&passageiro->mutex_embarque); // passageiro dorme, enquanto o ônibus não chega no ponto
        passageiro->id_onibus = conjunto_pontos[tmp_start].id_onibus;
        printf("LOOP\n\n");
        if((conjunto_onibus[passageiro->id_onibus].assentos - conjunto_onibus[passageiro->id_onibus].num_pessoas)>=0){ // verifica se há lugares livres
            sem_post(&pessoas_inseridas);
            sem_getvalue(&pessoas_inseridas,&total_pessoas_inseridas);
            printf("PESSOAS EMBARCADAS:%d\n\n",total_pessoas_inseridas);
            conjunto_onibus[passageiro->id_onibus].num_pessoas++;
            conjunto_onibus[passageiro->id_onibus].assentos--;
            conjunto_pontos[tmp_start].num_pessoas--;
            if(passageiro->prox!=-1){ // verifica se não é o último passageiro a subir 
                pthread_mutex_unlock(&conjunto_passageiro[passageiro->prox].mutex_embarque); // acorda o próximo passageiro da fila 
            }
            else{
                pthread_mutex_unlock(&conjunto_onibus[passageiro->id_onibus].sleep_onibus_subida);
            }
            stay=0;
        }
    }while(stay);
    sem_getvalue(&pessoas_inseridas,&total_pessoas_inseridas);
    printf("\n\nPASSAGEIRO %d PELO ÔNIBUS:%d total inseridos de:%d\n",passageiro->id,passageiro->id_onibus,total_pessoas_inseridas);
    
    pthread_exit(0);
}



// ----------------------------------------------------------------------------------------
 void main(int argc, char *argv[]){
    // usando valores pré-definido
    
    stay=1;
    total_pessoas = P; 
    pid_t pid = fork();
    sem_init(&pessoas_sem,0,P);
    sem_init(&pessoas_inseridas,0,0);

    // PROCESSO ONIBUS
    if (pid == 0) {// Processo filho - Processo 'ônibus'
        
       
        // INICIANDO PONTO DE ONIBUS ------------------------------------------------------
        
        int tmp_start; 
        int tmp_end;
        for(int i=0;i<S;i++){
            conjunto_pontos[i].id = i; 
            conjunto_pontos[i].primeiro_passageiros = -1;
            conjunto_pontos[i].num_pessoas = 0;
            pthread_mutex_init(&conjunto_pontos[i].mutex,NULL);
            pthread_mutex_init(&conjunto_pontos[i].mutex_passageiro,NULL);
            pthread_mutex_init(&conjunto_pontos[i].mutex_ob,NULL);      

        }
        // INICIANDO ONIBUS ---------------------------------------------------------------
        for(int i=0;i<C;i++){
            conjunto_onibus[i].id = i;
            conjunto_onibus[i].assentos = A;
            conjunto_onibus[i].partida = rand()%S; 
            pthread_mutex_init(&conjunto_onibus[i].descida_passageiro,NULL);  
            pthread_mutex_init(&conjunto_onibus[i].sleep_onibus_descida,NULL);  
            pthread_mutex_init(&conjunto_onibus[i].sleep_onibus_subida,NULL);  
            // colocando mutex para começar com valor 0
            pthread_mutex_lock(&conjunto_onibus[i].descida_passageiro);  // passageiro só pode descer quando o onibus chegar no ponto 
            pthread_mutex_lock(&conjunto_onibus[i].sleep_onibus_descida); // onibus dorme enquanto passageiros tiverem que descer  
            pthread_mutex_lock(&conjunto_onibus[i].sleep_onibus_subida); //  onibus dorme enquanto passageiros tiverem que subir

        }

        // INICIANDO PASSAGEIRO -----------------------------------------------------------
        for(int i=0;i<P;i++){
            conjunto_passageiro[i].id = i;
            conjunto_passageiro[i].prox = -1;
            pthread_mutex_init(&conjunto_passageiro[i].mutex_embarque,NULL);  
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