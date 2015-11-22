#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>

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

char* smsh_redr_in(char** args) {
  FILE* file;
  char* line;
  char* path;
  char buff[255];

  path = malloc(sizeof(char)*255);

  sprintf(path, "./%s", args[2]);
  printf("%s\n", path);
  line = args[0];
  file = fopen(path, "r");
  if(file != NULL) {
    fgets(buff, 255, (FILE*)file);
    strcat(line, buff); 
  }
  else {
    perror("smallsh");
    return NULL;
  }

  free(path);

  return line;
}

int smsh_execute(char** args, int status) {
  char* line;

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
  else if(args[1] && strcmp(args[1], "<") == 0) {
    line = smsh_redr_in(args);
    if(line != NULL) {
       args = smsh_split_line(line);
       return smsh_launch(args);
    }
    return 1;
  }
  //else if(strcmp(args[1], ">") == 0) {
  //}
  //else if(args[1] == '&' || args[2] == '&') {
  //}

  return smsh_launch(args);
}

void smsh_loop(void) {
  char* line;
  char** args;
  int status;

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

