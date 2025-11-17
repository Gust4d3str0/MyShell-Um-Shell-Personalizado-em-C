# MyShell — Shell Personalizado em C

Este projeto implementa um **mini interpretador de comandos (shell)** programado em C, capaz de executar comandos internos, rodar programas externos, manipular diretórios, colorir o prompt, lidar com redirecionamento de entrada/saída, criar pipelines com `|`, e até executar processos em *background*.

O programa também demonstra, de forma didática, o funcionamento de chamadas de sistema importantes de Linux como:

- `fork()`
- `execvp()`
- `waitpid()`
- `pipe()`
- `dup2()`
- `open()`
- `chdir()`
- Manipulação de diretórios (`opendir`, `readdir`)
- Coleta de processos zumbis

---

## ✨ Funcionalidades Principais

### 🔹 **Comandos internos implementados**
O shell reconhece e executa internamente:

| Comando | Função |
|--------|--------|
| `ls` | Lista arquivos do diretório usando `opendir/readdir` |
| `cd` | Altera o diretório atual |
| `ajuda` | Exibe instruções sobre o shell |
| `sair` | Encerra o shell |
| `cor` | Altera a cor do prompt (vermelho, verde, azul etc.) |

---

## 🔹 Execução de comandos externos

Caso o usuário digite um comando que não seja interno, o shell:

1. Cria um filho com `fork()`
2. No filho, executa o programa com `execvp()`
3. O pai espera o término com `waitpid()`, a menos que o comando seja executado em background

---

## 🔹 Suporte a pipelines (`|`)

O shell suporta um **pipe simples**, como:

```
ls | sort
```

Implementado com:

- `pipe()`
- Dois `fork()`
- Redirecionamento via `dup2()`

---

## 🔹 Redirecionamento de entrada e saída

O shell implementa:

| Sintaxe | Descrição |
|--------|-----------|
| `comando < arquivo` | Lê entrada do arquivo |
| `comando > arquivo` | Sobrescreve arquivo |
| `comando >> arquivo` | Anexa ao final do arquivo |

Exemplo:

```
cat < entrada.txt > saida.txt
```

---

## 🔹 Execução em background (`&`)

Exemplo:

```
sleep 5 &
```

O pai **não espera** pelo filho.

O shell também possui coleta automática de zumbis:

```c
waitpid(-1, &status, WNOHANG)
```

---

## 🔹 Prompt personalizável

O usuário pode mudar a cor do prompt:

```
cor verde
cor azul
cor vermelho
cor reset
```

Usando códigos ANSI armazenados em uma variável global.

---

## 🔹 Estrutura Geral do Shell

O shell segue o clássico modelo:

### 1. **loop_shell()**
- mostra o prompt
- lê linha (`ler_linha`)
- divide em tokens (`dividir_linha`)
- executa (`executar`)
- repete

### 2. **executar()**
- identifica comandos internos
- detecta pipe (`|`)
- detecta background (`&`)
- chama:
  - `executar_simples()`
  - ou `executar_pipe()`

### 3. **executar_simples()**
- trata redirecionamento `<`, `>`, `>>`
- cria processo com `fork()`
- chama `execvp()`

### 4. **executar_pipe()**
- cria pipe
- cria dois filhos
- conecta saída → entrada

---

## 🧠 Conceitos de Sistemas Operacionais Aplicados

O código demonstra conceitos importantes:

- **Processos e criação de processos**
- **Execução de programas externos**
- **Descritores de arquivos**
- **Herança de descritores em fork**
- **Comunicação entre processos com pipe**
- **Tratamento de processos zumbis**
- **Manipulação de diretórios**
- **Redirecionamento de I/O no Linux**

---

## 🧪 Como Compilar

```
gcc MyShell2.c -o myshell
```

## ▶️ Como Executar

```
./myshell
```

Você verá o ASCII art de boas-vindas e o prompt:

```
Dunk >
```

---

## 👥 Autores

- Jean  
- Yasmim  
- Gustavo  
- JP  
