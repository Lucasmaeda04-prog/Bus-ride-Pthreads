// para compilar: gcc prodcons_n_threads_sem.c -o prodcons_n_threads_sem -lpthread
// para executar: prodcons_n_threads_sem

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h> 
#include <unistd.h>
#include <sys/types.h>
#include <time.h> 

#define S 2
#define C 1
#define A 10
#define P 2

sem_t pessoas_inseridas; 
sem_t pessoas_global_sem; 
sem_t global_mutex; 
int total_pessoas; 
int total_pessoas_inseridas;

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
    pthread_mutex_t mutex_desembarque; // mutex para acordar passageiro quando chegar no ponto
    pthread_mutex_t sleep_onibus_descida; // onibus esperando povo descer 
    pthread_mutex_t sleep_onibus_subida; // onibus esperando povo subir 
    sem_t desce_passageiros; // semáforo com valor temporário equivalente ao número de pessoas que estão no onibus e podem ou não descer. 
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
    int descida_tmp; // variável tmp que recebe valor equivalente ao contador desce_passageiro
    t_Onibus *onibus = (t_Onibus *)arg; 
    printf("Thread do Onibus:(%d) iniciada\n",onibus->id);

    // circulação do onibus enquanto houver passageiros no sistema global. 
    do {
        if(pthread_mutex_trylock(&conjunto_pontos[(onibus->partida)%S].mutex)==0 ){ // verificando se já há um ônibus no ponto 
            
            // verificando se ônibus ainda precisa rodar --------------------------------------------------
            sem_getvalue(&pessoas_global_sem,&total_pessoas); 
            printf("total pessoas:%d  no onibus há %d pessoas\n\n",total_pessoas,onibus->num_pessoas);
            printf("PESSOAS TOTAIS NO SISTEMA:%d",total_pessoas);
            if(total_pessoas<=0){ // número de pessoas globalmente isso precisa ser uma semáforo. // TODO - VIRAR UM SEM_GET_VALUE
                printf("\n\n\npessoas no onibus: %d\n\n\n",onibus->num_pessoas);
                printf("\n\n\ncabou onibus\n\n\n");
                stay=0;

            }

            conjunto_pontos[(onibus->partida)%S].id_onibus = onibus->id; 
            // DESCIDA DOS PASSAGEIROS NO ONIBUS  --------------------------------------------------------
            sem_init(&onibus->desce_passageiros, onibus->num_pessoas, onibus->num_pessoas); // semáforo com valor = número de pessoas -1, menos um pois o semáforo trava a thread qnd decrementar de 0
            if(onibus->num_pessoas>0){
                printf("desembarcar passageiros\n");
                pthread_mutex_unlock(&onibus->mutex_desembarque);
                pthread_mutex_lock(&onibus->sleep_onibus_descida);
            }
            

            // SUBIDA DOS PASSAGEIROS NO ONIBUS ---------------------------------------------------------
            pthread_mutex_lock(&conjunto_pontos[(onibus->partida)%S].mutex_ob); // impedindo que novos passageiros entrem no ponto enquanto o ônibus estiver lá
            if(conjunto_pontos[(onibus->partida)%S].num_pessoas>0){ // verificando se ainda tem pessoas no ponto // TODO - ARRUMAR ISSO COM UM SEMÁFORO CONTADOR DEPOIS. 
                pthread_mutex_unlock(&conjunto_passageiro[conjunto_pontos[(onibus->partida)%S].primeiro_passageiros].mutex_embarque); // acordando o primeiro passageiro do FIFO do Ponto 
                pthread_mutex_lock(&onibus->sleep_onibus_subida); // onibus se colando para dormir enquanto passageiros sobem 
                printf("Onibus %d está no ponto %d com %d pessoas \n",onibus->id,(onibus->partida)%S,onibus->num_pessoas);            
                /*if(total_pessoas<=0){
                    printf("\n\n\ncabou onibus\n\n\n");
                    stay=0;total_pessoas--;
                }*/
            }
            pthread_mutex_unlock(&conjunto_pontos[(onibus->partida)%S].mutex_ob);
            pthread_mutex_unlock(&conjunto_pontos[(onibus->partida)%S].mutex); // liberando mutex que dá acesso ao ponto de ônibus
        }
        onibus->partida++; 
        printf("STAY %d\n\n",stay);
        printf("\nONIBUS SAINDO\n\n");
    } while (stay);
    printf("ONIBUS %d SAINDO\n",onibus->id);
    pthread_exit(0); 
}

