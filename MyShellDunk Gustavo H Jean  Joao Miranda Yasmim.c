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


// Variável global para armazenar o código de cor do prompt
char cor_prompt[20] = "\033[0m";

// Protótipos das funções internas
int meu_ls(char **args);
int meu_cd(char **args);
int meu_ajuda(char **args);
int meu_sair(char **args);
int meu_cor(char **args);


int executar_simples(char **args, int background);
int executar_pipe(char **cmd1_args, char **cmd2_args, int background);
void reap_zombies(void);

// Estrutura para descrever um comando interno
struct comando_interno {
    char *nome;
    int (*funcao)(char **);
};

// Array com todos os comandos internos disponíveis
struct comando_interno internos[] = {
    {"ls", &meu_ls},
    {"cd", &meu_cd},
    {"ajuda", &meu_ajuda},
    {"sair", &meu_sair},
    {"cor", &meu_cor},
};

// Retorna o número total de comandos internos.
int num_internos() {
    return sizeof(internos) / sizeof(struct comando_interno);
}

// Altera a cor do prompt do shell.
int meu_cor(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "Uso: cor <nome_da_cor>\n");
        fprintf(stderr, "Cores: vermelho, verde, amarelo, azul, magenta, ciano, branco, reset\n");
        return 1;
    }
    if (strcmp(args[1], "vermelho") == 0) strcpy(cor_prompt, "\033[1;31m");
    else if (strcmp(args[1], "verde") == 0) strcpy(cor_prompt, "\033[1;32m");
    else if (strcmp(args[1], "amarelo") == 0) strcpy(cor_prompt, "\033[1;33m");
    else if (strcmp(args[1], "azul") == 0) strcpy(cor_prompt, "\033[1;34m");
    else if (strcmp(args[1], "magenta") == 0) strcpy(cor_prompt, "\033[1;35m");
    else if (strcmp(args[1], "ciano") == 0) strcpy(cor_prompt, "\033[1;36m");
    else if (strcmp(args[1], "branco") == 0) strcpy(cor_prompt, "\033[1;37m");
    else if (strcmp(args[1], "reset") == 0) strcpy(cor_prompt, "\033[0m");
    else fprintf(stderr, "Cor '%s' nao reconhecida.\n", args[1]);
    return 1;
}

// Lista os arquivos de um diretório (implementação interna do 'ls').
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

// Altera o diretório atual do shell (comando 'cd').
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

// Exibe informações de ajuda sobre os comandos internos.
int meu_ajuda(char **args) {
    printf("Meu Shell Personalizado\n");
    printf("Digite o nome do programa e argumentos, e pressione enter.\n");
    printf("Os seguintes comandos são internos:\n");
    for (int i = 0; i < num_internos(); i++) {
        printf("  %s\n", internos[i].nome);
    }
    printf("\nComandos avançados:\n");
    printf("  comando1 | comando2 : Executa um pipe.\n");
    printf("  comando < arquivo   : Redireciona a entrada.\n");
    printf("  comando > arquivo   : Redireciona a saída (sobrescreve).\n");
    printf("  comando >> arquivo  : Redireciona a saída (anexa).\n");
    printf("  comando &         : Executa em background.\n");
    printf("\nUse 'man <comando>' para ajuda sobre programas externos.\n");
    return 1;
}

// Finaliza a execução do shell.
int meu_sair(char **args) {
    printf("\033[0m");
    return 0;
}

/**
 * @brief Executa um comando simples (sem pipes).
 * Redirecionamento de I/O (<, >, >>) e execução em background (&).
 */
