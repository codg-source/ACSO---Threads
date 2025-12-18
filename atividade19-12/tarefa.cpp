#include <iostream>
#include <fstream>
#include <vector>
#include <queue>
#include <algorithm>
#include <iomanip>
#include <string>

using namespace std;

/*
Para rodar o arquivo:
Renomear input de entrada para "Entrada Processos.txt"
Isso por conta da linha de codigo: ifstream in("Entrada Processos.txt");
O recomendável: Criar um arquivo "Entrada Processos.txt" e substituir o conteúdo dele pela entrada desejada.
*/



struct Processo { // processo tem que saber: quem é, quando começou e terminou, quanto tempo ficará na cpu (total e atual), e a sua prioridade.
    int id;
    int chegada;
    int prioridade;
    int cpu_total;
    int cpu_restante;
    int termino;
};


struct Evento { // Tempo de inicio e fim + o que aconteceu nesse período
    string nome;
    int inicio;
    int fim;
};


void salvarTimeline(vector<string> &rawTimeline, ofstream &out) {
    if (rawTimeline.empty()) return;

    string atual = rawTimeline[0];
    int inicio = 0;

    for (size_t i = 1; i < rawTimeline.size(); i++) {
        if (rawTimeline[i] != atual) {
            out << "  [" << inicio << "-" << i << "] " << atual << "\n";
            atual = rawTimeline[i];
            inicio = i;
        }
    }

    out << "  [" << inicio << "-" << rawTimeline.size() << "] " << atual << "\n";
}

// logica do roundrobin aqui:
void roundRobin(vector<Processo> processos, int quantum, int tTroca, ofstream &out) {
    queue<int> fila;
    int tempo = 0;
    int chaveamentos = 0;
    int tempoTrocaTotal = 0;
    vector<string> linhaTempo;

    int n = processos.size();
    vector<bool> entrou(n, false);
    
    // Adiciona processos iniciais (T=0) ordenados por ID se necessário
    for (int i = 0; i < n; i++) {
        if (!entrou[i] && processos[i].chegada <= tempo) {
            fila.push(i);
            entrou[i] = true;
        }
    }

    while (true) {
        if (fila.empty()) {
            // Verifica se todos os processos terminaram
            bool acabou = true;
            for (auto &p : processos) if (p.cpu_restante > 0) acabou = false;
            if (acabou) break;

            // Ocioso
            linhaTempo.push_back("Ocioso");
            tempo++;
            
            // Verifica chegadas durante ocioso
            for (int i = 0; i < n; i++) {
                if (!entrou[i] && processos[i].chegada <= tempo) {
                    fila.push(i);
                    entrou[i] = true;
                }
            }
            continue;
        }

        int idx = fila.front();
        fila.pop();

        // Troca de contexto
        // Regra: Se a timeline não está vazia (não é o início absoluto), paga switch
        if (!linhaTempo.empty()) {
            chaveamentos++;
            for (int i = 0; i < tTroca; i++) {
                linhaTempo.push_back("Escalonador");
                tempo++;
                 // Checagem de chegadas durante a troca
                for (int j = 0; j < n; j++) {
                    if (!entrou[j] && processos[j].chegada <= tempo) {
                        fila.push(j);
                        entrou[j] = true;
                    }
                }
            }
            tempoTrocaTotal += tTroca;
        }

        int exec = min(quantum, processos[idx].cpu_restante);
        
        for (int i = 0; i < exec; i++) {
            linhaTempo.push_back("P" + to_string(processos[idx].id));
            tempo++;
            processos[idx].cpu_restante--;

            // Chegada de novos processos durante execução
            for (int j = 0; j < n; j++) {
                if (!entrou[j] && processos[j].chegada <= tempo) {
                    fila.push(j);
                    entrou[j] = true;
                }
            }
        }

        if (processos[idx].cpu_restante > 0) {
            fila.push(idx); // Round Robin: volta pro fim da fila
        } else {
            processos[idx].termino = tempo;
        }
    }

    // Métricas
    double tempoMedioRetorno = 0;
    for (auto &p : processos)
        tempoMedioRetorno += (p.termino - p.chegada);

    tempoMedioRetorno /= processos.size();
    double overhead = (tempo > 0) ? (double)tempoTrocaTotal / tempo : 0;

    // Saída
    out << "===== ROUND ROBIN =====\n";
    out << "Tempo medio de retorno: " << fixed << setprecision(2) << tempoMedioRetorno << "\n";
    out << "Numero de chaveamentos: " << chaveamentos << "\n";
    out << "Overhead de chaveamento: " << overhead << "\n";
    out << "Tempo total de simulacao: " << tempo << "\n";
    out << "Linha do tempo:\n";
    salvarTimeline(linhaTempo, out); 
    out << "\n";
}

