#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h> 
#include <fcntl.h>    

#define TAM_BUFFER_LINHA 1024
#define TAM_BUFFER_TOKENS 64
#define DELIMITADORES " \t\r\n\a"

// guarda a cor atual do prompt Assim fica bonito XD
char cor_prompt[20] = "\033[0m";

// funções dos comandos internos UwU
int meu_ls(char **args);
int meu_cd(char **args);
int meu_ajuda(char **args);
int meu_sair(char **args);
int meu_cor(char **args);

// UwU funções responsáveis por executar comandos UwU
int executar_simples(char **args, int background);
int executar_pipe(char **cmd1_args, char **cmd2_args, int background);
void reap_zombies(void);

// estrutura de cada comando interno UwU UwU UwU UwU UwU UwU UwU
struct comando_interno {
    char *nome;
    int (*funcao)(char **);
};

// lista de comandos internos disponíveis
struct comando_interno internos[] = {
    {"ls",    &meu_ls},
    {"cd",    &meu_cd},
    {"ajuda", &meu_ajuda},
    {"help",  &meu_ajuda},   //agora ele é bilingue :) 
    {"sair",  &meu_sair},
    {"exit",  &meu_sair},   
    {"cor",   &meu_cor},
};

// retorna quantos comandos internos existem
int num_internos() {
    return sizeof(internos) / sizeof(struct comando_interno);
}

// muda a cor do prompt usando nomes de cores simples Pra ficar bonitinho OwO
int meu_cor(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "Uso: cor <nome_da_cor>\n");
        fprintf(stderr, "Cores: vermelho, verde, amarelo, azul, magenta, ciano, branco, reset\n");
        return 1;
    }

    if (strcmp(args[1], "vermelho") == 0)      strcpy(cor_prompt, "\033[1;31m");
    else if (strcmp(args[1], "verde") == 0)    strcpy(cor_prompt, "\033[1;32m");
    else if (strcmp(args[1], "amarelo") == 0)  strcpy(cor_prompt, "\033[1;33m");
    else if (strcmp(args[1], "azul") == 0)     strcpy(cor_prompt, "\033[1;34m");
    else if (strcmp(args[1], "magenta") == 0)  strcpy(cor_prompt, "\033[1;35m");
    else if (strcmp(args[1], "ciano") == 0)    strcpy(cor_prompt, "\033[1;36m");
    else if (strcmp(args[1], "branco") == 0)   strcpy(cor_prompt, "\033[1;37m");
    else if (strcmp(args[1], "reset") == 0)    strcpy(cor_prompt, "\033[0m");
    else                                       fprintf(stderr, "Cor '%s' nao reconhecida.\n", args[1]);

    return 1;
}

// implementação simples do ls (lista arquivos da pasta) UwU
int meu_ls(char **args) {
    DIR *d;
    struct dirent *dir;
    char *diretorio_alvo = (args[1] == NULL) ? "." : args[1];

    d = opendir(diretorio_alvo);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_name[0] != '.') {
                printf("%s\n", dir->d_name);
            }
        }
        closedir(d);
    } else {
        perror("meu_shell (ls)");
    }
    return 1;
}

// comando cd: muda o diretório atual
int meu_cd(char **args) {
    if (args[1] == NULL) {
        const char* home_dir = getenv("HOME");
        if (home_dir == NULL) {
            fprintf(stderr, "meu_shell: cd: HOME não definido\n");
        } else if (chdir(home_dir) != 0) {
            perror("meu_shell");
        }
    } else if (chdir(args[1]) != 0) {
        perror("meu_shell");
    }
    return 1;
}

// mostra uma ajuda básica sobre o shell e os comandos internos
int meu_ajuda(char **args) {
    (void)args; 

    printf("Meu Shell Personalizado (MyShell)\n");
    printf("Digite o nome do programa e argumentos, e pressione enter.\n\n");
    printf("Comandos internos:\n");
    for (int i = 0; i < num_internos(); i++) {
        printf("  %s\n", internos[i].nome);
    }
    printf("\nFuncionalidades suportadas:\n");
    printf("  comando1 | comando2   : pipe simples entre dois comandos.\n");
    printf("  comando < arquivo     : redireciona a entrada padrão.\n");
    printf("  comando > arquivo     : redireciona a saída (sobrescreve).\n");
    printf("  comando >> arquivo    : redireciona a saída (anexa).\n");
    printf("  comando &             : executa em background.\n");
    printf("\nUse 'man <comando>' para ajuda sobre programas externos.\n");
    return 1;
}

// comando para sair do shell
int meu_sair(char **args) {
    (void)args; // não usado
    printf("\033[0m");
    return 0;   // faz o loop do shell parar
}

