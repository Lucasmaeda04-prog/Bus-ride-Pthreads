// para compilar: gcc prodcons_n_threads_sem.c -o prodcons_n_threads_sem -lpthread
// para executar: prodcons_n_threads_sem
#define _POSIX_C_SOURCE 200112L /* Or higher */ // permitindo a utilização de barreiras
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h> 
#include <unistd.h>
#include <sys/types.h>
#include <time.h> 
#include <sys/stat.h>

int S; 
int P;
int A;
int C; 
sem_t pessoas_inseridas; 
sem_t pessoas_global_sem; 
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
    int pessoas_descidas;
    int id; 
    int id_ponto; // um ponteiro para a array de pontos de ônibus 
    int partida;  // ponto de partida para o ônibus
    pthread_mutex_t  mutex_desembarque; // mutex para acordar passageiro quando chegar no ponto
    pthread_mutex_t  sleep_onibus_descida; // onibus esperando povo descer 
    pthread_mutex_t  sleep_onibus_subida; // onibus esperando povo subir 
    sem_t desce_passageiros; // semáforo com valor temporário equivalente ao número de pessoas que estão no onibus e podem ou não descer. 
};

struct t_Passageiros{     
    int id; 
    int ponto_origem; 
    int ponto_saida;
    struct timespec hora_chegada_origem; 
    struct timespec hora_entrada_onibus;
    struct timespec hora_saida_onibus; 
    int id_onibus;
    int prox;  // int com endereço para próxima elemento da FIFO no ponto de onibus 
    pthread_mutex_t mutex_embarque; // mutex para hora de embarcar 
};

typedef struct t_Argumento{     
    t_Passageiros* conjunto_passageiro;
    t_PontoOnibus* conjunto_pontos;
    t_Onibus* conjunto_onibus;  
}t_Argumento;

t_Argumento args; 

// ----------------------------------------------------------------------------------------
void criarPastaRastreio() {
    struct stat st = {0};
    if (stat("rastreio", &st) == -1) {
        mkdir("rastreio", 0700);
    }
}

void salvarViagem(t_Passageiros *passageiro) {
    criarPastaRastreio();
    
    char arquivo_trace[50];
    sprintf(arquivo_trace, "rastreio/passageiro%d.trace", passageiro->id);


    FILE *file = fopen(arquivo_trace, "w");
    if (file == NULL) {
        perror("Erro ao abrir arquivo de rastreamento");
        exit(1);
    }

    // Escreve todos os eventos no arquivo
    fprintf(file, "Horário de Chegada no ponto de origem %d: %ld.%09ld\n", passageiro->ponto_origem, passageiro->hora_chegada_origem.tv_sec, passageiro->hora_chegada_origem.tv_nsec);
    fprintf(file, "Horário de Entrada no ônibus %d: %ld.%09ld\n",passageiro->id_onibus, passageiro->hora_entrada_onibus.tv_sec, passageiro->hora_entrada_onibus.tv_nsec);
    fprintf(file, "Horário de Saída do ônibus %d: %ld.%09ld\n",passageiro->id_onibus, passageiro->hora_saida_onibus.tv_sec, passageiro->hora_saida_onibus.tv_nsec);
    fprintf(file, "Ponto de destino: %d\n", passageiro->ponto_saida);
    fclose(file);
}

void adicionarPassageiroPonto(int Ponto, int id){
    t_Passageiros *conjunto_passageiro ;
    t_PontoOnibus *conjunto_pontos;
    t_Onibus *conjunto_onibus;
    conjunto_passageiro= args.conjunto_passageiro;
    conjunto_pontos = args.conjunto_pontos;
    conjunto_onibus = args.conjunto_onibus; 


    int atual = conjunto_pontos[Ponto].primeiro_passageiros;
    if (atual == -1) {
        // printf("Primeiro:%d e prox:%d\n",atual,conjunto_passageiro[atual].prox);
        printf("(Ponto%d):não havia passageiros, o primeiro agora é:%d\n",Ponto,id);
        conjunto_pontos[Ponto].primeiro_passageiros = id;
    } 
    else {
        // Caso contrário, percorra a lista e insira no final
        while (conjunto_passageiro[atual].prox != -1) {
            printf("(Ponto%d): Sequência no ponto:%d-",Ponto,atual);
            atual = conjunto_passageiro[atual].prox;
        }
        printf("\n Ponto(%d):Adicionado o passageiro %d\n",Ponto, id);
        conjunto_passageiro[atual].prox = id;
    }
        //printf("Atual:%d e prox:%d\n",atual,conjunto_passageiro[atual].prox);
    // Incrementa o contador de passageiros
    conjunto_pontos[Ponto].num_pessoas++;
    printf("(ponto %d): numero de pessoas no ponto após arrumar:%d O primeiro é o %d \n",Ponto,conjunto_pontos[Ponto].num_pessoas,conjunto_pontos[Ponto].primeiro_passageiros);        
}

