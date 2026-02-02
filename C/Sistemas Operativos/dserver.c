#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include "common.h"
#include <sys/mman.h>

int desligar = 1;
Cache *cache;
char base_path [MAX_PATH];



Cache *init_cache (int tamanho){
    Cache *c = malloc(sizeof(Cache));
    if (c==NULL){
        perror ("Erro a iniciar a cache");
        exit(EXIT_FAILURE);
    }

    c->tam_max = tamanho;
    c->tam_atual = 0;
    c -> nodes = malloc(sizeof(CacheNode) * tamanho);
    if (c->nodes == NULL) {
        perror("Erro ao alocar memória para os nós do cache");
        free(c);
        exit(EXIT_FAILURE);
    }

    return c;
}

int prox_id (const char *ficheiro_id){
    int fd = open(ficheiro_id,O_RDONLY);
    if (fd == -1){
        perror ("erro ao abrir ficheiro de id");
        return 1;
    }
    char buffer[16] = {0};
    ssize_t l = read(fd,buffer,sizeof(buffer)-1);
    close(fd);

    if (l<=0){ 
        perror ("erro buffer id");
        return 1;
    }
    return atoi(buffer);
}

void guarda_id (const char *ficheiro_id, int novo_id){
    int fd = open (ficheiro_id, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd==-1){
        perror ("erro ao abrir o id");
        return;
    }
    char buffer[16];
    int len = snprintf (buffer, sizeof(buffer), "%d", novo_id);
    write (fd,buffer,len);
    close(fd);
}

void guardar_documento (Documento *d){
    int fd = open (DATA, O_WRONLY | O_APPEND | O_CREAT , 0666);
    if (fd == -1){
        perror ("erro ao abrir o ficheiro data");
        return;
    }
    if (write (fd, d, sizeof (Documento)) == -1){
        perror ("erro ao escrever no ficheiro data");
    }
    close (fd);
}

Documento *procura_cache (const char *key){
    printf ("%d\n", cache->tam_atual);
    for (int i = 0; i<cache->tam_atual; i++){
        if (!cache->nodes[i].d.estado && strcmp (cache->nodes[i].d.key, key) == 0){ //Encontramos na cache
            cache->nodes[i].ultimo_doc = time(NULL);
            return &cache->nodes[i].d;
        }
    }
    return NULL;
}

void insere_cache (Documento *doc){
    if (cache->tam_atual == cache->tam_max){
        int index = 0;
        time_t t = cache->nodes[0].ultimo_doc;

        for (int i = 1; i<cache->tam_max; i++){
            if (cache->nodes[i].ultimo_doc < t){        //Retiramos sempre o documento que nao é usado a mais tempo
                t = cache->nodes[i].ultimo_doc;
                index = i;
            }
        }
        //Aqui já temos definido o documento usado ha mais tempo, falta substituir
        cache->nodes[index].d = *doc;
        cache->nodes[index].ultimo_doc = time(NULL);
    } else{
        cache->nodes[cache->tam_atual].d = *doc;
        cache->nodes[cache->tam_atual].ultimo_doc = time(NULL);

        cache->tam_atual++;
    }
}

int procurar_documento (const char *key , Documento *d){
    Documento *cached = procura_cache(key);
    if (cached != NULL){    //Está em cache
        if (d) *d = *cached;
        return 1;
    }
    //Nao esta em cache, vai ao disco   
    int fd = open (DATA, O_RDONLY);
    if (fd == -1){
        perror ("erro ao abrir o ficheiro data");
        return 0;
    }

    Documento doc;
    while (read (fd, &doc, sizeof(Documento)) == sizeof (Documento)){  //percorre os ficheiros no data file 
        if (!doc.estado && strcmp (doc.key, key) == 0){
            if (d){ *d=doc;  // if(d) para evitar segmentations fault em caso de d ser null
                            // (vai acontecer quando procuro pelo documento para apagar)
            insere_cache(&doc);
            }
            close(fd);
            
            return 1;
        }   
    }
    if (read(fd, &doc, sizeof(Documento)) == -1){
        perror ("erro ao ler o ficheiro data");
    }
    close (fd);
    return 0;        //se chegou aqui não encontrou o documento
}

void apagar_documento (const char *key){
    int fd = open (DATA, O_RDWR);
    if (fd == -1){
        perror ("erro ao abrir o ficheiro data");
        return;
    }

    Documento d;
    while (read(fd,&d,sizeof(Documento)) == sizeof (Documento)){
        if (!d.estado && strcmp (d.key, key) == 0){
            d.estado = 1;
            lseek (fd, -sizeof(Documento), SEEK_CUR); //temos de recuar para o inicio do bloco deste documento
            write (fd, &d, sizeof(Documento));      //reescreve o documento
            break;
        }
    }
    close (fd);
}