// executa um comando simples (sem pipe), com redirecionamento e background UwU
int executar_simples(char **args, int background) {
    char *inputFile = NULL;
    char *outputFile = NULL;
    int appendMode = 0;
    
    char *clean_args[TAM_BUFFER_TOKENS];
    int clean_idx = 0;

    // separa os redirecionamentos dos argumentos reais do comando
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "<") == 0) {
            inputFile = args[++i];
            if (inputFile == NULL) {
                fprintf(stderr, "meu_shell: erro de sintaxe, falta arquivo para <\n");
                return 1;
            }
        } else if (strcmp(args[i], ">") == 0) {
            outputFile = args[++i];
            appendMode = 0;
            if (outputFile == NULL) {
                fprintf(stderr, "meu_shell: erro de sintaxe, falta arquivo para >\n");
                return 1;
            }
        } else if (strcmp(args[i], ">>") == 0) {
            outputFile = args[++i];
            appendMode = 1;
            if (outputFile == NULL) {
                fprintf(stderr, "meu_shell: erro de sintaxe, falta arquivo para >>\n");
                return 1;
            }
        } else {
            clean_args[clean_idx++] = args[i];
        }
    }
    clean_args[clean_idx] = NULL;

    if (clean_args[0] == NULL) {
        fprintf(stderr, "meu_shell: comando inválido\n");
        return 1;
    }

    pid_t pid = fork();
    int status;

    if (pid < 0) {
        perror("meu_shell (fork)");
    } else if (pid == 0) {
        // parte do filho: cuida de redirecionamento e depois executa o comando UwU
        if (inputFile) {
            int fd_in = open(inputFile, O_RDONLY);
            if (fd_in < 0) {
                perror("meu_shell (open input)");
                exit(EXIT_FAILURE);
            }
            if (dup2(fd_in, STDIN_FILENO) < 0) {
                perror("meu_shell (dup2 input)");
                exit(EXIT_FAILURE);
            }
            close(fd_in);
        }
        
        if (outputFile) {
            int flags = O_WRONLY | O_CREAT;
            flags |= (appendMode) ? O_APPEND : O_TRUNC;
            int fd_out = open(outputFile, flags, 0644);
            if (fd_out < 0) {
                perror("meu_shell (open output)");
                exit(EXIT_FAILURE);
            }
            if (dup2(fd_out, STDOUT_FILENO) < 0) {
                perror("meu_shell (dup2 output)");
                exit(EXIT_FAILURE);
            }
            close(fd_out);
        }

        if (execvp(clean_args[0], clean_args) == -1) {
            perror("meu_shell (execvp)");
        }
        exit(EXIT_FAILURE);

    } else {
        // parte do pai: só espera se não for background UwU
        if (!background) {
            waitpid(pid, &status, 0);
        }
    }
    return 1;
}

// executa dois comandos ligados com pipe: cmd1 | cmd2 UwU
int executar_pipe(char **cmd1_args, char **cmd2_args, int background) {
    int pipe_fd[2];
    pid_t pid1, pid2;
    int status;

    if (pipe(pipe_fd) < 0) {
        perror("meu_shell (pipe)");
        return 1;
    }

    // primeiro filho: escreve no pipe
    pid1 = fork();
    if (pid1 < 0) {
        perror("meu_shell (fork 1)");
        return 1;
    }

    if (pid1 == 0) {
        dup2(pipe_fd[1], STDOUT_FILENO);
        close(pipe_fd[0]);
        close(pipe_fd[1]);

        if (execvp(cmd1_args[0], cmd1_args) == -1) {
            perror("meu_shell (execvp 1)");
            exit(EXIT_FAILURE);
        }
    }

    // segundo filho: lê do pipe
    pid2 = fork();
    if (pid2 < 0) {
        perror("meu_shell (fork 2)");
        return 1;
    }

    if (pid2 == 0) {
        dup2(pipe_fd[0], STDIN_FILENO);
        close(pipe_fd[0]);
        close(pipe_fd[1]);

        if (execvp(cmd2_args[0], cmd2_args) == -1) {
            perror("meu_shell (execvp 2)");
            exit(EXIT_FAILURE);
        }
    }

    // pai fecha o pipe
    close(pipe_fd[0]);
    close(pipe_fd[1]);

    if (!background) {
        waitpid(pid1, &status, 0);
        waitpid(pid2, &status, 0);
    }

    return 1;
}

