#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>

#define NUM_ACCOUNTS 5 // NUM_ACCOUNTS, para n ter erros, precisa ser >1, >2 caso o arquivo c queira fazer transferencia entre 2 

// Cada conta bancária vai ter: seu id, a qtde de dinheiro, seu lock e arquivo log
typedef struct {
    int id;
    double balance;
    pthread_mutex_t lock;
    FILE *logFile;
    int opsFuncionais;
    int opsDefeituosas;   
} Account;

// Array global de contas 
Account accounts[NUM_ACCOUNTS];

// Inicializa as contas,os locks e os arquivos de logs
void initAccounts() {
    for (int i = 0; i < NUM_ACCOUNTS; i++) {
        accounts[i].id = i;
        accounts[i].balance = 0.0; // começam sem dinheiro algum
        accounts[i].opsFuncionais = 0;
        accounts[i].opsDefeituosas = 0;
        pthread_mutex_init(&accounts[i].lock, NULL);  // ajusta os locks para serem comuns
        
        char filename[20];
        sprintf(filename, "conta_%d.log", i);  
        accounts[i].logFile = fopen(filename, "w"); // criação do arquivo de log
        if (accounts[i].logFile == NULL) { // se der erro (por faltar memória geralmente)
            perror("Erro ao criar arquivo de log");
            exit(1);
        }
        fprintf(accounts[i].logFile, "Conta %d criada. Saldo inicial: 0.00\n", i); //no arquivo de log da conta em específico, confirma que foi criada
    }
}

// Fecha as contas, falando quanto sobrou na conta (no arquivo de log da conta), e também destroi os locks
void cleanupAccounts() {
    for (int i = 0; i < NUM_ACCOUNTS; i++) {
        int total = accounts[i].opsDefeituosas + accounts[i].opsFuncionais;
        fprintf(accounts[i].logFile, "=== FIM DO EXTRATO ===\nSaldo Final: %.2f\n", accounts[i].balance);
        fprintf(accounts[i].logFile, "Operações Defeituosas: %d\n", accounts[i].opsDefeituosas);
        fprintf(accounts[i].logFile, "Operações Funcionais: %d\n", accounts[i].opsFuncionais);
        fprintf(accounts[i].logFile, "Operações Totais: %d\n", total);
        fclose(accounts[i].logFile);
        pthread_mutex_destroy(&accounts[i].lock);
    }
}

// Depositar na conta: saber para quem e quanto.
void deposit(int accountId, double amount) {

    pthread_mutex_lock(&accounts[accountId].lock);
    accounts[accountId].balance += amount;
    accounts[accountId].opsFuncionais++;
    fprintf(accounts[accountId].logFile, "[DEPOSITO] +%.2f | Saldo: %.2f\n", 
            amount, accounts[accountId].balance);

    pthread_mutex_unlock(&accounts[accountId].lock);
}

// Sacar na conta: tem restrição caso saldo insuficiente
void withdraw(int accountId, double amount) {
    
    pthread_mutex_lock(&accounts[accountId].lock);
    if(accounts[accountId].balance >= amount){ //nao pode negativar (pode zerar)
      accounts[accountId].balance -= amount;
      accounts[accountId].opsFuncionais++;
      fprintf(accounts[accountId].logFile, "[SAQUE]    -%.2f | Saldo: %.2f\n", amount, accounts[accountId].balance);
    }else{
      accounts[accountId].opsDefeituosas++;
      fprintf(accounts[accountId].logFile, "[FALHA SAQUE] Tentou sacar: %.2f | Mas seu saldo atual é: %.2f\n", amount, accounts[accountId].balance);
    }
    pthread_mutex_unlock(&accounts[accountId].lock);
}

// Consulta de saldo, e o arquivo de log da conta mostra que chamou e quanto tinha naquele momento
void balanceCheck(int accountId) {
    
    pthread_mutex_lock(&accounts[accountId].lock);
    accounts[accountId].opsFuncionais++;
    fprintf(accounts[accountId].logFile, "[CONSULTA] Verificando saldo... Atual: %.2f\n", accounts[accountId].balance);
    pthread_mutex_unlock(&accounts[accountId].lock);
}

// Transferência entre contas 2.0, agora tem uma prevenção de deadlock
void accountTransfer(int fromId, int toId, double amount) {
    
    if (fromId == toId) { // Se for para a mesma conta, é um caso importante de se considerar
        pthread_mutex_lock(&accounts[fromId].lock); // tranca já que iremos anotar isso no logfile
        accounts[fromId].opsDefeituosas++;
        fprintf(accounts[fromId].logFile, 
                "[TRANSF PARA SI MESMO] A conta %d tentou transferir para si!\n", fromId);
        pthread_mutex_unlock(&accounts[fromId].lock);
        return;
    }

    // Sempre travar o mutex de menor ID primeiro para evitar um deadlock
    Account *first = (fromId < toId) ? &accounts[fromId].id : &accounts[toId].id;
    Account *second = (fromId < toId) ? &accounts[toId].id : &accounts[fromId].id;

    pthread_mutex_lock(&first->lock);
    pthread_mutex_lock(&second->lock);

    // fromId transfere o amount para toId
    if(accounts[fromId].balance >= amount){ // se nao negativar quem envia, faz a operação
      accounts[fromId].balance -= amount;
      accounts[toId].balance += amount;
      accounts[fromId].opsFuncionais++;
      accounts[toId].opsFuncionais++;
      fprintf(accounts[fromId].logFile, "[TRANSF ENVIA]  Para conta %d: -%.2f | Saldo: %.2f\n", toId, amount, accounts[fromId].balance);
      fprintf(accounts[toId].logFile,   "[TRANSF RECEBE] De conta %d:   +%.2f | Saldo: %.2f\n", fromId, amount, accounts[toId].balance);
    }else{
      accounts[fromId].opsDefeituosas++;
      fprintf(accounts[fromId].logFile, "[FALHA TRANSF]  Para conta %d: %.2f | Saldo Insuficiente: %.2f\n", toId, amount, accounts[fromId].balance);
    }

    pthread_mutex_unlock(&second->lock);
    pthread_mutex_unlock(&first->lock);
}
/*
Sobre o deadlock que foi evitado, o que acontece se não checamos quem trancar primeiro:
Se thread 1 quer fazer A->B e thread 2 quer fazer B->A ao mesmo tempo.
Sem a ideia de trancar por Id, o que acontece: thread 1 trancaria A para si, e imediatamente depois a thread 2 trancaria B para si.
Daí a thread1 iria precisar de B, que está com thread 2, e a thread2 precisaria de A, que está com a thread 1. Ambos dormem eternamente.
A solução funciona porque ambos irão olhar pelo menor Id, assim a thread1 trancaria A, depois a thread2 tentaria trancar A, o que não acontece.
(assumindo que o valor da thread A é menor que o de B, só um exemplo).
*/