// mutualExclusion.c
/* * Sistema bancário multi-thread estendido.
 * Suporta N contas, logs individuais e operações aleatórias.
   N >= 1, para efeito completo, que N seja maior que 2.
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#include "bankAccount.h"

// Opções de operação como números (escolhi arbitrariamente)
#define OP_DEPOSIT 0
#define OP_WITHDRAW 1
#define OP_BALANCE 2
#define OP_TRANSFER 3

// Função que cada thread executa
void* clientThread(void * arg) { 
    unsigned long id = (unsigned long) arg;
    
    int numTransactions = 100; // Cada thread fará 100 ações aleatórias, só mudar essa varíavel caso deseje

    for (int i = 0; i < numTransactions; i++) {
        // Seleciona uma conta aleatória (com NUM_ACCOUNTS = 5, significa que será a conta 0 até a conta 4)
        int contaAlvo = rand() % NUM_ACCOUNTS;
        
        // Seleciona uma operação aleatória a se fazer (0 a 3)
        int operacao = rand() % 4;
        
        // Valor aleatório entre 10 e 150, para adicionar mais aleatoridade
        double valor = 10.0 + (rand() % 141);

        if(operacao == OP_DEPOSIT){
            deposit(contaAlvo, valor);
        } else if(operacao == OP_WITHDRAW){
            withdraw(contaAlvo, valor);
        } else if(operacao == OP_BALANCE){
            balanceCheck(contaAlvo);
        }
        else if (operacao == OP_TRANSFER){ // caso especial que usa 2 contas, geramos outra conta
         int contaDestino;
         contaDestino = rand() % NUM_ACCOUNTS; // observe que a 2a conta pode ser igual a contaAlvo, tem uma detecção pra isso na função no header.
         accountTransfer(contaAlvo, contaDestino, valor);
        }
        
        
        // Uma pausa de 100 microssegundos entre cada operação na thread, isso pra ter mais entrecruzamento,
        // não necessário mas achei legal adicionar isso
        usleep(100); 
    }
    return NULL;
}

int main(int argc, char** argv) {
    // Aleatorizador baseado no tempo
    srand(time(NULL));

    pthread_t * threads;
    unsigned long numThreads = 0;

    if (argc != 2) { // para executar mutualExclusion, precisamos informar que se deve colocar um numero caso ele nao saiba
        fprintf(stderr, "Uso: ./mutualExclusion [numeroDeThreads]\n");
        exit(1);
    }
    
    numThreads = strtoul(argv[1], 0, 10);
    threads = malloc(numThreads * sizeof(pthread_t)); // garante que tem espaço no computador para criar as threads

    initAccounts(); // chama a inicialização das contas
    printf("Sistema iniciado com %d contas.\n", NUM_ACCOUNTS);

    for (unsigned long i = 0; i < numThreads; i++) {
        pthread_create(&threads[i], NULL, clientThread, (void*)i);
    } // cria as threads, são comuns
  
    for (unsigned long i = 0; i < numThreads; i++) {
        pthread_join(threads[i], NULL);
    } // faz join nas threads, para que uma não seja interrompida no meu de seu processo

    printf("\nTodas as transações concluídas.\n");

    cleanupAccounts(); // mata as contas bancarias, falando quanto tinha em cada conta antes de encerra-lá
    
    printf("Verifique os arquivos 'conta_X.log' para o extrato detalhado.\n");

    free(threads);
    return 0;
}