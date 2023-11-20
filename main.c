// para compilar: gcc prodcons_n_threads_sem.c -o prodcons_n_threads_sem -lpthread
// para executar: prodcons_n_threads_sem

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h> 
#include <unistd.h>
#include <sys/types.h>


typedef struct {
    int num_pessoas;
    int id; 
    sem_t mutex; // mutex para alterar o número de pessoas dentro.  
}t_PontoOnibus; 

void *pontoOnibus(void *arg){
    t_PontoOnibus *ponto = (t_PontoOnibus *)arg; 
    printf("Thread do Ponto:(%d) iniciada\n",ponto->id);
    pthread_exit(0); 
}

int main(int argc, char *argv[]){
    // usando valores pré-definido
    int S = 5; // atoi(argv[1]);
    int C = 0; // atoi(argv[2]);
    int P = 0; // atoi(argv[3]);
    int A = 0; // atoi(argv[4]);
    
    pid_t pid = fork();
    if (pid == 0) {// Processo filho - Processo 'ônibus'
        
        // inicializando Pontos de onibus
        t_PontoOnibus pontos[S];
        pthread_t PontoOnibus_h[S];  
        for(int i=0;i<S;i++){
            pontos[i].id = i; 
            sem_init(&pontos[i].mutex,0,1);
            if (pthread_create(&PontoOnibus_h[i],0,pontoOnibus,(void *)&pontos[i])!=0){
                printf("Falha ao criar o ponto de ônibus");
                fflush(0);
                exit(0);
            };
        }
        // rodando as threads dos pontos 
        for (int i=0; i < S; i++) {
		    pthread_join(PontoOnibus_h[ i ], 0);
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