void *onibus(void *arg){
    int id = (int)(intptr_t)arg; 
    printf("ID:%d",id);
    int stay=1; 
    int descida_tmp; // variável tmp que recebe valor equivalente ao contador desce_passageiro
    t_Onibus *onibus = &args.conjunto_onibus[id]; 
    t_Passageiros *conjunto_passageiro ;
    t_PontoOnibus *conjunto_pontos;
    t_Onibus *conjunto_onibus;
    conjunto_passageiro = args.conjunto_passageiro;
    conjunto_pontos = args.conjunto_pontos;
    conjunto_onibus = args.conjunto_onibus; 
    printf("testando %d",conjunto_passageiro[id].id);
    printf("Thread do Onibus:(%d) iniciada\n",onibus->id);

    // circulação do onibus enquanto houver passageiros no sistema global. 
    do {
        if(pthread_mutex_trylock(&conjunto_pontos[(onibus->partida)%S].mutex)==0 ){ // verificando se já há um ônibus no ponto 
            
            // verificando se ônibus ainda precisa rodar --------------------------------------------------
            sem_getvalue(&pessoas_global_sem,&total_pessoas); 
            printf("(Onibus %d)Estou no ponto:%d total pessoas no sistema:%d  no onibus há %d pessoa  e outro onibus há %d\n\n\n",onibus->id,onibus->partida%S, total_pessoas,onibus->num_pessoas,conjunto_onibus[(onibus->partida+1)%S].num_pessoas);
            if(total_pessoas<=0){ // número de pessoas globalmente isso precisa ser uma semáforo. // TODO - VIRAR UM SEM_GET_VALUE
                printf("\n\n\npessoas no onibus: %d\n\n\n",onibus->num_pessoas);
                printf("\n\n\ncabou onibus\n\n\n");
                stay=0;

            }
            conjunto_pontos[(onibus->partida)%S].id_onibus = onibus->id; 
            // DESCIDA DOS PASSAGEIROS NO ONIBUS  --------------------------------------------------------
            sem_init(&onibus->desce_passageiros, 0, onibus->num_pessoas-1); // semáforo com valor = número de pessoas -1, menos um pois o semáforo trava a thread qnd decrementar de 0
            onibus->pessoas_descidas = onibus->num_pessoas; 
            if(onibus->num_pessoas>0){
                printf("(onibus %d): desembarcar passageiros no ponto %d, total de %d passageiros no onibus\n",onibus->id,onibus->partida%S, onibus->num_pessoas);
                while(onibus->pessoas_descidas > 0){
                    pthread_mutex_unlock(&onibus->mutex_desembarque);
                    pthread_mutex_lock(&onibus->sleep_onibus_descida);
                }   
            }

            // SUBIDA DOS PASSAGEIROS NO ONIBUS ---------------------------------------------------------
            pthread_mutex_lock(&conjunto_pontos[(onibus->partida)%S].mutex_ob); // impedindo que novos passageiros entrem no ponto enquanto o ônibus estiver lá
            if(conjunto_pontos[(onibus->partida)%S].num_pessoas>0){ // verificando se ainda tem pessoas no ponto // TODO - ARRUMAR ISSO COM UM SEMÁFORO CONTADOR DEPOIS. 
                printf("(onibus %d): iniciando embarque dos passageiros no ponto %d, o ponto possui %d passageiros esperando, o primeiro é o%d\n,",onibus->id,onibus->partida%S,conjunto_pontos[(onibus->partida)%S].num_pessoas, conjunto_pontos[(onibus->partida)%S].primeiro_passageiros);
                pthread_mutex_unlock(&conjunto_passageiro[conjunto_pontos[(onibus->partida)%S].primeiro_passageiros].mutex_embarque); // acordando o primeiro passageiro do FIFO do Ponto 
                pthread_mutex_lock(&onibus->sleep_onibus_subida); // onibus se colando para dormir enquanto passageiros sobem 
                printf("(onibus %d):  embarque concluido está no ponto %d com %d pessoas \n",onibus->id,(onibus->partida)%S,onibus->num_pessoas);            
            }
            pthread_mutex_unlock(&conjunto_pontos[(onibus->partida)%S].mutex_ob);
            pthread_mutex_unlock(&conjunto_pontos[(onibus->partida)%S].mutex); // liberando mutex que dá acesso ao ponto de ônibus
        }
        onibus->partida++; 
        // printf("\nONIBUS SAINDO\n\n");
    } while (stay);
    printf("ONIBUS %d SAINDO\n",onibus->id);
    printf("PESSOAS TOTAIS NO SISTEMA:%d",total_pessoas);
    pthread_exit(0); 
}

