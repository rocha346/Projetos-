#ifndef COMMON_H
#define COMMON_H

#define MAX_TITLE 200
#define MAX_AUTHORS 200
#define MAX_PATH 64
#define MAX_YEAR 5
#define MAX_KEY 32
#define MAX_KEYWORD 64
#define SERVIDOR "/tmp/dserver_fifo"
#define DATA "documentos.dat"

typedef enum {
    ADICIONAR,
    CONSULTAR,
    APAGAR,
    CONTA_LINHAS_KEYWORD,
    DOCUMENTOS_KEYWORD,
    PARAR_SERVER,
} Operacao;

typedef struct {
    Operacao o;
    char resposta[512];
    union {
        struct {
            char title[MAX_TITLE]; 
            char authors[MAX_AUTHORS];
            char year[MAX_YEAR];
            char path[MAX_PATH];
        } add;

        struct {
            char key[MAX_KEY];
        } consultar, apagar;

        struct {
            char key[MAX_KEY];
            char keyword[MAX_KEYWORD];
        } conta_linhas;

        struct {
            char keyword[MAX_KEYWORD];
            int nr_processos;           //0 -> sem limite, >=0 limite
        } docs_keyword;
    } data;
} Request;

typedef struct {
    int sucesso;            //0 ou 1
    char message[512];  
} Resposta;

typedef struct {
    char key [MAX_KEY];
    char title [MAX_TITLE];
    char authors [MAX_AUTHORS];
    char year [MAX_YEAR];
    char path [MAX_PATH];
    int estado;  // 0 existe, 1 apagado
} Documento;

typedef struct CacheNode{
    Documento d;
    time_t ultimo_doc;
} CacheNode;

typedef struct {
    CacheNode *nodes;
    int tam_max;
    int tam_atual;
} Cache;

#endif