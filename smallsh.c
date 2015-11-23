#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <fcntl.h>

#define SMSH_RL_BUFSIZE 1024
char* smsh_read_line(void) {
  int bufsize = SMSH_RL_BUFSIZE;
  int position = 0;
  char* buffer = malloc(sizeof(char)*bufsize);
  int c;

  //handles buffer allocation error
  if(!buffer) {
    //exits with allocation error
    fprintf(stderr, "smallsh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  while (1) {
    //read char
    c = getchar();

    //if eof has been reached, replace eof char with null char
    if(c == EOF || c == '\n') {
      buffer[position] = '\0';
      return buffer;
    }
    //otherwise, add the character to the buffer
    else {
      buffer[position] = c;
    }
    //increment position
    position++;

    //if the buffer size has been exceeded, reallocate
    if(position >= bufsize) {
      bufsize += SMSH_RL_BUFSIZE;
      buffer = realloc(buffer, bufsize);
      //handles buffer reallocation error
      if(!buffer) {
        //exits with allocation error
        fprintf(stderr, "smallsh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }
  }
}

#define SMSH_TOK_BUFSIZE 64
#define SMSH_TOK_DELIM " \t\r\n\a"
char** smsh_split_line(char* line) {
  int bufsize = SMSH_TOK_BUFSIZE;
  int position = 0;
  char** tokens = malloc(bufsize*sizeof(char*));
  char* token;

  if(!tokens) {
    fprintf(stderr, "smallsh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  token = strtok(line, SMSH_TOK_DELIM);
  while(token != NULL) {
    tokens[position] = token;
    position++;

    if(position >= bufsize) {
      bufsize += SMSH_TOK_BUFSIZE;
      tokens = realloc(tokens, sizeof(char*)*bufsize);
      if(!tokens) {
        fprintf(stderr, "smallsh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }

    token = strtok(NULL, SMSH_TOK_DELIM);
  }
  tokens[position] = NULL;

  return tokens;
}

int smsh_launch_bg(char** args) {
  pid_t pid;
  pid_t wpid;
  int status;

  pid = fork();
  if(pid == 0) {
    //child process
    if(execvp(args[0], args) == -1) {
      perror("smallsh");
      return 1;
    }
    exit(EXIT_FAILURE);
  }
  else if(pid < 0) {
    //error forking
    perror("smallsh");
    return 1;
  }
  else {
    printf("background pid is %d\n", pid);
    fclose(stdout);
    fclose(stdin);
    do {
      wpid = waitpid(pid, &status, WUNTRACED);
    }while(!WIFEXITED(status) && !WIFSIGNALED(status));
  }

  return 0;
}

int smsh_launch(char** args) {
  pid_t pid;
  pid_t wpid;
  int status;

  pid = fork();
  if(pid == 0) {
    //child process
    if(execvp(args[0], args) == -1) {
      perror("smallsh");
      return 1;
    }
    exit(EXIT_FAILURE);
  }
  else if(pid < 0) {
    //error forking
    perror("smallsh");
    return 1;
  }
  else {
    //parent process
    do {
      wpid = waitpid(pid, &status, WUNTRACED);
    }while(!WIFEXITED(status) && !WIFSIGNALED(status));
  }

  return 0;
}

int smsh_cd(char** args) {
  if(args[1] == NULL) {
     //this thing doesn't work
     //TODO fix cd to home
    if(chdir("~") != 0) {
      perror("smallsh");
      return 1;
    }
  }
  else {
    if(chdir(args[1]) != 0) {
      perror("smallsh");
      return 1;
    }
  }
  return 0;
}

int smsh_exit(char** args) {
  return -1;
}

int smsh_status(int status) {
   printf("exit value %d\n", status);
   return 0;
}

int amp(char** args) {
  int i;

  i = 0;

  while(args[i] != NULL) {
    if(strcmp(args[i], "&") == 0) {
      return i;
    }
    i++;
  }
  
  return -1;
}

int smsh_execute(char** args, int status) {
  char* path;
  char* line;
  FILE* fp;
  int exit;
  int bak, new;

  if(args[0] == NULL) {
    //an empty command was entered
    return 0;
  }

  if(strcmp(args[0], "cd") == 0) {
    return (smsh_cd(args));
  }
  else if(strcmp(args[0], "exit") == 0) {
    return (smsh_exit(args));
  }
  else if(strcmp(args[0], "status") ==  0) {
     return (smsh_status(status));
  }
  else if(strcmp(args[0], "#") == 0) {
    return 0;
  }
  else if(args[1] && strcmp(args[1], "<") == 0) {
    path = malloc(sizeof(char)*255);
    sprintf(path, "./%s", args[2]);

    args[1] = path;
    if(args[3] && strcmp(args[3], "&") == 0) {
      args[2] = NULL;
      args[3] = NULL;
      free(path);
      return smsh_launch_bg(args);
    }
    else {
      args[2] = NULL;
    }

    //free malloc stuff
    free(path);

    return smsh_launch(args);

  }
  else if(args[1] && strcmp(args[1], ">") == 0) {
    if(args[2]) {
      fflush(stdout);
      bak = dup(1);
      new = open(args[2], O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
      if(new == -1) {
        perror("open");
      }
      dup2(new, 1);
      close(new);
      args[1] = NULL;
      args[2] = NULL;
      exit = smsh_launch(args);
      fflush(stdout);
      dup2(bak, 1);
      close(bak);

      return exit;
    }
    else {
      perror("smallsh");
      return 1;
    }
  }

  exit = amp(args);
  if(exit != -1) {
    args[exit] = NULL;
    return smsh_launch_bg(args);
  }
  else {
    return smsh_launch(args);
  }
}

int kill_zombies(int bg) {
  pid_t pid, wpid;
  int status;

  pid = bg;
  do {
    wpid = waitpid(pid, &status, WUNTRACED);
  }while(!WIFEXITED(status) && !WIFSIGNALED(status));

  return status;
}

void smsh_loop(void) {
  char* line;
  char** args;
  int* bg;
  int i, status, exit;

  bg=malloc(sizeof(int)*5);
  for(i=0; i<5; i++) {
    bg[i] = 0;
  }


  do {
    printf(": ");
    line = smsh_read_line();
    args = smsh_split_line(line);
    status = smsh_execute(args, status);

    free(line);
    free(args);
  }while(status != -1);
}

int main(int argc, char** argv) {
  //run the prompt loop
  smsh_loop();

  //exit
  return EXIT_SUCCESS;
}