void *passageiro(void *arg){
    int stay=1;
    t_Passageiros *passageiro = (t_Passageiros *)arg; 
    printf("Thread do Passageiro:(%d) iniciada",passageiro->id);
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
    
    // EMBARQUE PASSAGEIRO  ----------------------------------------------------------------------------

    passageiro->tempo_comeco = time(NULL); // marcando inicio da  espera no ponto 
    passageiro->tempo_fim = time( NULL); 

    do { //enquanto não conseguir embarcar 
        pthread_mutex_lock(&passageiro->mutex_embarque); // passageiro dorme, enquanto o ônibus não chega no ponto
        passageiro->id_onibus = conjunto_pontos[tmp_start].id_onibus;
        printf("LOOP\n\n");
        if((conjunto_onibus[passageiro->id_onibus].assentos - conjunto_onibus[passageiro->id_onibus].num_pessoas)>=0){ // verifica se há lugares livres
            sem_post(&pessoas_inseridas);
            sem_getvalue(&pessoas_inseridas,&total_pessoas_inseridas);
            // printf("PESSOAS EMBARCADAS:%d\n\n",total_pessoas_inseridas);

            conjunto_onibus[passageiro->id_onibus].num_pessoas++;
            conjunto_onibus[passageiro->id_onibus].assentos--;
            conjunto_pontos[tmp_start].num_pessoas--;

            // verifica se não é o último passageiro a subir 
            if(passageiro->prox!=-1){ 
                pthread_mutex_unlock(&conjunto_passageiro[passageiro->prox].mutex_embarque); // acorda o próximo passageiro da fila 
            }
            else{
                pthread_mutex_unlock(&conjunto_onibus[passageiro->id_onibus].sleep_onibus_subida); // acordando o ônibus
            }
            stay=0;
        }
    }while(stay);
    sem_getvalue(&pessoas_inseridas,&total_pessoas_inseridas);
    printf("\n\nPASSAGEIRO EMBARCADO %d PELO ÔNIBUS:%d TOTAL IN:%d\n",passageiro->id,passageiro->id_onibus,total_pessoas_inseridas);


    // DESEMBARQUE PASSAGEIRO -------------------------------------------------
    int stay2 =1; 
    do{
        
        pthread_mutex_lock(&conjunto_onibus[passageiro->id_onibus].mutex_desembarque);// dorme enquanto não chegar em um ponto e é acordado pelo ônibus
        printf("\ntentando sair passageiro(%d) ponto:%d,saida esperada:%d \n\n",passageiro->id,conjunto_onibus[passageiro->id_onibus].partida%S,passageiro->ponto_saida);
            // TODO -VERIFICAR O POR QUE DE NÃO ENTRAR NO WHILE
        if(conjunto_onibus[passageiro->id_onibus].partida%S == passageiro->ponto_saida){// verifica se tá na posição correta
            stay2 = 0; // se está na posição correta, sai do loop
            conjunto_onibus[passageiro->id_onibus].num_pessoas--;
            conjunto_onibus[passageiro->id_onibus].assentos++;
            sem_wait(&pessoas_global_sem); // diminuindo número de passageiros no escopo global. 
                // se o sem_try_wait der 0, é o último 
                    // acorda ônibus 
            printf("\nsaindo passageiro DE FATO:(%d)\n\n",passageiro->id);
        }
        int num_passageiros_descida; 
        sem_getvalue(&conjunto_onibus[passageiro->id_onibus].desce_passageiros,&num_passageiros_descida);
        printf("passageiros no onibus pra descer:%d\n",num_passageiros_descida);
        if(sem_trywait(&conjunto_onibus[passageiro->id_onibus].desce_passageiros)==0){  // verificação se é o último passageiro interno a tentar descer. 
            pthread_mutex_unlock(&conjunto_onibus[passageiro->id_onibus].sleep_onibus_descida); // acordando ônibus
        }
    } while (stay2);
    printf("PASSAGEIRO: %d SAINDO!!!",passageiro->id);
    pthread_exit(0);
}



// ----------------------------------------------------------------------------------------
 void main(int argc, char *argv[]){
    // usando valores pré-definido
    total_pessoas = P; 
    pid_t pid = fork();
    sem_init(&pessoas_global_sem,0,P); 
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
            pthread_mutex_init(&conjunto_onibus[i].mutex_desembarque,NULL);  
            pthread_mutex_init(&conjunto_onibus[i].sleep_onibus_descida,NULL);  
            pthread_mutex_init(&conjunto_onibus[i].sleep_onibus_subida,NULL);  
            // colocando mutex para começar com valor 0
            pthread_mutex_lock(&conjunto_onibus[i].mutex_desembarque);  // passageiro só pode descer quando o onibus chegar no ponto 
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