//função mais importante, é esta que processa a mensagem do cliente e envia a resposta
void enviar_resposta (Request *req){
    Resposta resp;
    memset (&resp, 0, sizeof(resp));
    resp.sucesso = 1;

    if (req->o == ADICIONAR){
        Documento d;
        memset (&d, 0, sizeof(d));
        
        int id = prox_id("id.txt");
        snprintf (d.key,MAX_KEY,"%d", id);
        guarda_id ("id.txt",id+1);
        
        strncpy (d.title, req->data.add.title, MAX_TITLE);
        strncpy (d.authors, req->data.add.authors, MAX_AUTHORS);
        strncpy (d.year, req->data.add.year, MAX_YEAR);

        char full_path[MAX_PATH];
        snprintf (full_path, MAX_PATH, "%s/%s", base_path, req->data.add.path);
        strncpy (d.path, full_path, MAX_PATH);
        d.estado = 0;

        guardar_documento (&d);
        snprintf (resp.message, sizeof(resp.message), "Documento adicionado com sucesso! Key %s", d.key);

    } else if (req->o == CONSULTAR){
        Documento d;        //vai ser preenchido com o documento com a key
        if (procurar_documento(req->data.consultar.key, &d)){
            snprintf (resp.message, sizeof (resp.message), "Titulo: %s\nAutores: %s\nAno: %s\nPath: %s",
                      d.title,d.authors,d.year,d.path);

        } else {
            resp.sucesso = 0;
            snprintf (resp.message, sizeof(resp.message), "Documento não encontrado");
        }

    } else if (req->o == APAGAR){
        //só é preciso ver se existe um documento com essa chave
        if (procurar_documento(req->data.apagar.key, NULL)){ //procura com null porque nao é preciso reescrever nada
            apagar_documento(req->data.apagar.key);
            snprintf (resp.message, sizeof(resp.message), "Documento apagado com sucesso");
        } else {
            resp.sucesso = 0;
            snprintf (resp.message, sizeof(resp.message), "Não existe nenhum documento com essa chave");
        }

    } else if (req->o == CONTA_LINHAS_KEYWORD){
        Documento d;
        if (procurar_documento(req->data.conta_linhas.key, &d)){
            char path[512];
            snprintf (path, sizeof(path), "%s", d.path);
            int pipefds[2];
            pipe(pipefds);
            int read_fd = pipefds[0];
            int write_fd = pipefds[1];

            //Como o grep é um comando externo, tem que correr num processo separado (cria-se um filho)
            if (fork() == 0){      
                dup2(write_fd, STDOUT_FILENO);      //FILHO
                close (read_fd);
                execlp ("grep", "grep", "-c", req->data.conta_linhas.keyword, path, NULL);
                exit(1);
            } else {
                close (write_fd);
                char buffer [64];
                ssize_t n = read (read_fd, buffer, sizeof(buffer) -1 );
                if (n>=0){
                    buffer[n] = '\0';       //tava a ficar com lixo no fim do read
                } else {
                    buffer[0] = '\0';
                }
                snprintf (resp.message, sizeof(resp.message), "Linhas do documento %s com a keyword %s : %s", req->data.conta_linhas.key, req->data.conta_linhas.keyword, buffer);
                close (read_fd);
            }
        } else{
            resp.sucesso = 0;
            snprintf (resp.message, sizeof(resp.message), "Documento não encontrado");
        }

    } else if (req->o == DOCUMENTOS_KEYWORD){
        int fd = open (DATA, O_RDONLY);
        if (fd == -1){
            resp.sucesso = 0;
            snprintf (resp.message, sizeof(resp.message), "erro ao abrir o ficheiro data");
        } else{
            Documento d;
            char r[1024] = "";
            int max = req->data.docs_keyword.nr_processos;

            if (max == 0){
                while (read(fd, &d, sizeof(Documento)) == sizeof (Documento)){
                    if (!d.estado) {
                        char path[512];
                        snprintf (path, sizeof(path), "%s", d.path);

                        int pipefds[2];
                        pipe(pipefds);
                        int read_fd = pipefds[0];
                        int write_fd = pipefds[1];
                        
                        if (fork() == 0){
                            dup2 (write_fd, STDOUT_FILENO);
                            close (read_fd);
                            execlp ("grep", "grep", "-q", req->data.docs_keyword.keyword, path, NULL);       
                            exit(1);                            //"-q" para não mandar nada para o stdout
                                            //não queremos contar dentro dos ficheiros, só queremos saber se existe

                        } else {
                            close (write_fd);
                            int status;
                            wait (&status);
                            //Se o filho terminou normalmente e se o grep encontrou a palavra no documento
                            if (WIFEXITED (status) && WEXITSTATUS (status) == 0){
                                strcat (r, d.key);
                                strcat (r, " ");
                            }
                            close (read_fd);
                        }
                    }
                }
            
            } else{
                int processos_ativos = 0;
                pid_t filhos [1024];
                char keys[1024][MAX_KEY];
                int ix = 0;

                while (read(fd, &d, sizeof(Documento)) == sizeof(Documento)){
                    if (!d.estado){
                        char path[512];
                        snprintf (path,sizeof(path), "%s", d.path);

                        int pipefds[2];
                        pipe (pipefds);
                        int read_fd = pipefds[0];
                        int write_fd = pipefds[1];

                        if (processos_ativos >= max){
                            //temos de esperar que um filho termine

                            int status;
                            pid_t pid_terminou = wait(&status); 
                            //filho terminou;
                            processos_ativos--;

                            for (int i = 0 ; i<ix; i++){
                                if (filhos[i] == pid_terminou && WIFEXITED(status) && WEXITSTATUS(status) == 0){
                                    strcat (r, keys[i]);
                                    strcat (r, " ");
                                    break;
                                }
                            }
                        }

                        pid_t pid = fork();
                        if (pid == 0){
                            dup2(write_fd, STDOUT_FILENO);
                            close (read_fd);
                            execlp ("grep", "grep", "-q", req->data.docs_keyword.keyword, path, NULL);
                            exit(1);
                        } else {
                            close (write_fd);
                            filhos[ix] = pid;
                            strncpy (keys[ix], d.key, MAX_KEY);
                            ix++;
                            processos_ativos++;
                            close(read_fd);
                        }
                    }
                }

                //espera pelo resto dos filhos para não ficar nenhum zombie

                while (processos_ativos > 0){
                    int status;
                    pid_t pid_terminou = wait(&status);
                    processos_ativos--;
                    for (int j = 0 ; j<ix; j++){
                        if (filhos[j] == pid_terminou && WIFEXITED(status) && WEXITSTATUS (status) == 0){
                            strcat (r, keys[j]);
                            strcat (r, " ");
                            break;
                        }
                    }
                }
            }
            close (fd);
            snprintf (resp.message, sizeof(resp.message), "Documentos com a keyword %s : %s", req->data.docs_keyword.keyword, r);
        }
    } else if (req->o == PARAR_SERVER){
        resp.sucesso = 1;
        snprintf (resp.message, sizeof(resp.message), "Servidor a encerrar");
    } else {
        resp.sucesso = 0;
        snprintf (resp.message, sizeof(resp.message), "Operação desconhecida");
    }

    int fd_resp = open (req->resposta, O_WRONLY);
    if (fd_resp == -1){
        perror ("erro a envia a resposta (servidor)");
        return;
    }

    write (fd_resp, &resp, sizeof(resp));   
    close (fd_resp);
}