int executar_simples(char **args, int background) {
    char *inputFile = NULL;
    char *outputFile = NULL;
    int appendMode = 0;
    
    // Array para os argumentos "limpos" (sem <, >, >> e nomes de arquivos)
    // AGORA ISTO FUNCIONA, pois TAM_BUFFER_TOKENS foi definido no topo.
    char *clean_args[TAM_BUFFER_TOKENS];
    int clean_idx = 0;

    // 1. Analisa os argumentos para encontrar redirecionamentos
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "<") == 0) {
            inputFile = args[++i]; // Pega o próximo arg como nome de arquivo
            if (inputFile == NULL) {
                fprintf(stderr, "meu_shell: erro de sintaxe, falta arquivo para <\n");
                return 1;
            }
        } else if (strcmp(args[i], ">") == 0) {
            outputFile = args[++i];
            appendMode = 0; // Modo sobrescrever
            if (outputFile == NULL) {
                fprintf(stderr, "meu_shell: erro de sintaxe, falta arquivo para >\n");
                return 1;
            }
        } else if (strcmp(args[i], ">>") == 0) {
            outputFile = args[++i];
            appendMode = 1; // Modo anexar
            if (outputFile == NULL) {
                fprintf(stderr, "meu_shell: erro de sintaxe, falta arquivo para >>\n");
                return 1;
            }
        } else {
            // Se não for um token de redirect, é um argumento de comando
            clean_args[clean_idx++] = args[i];
        }
    }
    clean_args[clean_idx] = NULL; // Termina o array de args limpos

    // Se não houver nenhum comando (ex: apenas "> out.txt"), retorna erro.
    if (clean_args[0] == NULL) {
         fprintf(stderr, "meu_shell: comando inválido\n");
         return 1;
    }

    // 2. Fork e Exec
    // Pega a lógica de debug do 'executar' original
    printf("\n--- [INÍCIO DO TESTE FORK/EXEC (SIMPLES)] ---\n");
    printf("[PAI] Meu PID é %d. Prestes a criar um filho com fork().\n", getpid());

    pid_t pid = fork();
    int status;

    if (pid < 0) {
        perror("meu_shell (fork)");
    } else if (pid == 0) { // Processo Filho
        printf("[FILHO] Olá! Meu PID é %d e o PID do meu pai é %d.\n", getpid(), getppid());
        
        // Se inputFile foi especificado, abre-o e duplica-o para STDIN
        if (inputFile) {
            int fd_in = open(inputFile, O_RDONLY);
            if (fd_in < 0) {
                perror("meu_shell (open input)");
                exit(EXIT_FAILURE);
            }
            // dup2 "copia" o descritor de arquivo fd_in para o STDIN_FILENO (0)
            if (dup2(fd_in, STDIN_FILENO) < 0) {
                perror("meu_shell (dup2 input)");
                exit(EXIT_FAILURE);
            }
            close(fd_in);
        }
        
        // Se outputFile foi especificado, abre-o e duplica-o para STDOUT
        if (outputFile) {
            // Define as flags: Escrita, Criação, e (Anexar ou Truncar)
            int flags = O_WRONLY | O_CREAT;
            flags |= (appendMode) ? O_APPEND : O_TRUNC;
            // 0644 = permissões rw-r--r--
            int fd_out = open(outputFile, flags, 0644);
            if (fd_out < 0) {
                perror("meu_shell (open output)");
                exit(EXIT_FAILURE);
            }
            // dup2 "copia" o descritor de arquivo fd_out para o STDOUT_FILENO (1)
            if (dup2(fd_out, STDOUT_FILENO) < 0) {
                 perror("meu_shell (dup2 output)");
                 exit(EXIT_FAILURE);
            }
            close(fd_out);
        }

        printf("[FILHO] Agora vou me substituir pelo comando '%s' usando execvp().\n", clean_args[0]);
        if (execvp(clean_args[0], clean_args) == -1) {
            perror("meu_shell (execvp)");
        }
        // execvp SÓ retorna se der erro
        exit(EXIT_FAILURE); 

    } else { // Processo Pai
        printf("[PAI] Criei um filho com PID %d.\n", pid);
        if (background) {
            printf("[PAI] Este processo rodará em background. Não vou esperar.\n");
        } else {
            printf("[PAI] Vou esperar (waitpid) pelo filho terminar.\n");
            waitpid(pid, &status, 0);
            printf("[PAI] Meu filho (PID %d) terminou.\n", pid);
        }
        printf("--- [FIM DO TESTE FORK/EXEC (SIMPLES)] ---\n\n");
    }
    return 1;
}

/**
 * @brief: Executa dois comandos conectados por um pipe (cmd1 | cmd2).
 * Lida com execução em background (&) para o pipe inteiro.
 */
