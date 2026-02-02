#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "common.h"

void help(){
    printf ("Usage:\n");
    printf ("Indexa um novo documento: ./dclient -a [nome_doc] [autores] [ano] [path]\n");
    printf ("Consulta a meta-informação de um documento: ./dclient -c [id_doc]\n");
    printf ("Remove a meta-informação de um documento: ./dclient -d [id_doc]\n");
    printf ("Devolve o número de linhas de um documento que contém uma keyword: ./dclient -l [id_doc] [keyword]\n");
    printf ("Devolve o id dos documentos que contém uma keyword: ./dclient -s [keyword]\n");
    printf ("Desligar servidor: ./dclient -f\n");
    exit(1);
}

int main (int argc, char *argv[]){
    if (argc < 2) help();

    Request req;
    memset (&req, 0, sizeof(req));      //Garante que a struct começa limpa

    pid_t pid = getpid();
    char resposta[256];
    snprintf (resposta, sizeof(resposta), "/tmp/client_%d_fifo", pid);  //Cada cliente tem o seu canal para responder

    if (mkfifo (resposta,0666) == -1){
        perror ("mkfifo client erro");
        exit(1);
    }

    strncpy (req.resposta, resposta, sizeof(req.resposta));

    if (strcmp (argv[1], "-a") == 0 && argc == 6 ){
        req.o = ADICIONAR;
        strncpy (req.data.add.title, argv[2], MAX_TITLE);
        strncpy (req.data.add.authors, argv[3], MAX_AUTHORS);
        strncpy (req.data.add.year, argv[4], MAX_YEAR);
        strncpy (req.data.add.path, argv[5], MAX_PATH);

    } else if (strcmp (argv[1], "-c") == 0 && argc == 3){
        req.o = CONSULTAR;
        strncpy (req.data.consultar.key , argv[2], MAX_KEY);

    } else if (strcmp (argv[1], "-d") == 0 && argc == 3){
        req.o = APAGAR;
        strncpy (req.data.apagar.key, argv[2], MAX_KEY);

    } else if (strcmp (argv[1], "-l") == 0 && argc == 4){
        req.o = CONTA_LINHAS_KEYWORD;
        strncpy (req.data.conta_linhas.key, argv[2], MAX_KEY);
        strncpy (req.data.conta_linhas.keyword, argv[3], MAX_KEYWORD);

    } else if (strcmp (argv[1], "-s") == 0 && (argc == 3 || argc == 4)){
        req.o = DOCUMENTOS_KEYWORD;
        strncpy (req.data.docs_keyword.keyword, argv[2], MAX_KEYWORD);

        if (argc == 4){ 
            req.data.docs_keyword.nr_processos = atoi(argv[3]);
        } else {
            req.data.docs_keyword.nr_processos = 0;
        }
    
    } else if (strcmp (argv[1], "-f") == 0 && argc ==2){
        req.o = PARAR_SERVER;
    } else {
        help();
    }
    
    int fifo = open (SERVIDOR, O_WRONLY);

    if (fifo == -1){
        perror ("erro ao conectar ao servidor");
        unlink (resposta);
        exit(1);
    }

    write (fifo, &req, sizeof(req));
    close (fifo);       // Mandamos o pedido e fechamos o canal

    int fifo_resp = open (resposta, O_RDONLY);
    if (fifo_resp == -1){
        perror ("erro a receber a resposta");
        unlink (resposta);
        exit(1);
    }

    Resposta resp;
    if (read (fifo_resp, &resp, sizeof(resp)) > 0) {
        printf ("%s\n", resp.message);
    } else {
        printf ("Não recebemos resposta\n");
    }

    close (fifo_resp);
    unlink (resposta);

    return 0; 
}