void prioridade(vector<Processo> processos, int tTroca, ofstream &out) {
    int tempo = 0;
    int chaveamentos = 0;
    int tempoTrocaTotal = 0;
    vector<string> linhaTempo;

    int n = processos.size();
    int atual = -1; // Índice do processo que rodou na iteração anterior

    while (true) {
        // 1. Verificar se acabou
        bool todosAcabaram = true;
        for(auto &p : processos) if(p.cpu_restante > 0) todosAcabaram = false;
        if(todosAcabaram) break;

        // 2. Selecionar o melhor candidato
        int escolhido = -1;
        
        // Critério: Menor valor de prioridade = Maior prioridade
        // Desempate: Menor ID
        for (int i = 0; i < n; i++) {
            // Verifica se processo chegou e não acabou
            if (processos[i].chegada <= tempo && processos[i].cpu_restante > 0) {
                if (escolhido == -1) {
                    escolhido = i;
                } else {
                    // Comparação de Prioridade
                    if (processos[i].prioridade < processos[escolhido].prioridade) {
                        escolhido = i;
                    } 
                    // Comparação de ID (Desempate)
                    else if (processos[i].prioridade == processos[escolhido].prioridade) {
                        if (processos[i].id < processos[escolhido].id) {
                            escolhido = i;
                        }
                    }
                }
            }
        }

        // 3. Tratar Ociosidade
        if (escolhido == -1) {
            linhaTempo.push_back("Ocioso");
            tempo++;
            atual = -1; // Ninguém rodou
            continue;
        }

        // 4. Tratar Troca de Contexto 
        // Se mudou o processo E não estamos no início absoluto (timeline vazia)
        if (escolhido != atual) {
             if (!linhaTempo.empty()) {
                chaveamentos++;
                for (int i = 0; i < tTroca; i++) {
                    linhaTempo.push_back("Escalonador");
                    tempo++;
                }
                tempoTrocaTotal += tTroca;
             }
             atual = escolhido;
        }

        // 5. Executar 1 unidade de tempo 
        linhaTempo.push_back("P" + to_string(processos[atual].id));
        tempo++;
        processos[atual].cpu_restante--;

        if (processos[atual].cpu_restante == 0) {
            processos[atual].termino = tempo;
        }
    }

    // Métricas
    double tempoMedioRetorno = 0;
    for (auto &p : processos)
        tempoMedioRetorno += (p.termino - p.chegada);

    tempoMedioRetorno /= processos.size();
    double overhead = (tempo > 0) ? (double)tempoTrocaTotal / tempo : 0;

    out << "===== PRIORIDADE =====\n";
    out << "Tempo medio de retorno: " << fixed << setprecision(2) << tempoMedioRetorno << "\n";
    out << "Numero de chaveamentos: " << chaveamentos << "\n";
    out << "Overhead de chaveamento: " << overhead << "\n";
    out << "Tempo total de simulacao: " << tempo << "\n";
    out << "Linha do tempo:\n";
    salvarTimeline(linhaTempo, out);
    out << "\n";
}

int main() {
    ifstream in("Entrada Processos.txt"); 
    if (!in.is_open()) {
        cout << "Erro ao abrir arquivo!\n";
        return 1;
    }
    ofstream out("saida.txt");

    int nProc, quantum, tTroca;
    char c;

    // Leitura dos parâmetros globais
    in >> nProc >> c >> quantum >> c >> tTroca;

    vector<Processo> processos(nProc);

    // Leitura dos processos
    for (int i = 0; i < nProc; i++) {
        in >> processos[i].id >> c
           >> processos[i].chegada >> c
           >> processos[i].prioridade >> c
           >> processos[i].cpu_total;
        
        processos[i].cpu_restante = processos[i].cpu_total;
    }

    // Faz escalonamento via roundrobin, usando os parametros relevantes.
    roundRobin(processos, quantum, tTroca, out);

    // Reseta para Prioridade
    for (auto &p : processos) {
        p.cpu_restante = p.cpu_total;
        p.termino = 0;
    }

    // Faz escalonamento via prioridade, usando os parametros relevantes.
    prioridade(processos, tTroca, out);

    in.close();
    out.close();

    cout << "Simulacao concluida. Verifique 'saida.txt'.\n";
    return 0;
}