void *passageiro(void *arg){
    int id = (int)(intptr_t)arg;
    int stay=1;
    
    t_Passageiros *passageiro = &args.conjunto_passageiro[id];
    t_Passageiros *conjunto_passageiro ;
    t_PontoOnibus *conjunto_pontos;
    t_Onibus *conjunto_onibus;
    conjunto_passageiro= args.conjunto_passageiro;
    conjunto_pontos = args.conjunto_pontos;
    conjunto_onibus = args.conjunto_onibus; 


    printf("Thread do Passageiro:(%d) iniciada\n",passageiro->id);
    int tmp_start = rand()%S;
    int tmp_end = rand()%S;
    passageiro->ponto_origem = tmp_start;
    passageiro->ponto_saida = tmp_end; 


    // ENTRADA NO PONTO DE ONIBUS --------------------------------------------------------------------  

    pthread_mutex_lock(&conjunto_pontos[tmp_start].mutex_passageiro); // necessário proteger a inserção do passageiro no ponto pois é necessário manter a ordem do FIFO
    pthread_mutex_lock(&conjunto_pontos[tmp_start].mutex_ob);  // utilizando a mesma lógica de escritor e leitor, entre os passageiros que entram no ponto e os que sobem para o onibus
    adicionarPassageiroPonto(tmp_start,passageiro->id);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&passageiro->hora_chegada_origem); // marcando inicio da  espera no ponto 
    pthread_mutex_unlock(&conjunto_pontos[tmp_start].mutex_ob);
    pthread_mutex_unlock(&conjunto_pontos[tmp_start].mutex_passageiro);
    
    // EMBARQUE PASSAGEIRO  ----------------------------------------------------------------------------

    do { //enquanto não conseguir embarcar 
        pthread_mutex_lock(&passageiro->mutex_embarque); // passageiro dorme, enquanto o ônibus não chega no ponto
        printf("(passageiro %d): tentando embarcar no ponto %d com o ônibus %d\n",passageiro->id, tmp_start, conjunto_onibus[tmp_start].id);
        passageiro->id_onibus = conjunto_pontos[tmp_start].id_onibus;
        if((conjunto_onibus[passageiro->id_onibus].assentos - conjunto_onibus[passageiro->id_onibus].num_pessoas)>0){ // verifica se há lugares livres
            printf("(passageiro %d):Atualizando o indíce do primeiro passageiro para o ponto%d, era %d, agora é %d",passageiro->id,tmp_start, conjunto_pontos[passageiro->ponto_origem].primeiro_passageiros, conjunto_passageiro[conjunto_pontos[passageiro->ponto_origem].primeiro_passageiros].prox);
            conjunto_pontos[passageiro->ponto_origem].primeiro_passageiros =  conjunto_passageiro[conjunto_pontos[passageiro->ponto_origem].primeiro_passageiros].prox;
            sem_post(&pessoas_inseridas);
            sem_getvalue(&pessoas_inseridas,&total_pessoas_inseridas);
            // printf("PESSOAS EMBARCADAS:%d\n\n",total_pessoas_inseridas);

            conjunto_onibus[passageiro->id_onibus].num_pessoas++;
            conjunto_onibus[passageiro->id_onibus].assentos--;
            conjunto_pontos[tmp_start].num_pessoas--;

            // verifica se não é o último passageiro a subir 
            
            stay=0;
        }
        if(passageiro->prox!=-1){ 
            pthread_mutex_unlock(&conjunto_passageiro[passageiro->prox].mutex_embarque); // acorda o próximo passageiro da fila 
        }
        else{
            printf("(passageiro %d): acordando o ônibus %d que possui %d passageiros, no ponto %d\n",passageiro->id,conjunto_pontos[tmp_start].id_onibus,conjunto_onibus[passageiro->id_onibus].num_pessoas,tmp_start);
            pthread_mutex_unlock(&conjunto_onibus[passageiro->id_onibus].sleep_onibus_subida); // acordando o ônibus
        }
    }while(stay);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&passageiro->hora_entrada_onibus); // marcando horário de entrada no onibus
    printf("\n(passageiro %d): EMBARCADO NO PONTO %d PELO ÔNIBUS:%d, ONIBUS POSSUI  TOTAL INSERIDAS ATÉ AGORA:%d\n",passageiro->id,tmp_start, passageiro->id_onibus,total_pessoas_inseridas);


    // DESEMBARQUE PASSAGEIRO -------------------------------------------------
    int stay2 =1; 
    do{
        printf("Valor do Mutex: %p\n", conjunto_onibus[passageiro->id_onibus].mutex_desembarque);
        pthread_mutex_lock(&conjunto_onibus[passageiro->id_onibus].mutex_desembarque);// dorme enquanto não chegar em um ponto e é acordado pelo ônibus
        printf("(passageiro %d): tentando sair pelo ônibus %d , no ponto:%d,saida esperada:%d \n\n",passageiro->id,passageiro->id_onibus, conjunto_onibus[passageiro->id_onibus].partida%S,passageiro->ponto_saida);

        if(conjunto_onibus[passageiro->id_onibus].partida%S == passageiro->ponto_saida){// verifica se tá na posição correta
            stay2 = 0; // se está na posição correta, sai do loop
            conjunto_onibus[passageiro->id_onibus].num_pessoas--;
            conjunto_onibus[passageiro->id_onibus].assentos++;
            sem_wait(&pessoas_global_sem); // diminuindo número de passageiros no escopo global
            printf("(passageiro %d): saindo pelo ônibus %d no ponto %d \n\n",passageiro->id, passageiro->id_onibus,conjunto_onibus[passageiro->id_onibus].partida%S);
        }
        int num_passageiros_descida; 
        sem_getvalue(&conjunto_onibus[passageiro->id_onibus].desce_passageiros,&num_passageiros_descida);
        conjunto_onibus[passageiro->id_onibus].pessoas_descidas--;
        pthread_mutex_unlock(&conjunto_onibus[passageiro->id_onibus].sleep_onibus_descida); // acordando ônibus
    } while (stay2);

    printf("\n\nPASSAGEIRO: %d SAINDO!!!\n\n",passageiro->id);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&passageiro->hora_saida_onibus);
    salvarViagem(passageiro);
    pthread_exit(0);
}