int executar_pipe(char **cmd1_args, char **cmd2_args, int background) {
    int pipe_fd[2]; // pipe_fd[0] = leitura, pipe_fd[1] = escrita
    pid_t pid1, pid2;
    int status;

    printf("\n--- [INÍCIO DO TESTE FORK/EXEC (PIPE)] ---\n");
    
    // 1. Cria o pipe
    if (pipe(pipe_fd) < 0) {
        perror("meu_shell (pipe)");
        return 1;
    }

    // --- 2. Filho 1 (Comando 1: escreve no pipe) ---
    pid1 = fork();
    if (pid1 < 0) {
        perror("meu_shell (fork 1)");
        return 1;
    }

    if (pid1 == 0) { // Filho 1 (cmd1)
        printf("[FILHO 1 - %d] Vou executar '%s'. Minha saída vai para o pipe.\n", getpid(), cmd1_args[0]);
        
        // Redireciona stdout (1) para a escrita do pipe (pipe_fd[1])
        dup2(pipe_fd[1], STDOUT_FILENO);
        
        // Fecha os dois lados do pipe no filho 1
        close(pipe_fd[0]); // Fecha a leitura (não usada)
        close(pipe_fd[1]); // Fecha a escrita (já duplicada)
        
        if (execvp(cmd1_args[0], cmd1_args) == -1) {
            perror("meu_shell (execvp 1)");
            exit(EXIT_FAILURE);
        }
    }

    // --- 3. Filho 2 (Comando 2: lê do pipe) ---
    pid2 = fork();
    if (pid2 < 0) {
        perror("meu_shell (fork 2)");
        return 1;
    }

    if (pid2 == 0) { // Filho 2 (cmd2)
        printf("[FILHO 2 - %d] Vou executar '%s'. Minha entrada vem do pipe.\n", getpid(), cmd2_args[0]);
        
        // Redireciona stdin (0) para a leitura do pipe (pipe_fd[0])
        dup2(pipe_fd[0], STDIN_FILENO); 
        
        // Fecha os dois lados do pipe no filho 2
        close(pipe_fd[0]); // Fecha a leitura (já duplicada)
        close(pipe_fd[1]); // Fecha a escrita (não usada)

        if (execvp(cmd2_args[0], cmd2_args) == -1) {
            perror("meu_shell (execvp 2)");
            exit(EXIT_FAILURE);
        }
    }

    // --- 4. Processo Pai ---
    // O pai DEVE fechar AMBOS os lados do pipe,
    // senão os filhos podem travar (ex: 'ls | sort' trava se 'ls' for pequeno)
    close(pipe_fd[0]);
    close(pipe_fd[1]);

    printf("[PAI] Criei dois filhos: PID %d (cmd1) e PID %d (cmd2).\n", pid1, pid2);

    if (background) {
        printf("[PAI] O pipe rodará em background. Não vou esperar.\n");
    } else {
        printf("[PAI] Vou esperar (waitpid) por ambos os filhos.\n");
        waitpid(pid1, &status, 0); // Espera pelo filho 1
        waitpid(pid2, &status, 0); // Espera pelo filho 2
        printf("[PAI] Ambos os filhos terminaram.\n");
    }
    printf("--- [FIM DO TESTE FORK/EXEC (PIPE)] ---\n\n");
    
    return 1;
}


/**
 * @brief Função "despachante".
 * Verifica se é comando interno.
 * Se não, procura por pipes (|) e background (&).
 * Chama executar_simples() ou executar_pipe() conforme necessário.
 */
int executar(char **args) {
    if (args[0] == NULL) {
        return 1; // Comando vazio
    }

    // 1. Checar comandos internos
    for (int i = 0; i < num_internos(); i++) {
        if (strcmp(args[0], internos[i].nome) == 0) {
            return internos[i].funcao(args);
        }
    }

    // 2. Analisar o comando para pipe, background
    int background = 0;
    char **cmd_pipe = NULL; // Ponteiro para o início do segundo comando (após o |)
    
    int i = 0;
    while (args[i] != NULL) {
        // Checar por Pipe
        if (strcmp(args[i], "|") == 0) {
            args[i] = NULL; // Divide o comando, trocando "|" por NULL
            cmd_pipe = &args[i + 1]; // O próximo argumento é o início do cmd 2
            
            if (cmd_pipe[0] == NULL) {
                fprintf(stderr, "meu_shell: erro de sintaxe, pipe sem comando de destino\n");
                return 1;
            }
            break; // Por simplicidade, suporta apenas um pipe
        }
        i++;
    }

    // Checar por Background (&) no final
    // 'i' agora é o índice do argumento final (NULL) do *primeiro* comando se houver pipe,
    // ou do comando inteiro se não houver pipe.

    char **comando_a_verificar_bg = cmd_pipe ? cmd_pipe : args;
    int j = 0;
    while (comando_a_verificar_bg[j] != NULL) {
        j++;
    }

    // Se o último argumento (índice j-1) for "&"
    if (j > 0 && strcmp(comando_a_verificar_bg[j - 1], "&") == 0) {
        background = 1;
        comando_a_verificar_bg[j - 1] = NULL; // Remove o "&" dos argumentos
    }

    // 3. Chamar a função de execução apropriada
    if (cmd_pipe) {
        return executar_pipe(args, cmd_pipe, background);
    } else {
        return executar_simples(args, background);
    }
}


// Lê uma linha de entrada digitada pelo usuário.
// O #define TAM_BUFFER_LINHA foi movido para o topo do ficheiro.
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

// Divide uma linha de texto em um array de argumentos (tokens).
// Os #defines TAM_BUFFER_TOKENS e DELIMITADORES
// foram movidos para o topo do ficheiro.
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

/**
 * @brief : Coleta processos "zumbi" em background.
 * waitpid() com WNOHANG não bloqueia, e retorna > 0
 * se um filho (qualquer, -1) tiver terminado.
 */
void reap_zombies(void) {
    int status;
    pid_t pid;
    
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // Usamos printf direto ao invés de fprintf(stderr) para não
        // poluir o prompt em operações normais.
        printf("[MeuShell] Processo em background (PID %d) terminado.\n", pid);
    }
}


/**
 * @brief Implementa o loop principal de leitura e execução de comandos.
 * 
 */
void loop_shell(void) {
    char *linha;
    char **args;
    int status;

    do {
        // NOVO: Coleta zumbis antes de mostrar o prompt
        reap_zombies();

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

// Função principal que inicia o shell.
int main(int argc, char **argv) {
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