int main(int argc, char *argv[]){
    unlink (SERVIDOR);      //para não haver erros de já 
    if (argc < 3){
        perror ("dserver path cache_size");
        exit(1);
    }
    if (mkfifo (SERVIDOR, 0666) == -1){
        perror ("erro no mkfifo");
        exit(1);
    }   

    strncpy (base_path, argv[1], MAX_PATH);

    int tam_cache = atoi(argv[2]);
    cache = init_cache (tam_cache);

    printf ("SERVIDOR LIGADO %s\n", SERVIDOR);

    while (desligar != 0){
        int fd_servidor = open (SERVIDOR, O_RDONLY);
        srand (getpid()); 
        if (fd_servidor == -1){           
            perror ("erro no fd servidor");    
            exit(1);
        }

        Request req;
        while (read(fd_servidor, &req, sizeof(req)) > 0){
            
            if (fork() == 0){
                enviar_resposta (&req);
                exit(0);
            }
            
            if (req.o==PARAR_SERVER){
                desligar = 0;
            }

        }
        close (fd_servidor);
        while (wait(NULL) > 0); //esperamos pelos filhos acabarem
    }

    unlink (SERVIDOR);
    free (cache->nodes);
    free (cache);
    printf ("SERVIDOR TERMINADO\n");
    return 0;
}