// ----------------------------------------------------------------------------------------
 void main(int argc, char *argv[]){
    // usando valores pré-definido
    printf("Quantos pontos de ônibus você deseja inserir no sistema:\n");
    scanf("%d",&S);
    printf("Quantos ônibus você deseja inserir no sistema:\n");
    scanf("%d",&C);
    printf("Quantos passageiros você deseja inserir no sistema:\n");
    scanf("%d",&P);
    printf("Quantos assemtos por ônibus você deseja inserir no sistema:\n");
    scanf("%d",&A);

    total_pessoas = P; 
    t_Passageiros conjunto_passageiro[P];
    t_Onibus conjunto_onibus[C];
    t_PontoOnibus conjunto_pontos[S];
    
    args.conjunto_onibus = conjunto_onibus;
    args.conjunto_passageiro = conjunto_passageiro;
    args.conjunto_pontos = conjunto_pontos;
    
    pthread_t Passageiro_h[P];
    pthread_t Onibus_h[C];

    sem_init(&pessoas_global_sem,0,P); 
    sem_init(&pessoas_inseridas,0,0);
    pid_t pid = fork();
    
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
            conjunto_onibus[i].num_pessoas = 0;

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
            pthread_mutex_lock(&conjunto_passageiro[i].mutex_embarque);  

        }   

        // -----------------------RUNNING THREADS -----------------------------------------
         for(int i=0;i<C;i++){
            if (pthread_create(&Onibus_h[i],0,onibus,(void *)(intptr_t)i)!=0){
                printf("Falha ao criar o ônibus");
                fflush(0);
                exit(0);
            }; 
        }
         for(int i=0;i<P;i++){
            if (pthread_create(&Passageiro_h[i],0,passageiro,(void *)(intptr_t)i)!=0){
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
        return;
    }

    exit(0); 
} 