// decide se é comando interno, com pipe, background etc. e despacha
int executar(char **args) {
    if (args[0] == NULL) {
        return 1; // comando vazio
    }

    // verifica se é algum comando interno
    for (int i = 0; i < num_internos(); i++) {
        if (strcmp(args[0], internos[i].nome) == 0) {
            return internos[i].funcao(args);
        }
    }

    int background = 0;
    char **cmd_pipe = NULL;
    
    int i = 0;
    // procura um pipe no comando UwU UwU
    while (args[i] != NULL) {
        if (strcmp(args[i], "|") == 0) {
            args[i] = NULL;
            cmd_pipe = &args[i + 1];
            if (cmd_pipe[0] == NULL) {
                fprintf(stderr, "meu_shell: erro de sintaxe, pipe sem comando de destino\n");
                return 1;
            }
            break;
        }
        i++;
    }

    // UwU
    char **comando_a_verificar_bg = cmd_pipe ? cmd_pipe : args;
    int j = 0;
    while (comando_a_verificar_bg[j] != NULL) {
        j++;
    }

    if (j > 0) {
        char *ultimo = comando_a_verificar_bg[j - 1];

        // caso: último token é só "&"
        if (strcmp(ultimo, "&") == 0) {
            background = 1;
            comando_a_verificar_bg[j - 1] = NULL;
        } else {
            // caso: algo tipo "sleep&"
            size_t len = strlen(ultimo);
            if (len > 0 && ultimo[len - 1] == '&') {
                background = 1;
                ultimo[len - 1] = '\0';
                if (ultimo[0] == '\0') {
                    comando_a_verificar_bg[j - 1] = NULL;
                }
            }
        }
    }

    // escolhe se vai chamar pipe ou simples
    if (cmd_pipe) {
        return executar_pipe(args, cmd_pipe, background);
    } else {
        return executar_simples(args, background);
    }
}

// lê uma linha digitada pelo usuário
char *ler_linha(void) {
    int bufsize = TAM_BUFFER_LINHA;
    int position = 0;
    char *buffer = malloc(sizeof(char) * bufsize);
    int c;

    if (!buffer) {
        fprintf(stderr, "meu_shell: erro de alocação\n");
        exit(EXIT_FAILURE);
    }

    while (1) {
        c = getchar();
        if (c == EOF || c == '\n') {
            buffer[position] = '\0';
            return buffer;
        } else {
            buffer[position] = c;
        }
        position++;

        if (position >= bufsize) {
            bufsize += TAM_BUFFER_LINHA;
            buffer = realloc(buffer, bufsize);
            if (!buffer) {
                fprintf(stderr, "meu_shell: erro de alocação\n");
                exit(EXIT_FAILURE);
            }
        }
    }
}

// quebra a linha em tokens (argumentos) usando strtokOwO
char **dividir_linha(char *linha) {
    int bufsize = TAM_BUFFER_TOKENS;
    int position = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    char *token;

    if (!tokens) {
        fprintf(stderr, "meu_shell: erro de alocação\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(linha, DELIMITADORES);
    while (token != NULL) {
        tokens[position] = token;
        position++;

        if (position >= bufsize) {
            bufsize += TAM_BUFFER_TOKENS;
            tokens = realloc(tokens, bufsize * sizeof(char*));
            if (!tokens) {
                fprintf(stderr, "meu_shell: erro de alocação\n");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, DELIMITADORES);
    }
    tokens[position] = NULL;
    return tokens;
}

// coleta processos zumbis de background OwO
void reap_zombies(void) {
    int status;
    pid_t pid;
    
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("[MeuShell] Processo em background (PID %d) terminado.\n", pid);
    }
}

// laço principal do shell: mostra o prompt, lê e executa comandos OwO
void loop_shell(void) {
    char *linha;
    char **args;
    int status;

    do {
        reap_zombies(); // limpa processos em background finalizados

        printf("%s", cor_prompt);
        printf("Dunk > ");
        printf("\033[0m");
        
        linha = ler_linha();
        args = dividir_linha(linha);
        status = executar(args);

        free(linha);
        free(args);
    } while (status);
}

// ponto de entrada: mostra o banner e inicia o loop do shell OwO
int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
// nosso tutubarao OwO
    printf(" _________         .    .          By Jean, Yasmim, Gustavo e JP Miranda\n"
           "(..       \\_     ,  |\\  /|\n"
           " \\         O \\   /|  \\ \\/ /\n"
           "  \\______    \\/ |   \\  / \n"
           "     vvvv\\    \\ |   /   |\n"
           "     \\^^^^  ==  \\_/   |\n"
           "      `\\_   ===    \\.  |\n"
           "      / /\\_   \\ /      |\n"
           "      |/   \\_  \\|      /\n"
           "             \\________/\n");

    loop_shell();
    return EXIT_SUCCESS;
}
