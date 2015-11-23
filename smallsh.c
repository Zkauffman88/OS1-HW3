#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>

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

  //loop until the eof is reached or it fails to allocate more buffer space
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

  //error if tokens were not allocated
  if(!tokens) {
    fprintf(stderr, "smallsh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  //split line on delims
  token = strtok(line, SMSH_TOK_DELIM);
  //loop until there are no more tokens
  while(token != NULL) {
    //store the tokens (these will be "args" for the rest of the program
    tokens[position] = token;
    position++;

    //reallocate more space for tokens if needed
    if(position >= bufsize) {
      bufsize += SMSH_TOK_BUFSIZE;
      tokens = realloc(tokens, sizeof(char*)*bufsize);
      //error if allocation fails
      if(!tokens) {
        fprintf(stderr, "smallsh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }

    //set tokens to NULL
    token = strtok(NULL, SMSH_TOK_DELIM);
  }
  //NULL terminate the tokens array
  tokens[position] = NULL;

  return tokens;
}

int smsh_launch_bg(char** args) {
  pid_t pid;
  pid_t wpid;
  int status;

  //fork the process
  pid = fork();
  //error if it didn't fork right
  if(pid == 0) {
    //child process
    if(execvp(args[0], args) == -1) {
      perror("smallsh");
      return 1;
    }
    exit(EXIT_FAILURE);
  }
  //error if fork returns an error
  else if(pid < 0) {
    //error forking
    perror("smallsh");
    return 1;
  }
  //if everything is right, background it
  else {
    //print out the background pid message
    printf("background pid is %d\n", pid);
    //init wpid so we can see if it changed
    wpid = -1;
    //wait for the background process if it is done, otherwise keep going
    wpid = waitpid(-1, &status, WNOHANG);
    //if the process has ended, print out the end message
    if(wpid != -1 && wpid != 0) {
      printf("background pid %d is done: exit value %d\n", wpid, status);
    }
  }

  //return the status as 0 or error (1)
  if(status == 0) {
    return 0;
  }
  else {
    return 1;
  }
}

int smsh_launch(char** args) {
  pid_t pid;
  pid_t wpid;
  int status;

  //fork the process
  pid = fork();
  //error if the fork didn't work right
  if(pid == 0) {
    //child process
    if(execvp(args[0], args) == -1) {
      perror("smallsh");
      return 1;
    }
    exit(EXIT_FAILURE);
  }
  //error if fork() returned an error
  else if(pid < 0) {
    //error forking
    perror("smallsh");
    return 1;
  }
  //if everything is ok, wait for the process to finish
  else {
    //parent process
    do {
      wpid = waitpid(pid, &status, WUNTRACED);
    }while(!WIFEXITED(status) && !WIFSIGNALED(status));
  }

  //return the status of the child
  if(status == 0) {
    return 0;
  }
  else {
    return 1;
  }
}

int smsh_cd(char** args) {
  //go to the home dir if no location is specified
  if(args[1] == NULL) {
    if(chdir(getenv("HOME")) != 0) {
      perror("smallsh");
      return 1;
    }
  }
  //go to the home dir if ~ is specified
  else if(strcmp(args[1], "~") == 0) {
    if(chdir(getenv("HOME")) != 0) {
      perror("smallsh");
      return 1;
    }
  }
  //otherwise, go to the location specified
  else {
    if(chdir(args[1]) != 0) {
      perror("smallsh");
      return 1;
    }
  }
  return 0;
}

int smsh_exit(char** args) {
//return the stop case for the program
  return -1;
}

int smsh_status(int status) {
  //print the last exit status
   printf("exit value %d\n", status);
   return 0;
}

//checks if the process should be backgrounded
int amp(char** args) {
  int i;

  i = 0;

  //repeat while the argument is not null
  while(args[i] != NULL) {
    //check if the argument is &
    if(strcmp(args[i], "&") == 0) {
      //return the index where & was found
      return i;
    }
    i++;
  }
  
  //return -1 if there is no &
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

  //run the cd function if cd was entered
  if(strcmp(args[0], "cd") == 0) {
    return (smsh_cd(args));
  }
  //run the exit function if exit was entered
  else if(strcmp(args[0], "exit") == 0) {
    return (smsh_exit(args));
  }
  //run the status function if status was entered
  else if(strcmp(args[0], "status") ==  0) {
     return (smsh_status(status));
  }
  //do nothing if the line starts with #
  else if(strcmp(args[0], "#") == 0) {
    return 0;
  }
  //check if input is being redirected
  else if(args[1] && strcmp(args[1], "<") == 0) {
    //create a string to contain the path to the specified file
    path = malloc(sizeof(char)*255);
    //cat ./ and the file name to the path var
    sprintf(path, "./%s", args[2]);

    //moves path over one spot
    args[1] = path;
    args[2] = NULL;
    //checks if this was supposed to be running in the background
    if(args[3] && strcmp(args[3], "&") == 0) {
      //remove the &
      args[3] = NULL;
      //free malloc stuff
      free(path);
      //call the launch in background function
      return smsh_launch_bg(args);
    }

    //free malloc stuff
    free(path);
    //print the last exit status

    //call the launch function
    return smsh_launch(args);

  }
  //check if stdout should be redirected
  else if(args[1] && strcmp(args[1], ">") == 0) {
    //make sure there is actually somewhere for the output to be redirected to
    if(args[2]) {
      //start the switch by duplicating stdout
      fflush(stdout);
      bak = dup(1);
      //open a file with the specified name
      //create it if it doesn't exist, empty it if it does
      //open it for writing
      //give the current user read/write priveleges
      new = open(args[2], O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
      //error if the file didn't open
      if(new == -1) {
        perror("open");
      }
      //switch stdout and the file
      dup2(new, 1);
      //close stdout
      close(new);
      //get rid of the > and the filename, since that will happen by default
      args[1] = NULL;
      args[2] = NULL;
      //call the launch function
      exit = smsh_launch(args);
      //switch from the file to stdout
      fflush(stdout);
      dup2(bak, 1);
      close(bak);

      //return the exit status from the process
      return exit;
    }
    //error if there wasn't an output specified
    else {
      perror("smallsh");
      return 1;
    }
  }

  //check if there is an & in the args
  //this is just using exit as a spare int variable
  exit = amp(args);
  //if there is an &, set it's location in the args to NULL and call the
  //background launch function
  if(exit != -1) {
    args[exit] = NULL;
    return smsh_launch_bg(args);
  }
  //otherwise just call the regular launch function
  else {
    return smsh_launch(args);
  }
}

void smsh_loop(void) {
  char* line;
  char** args;
  int i, status, exit;
  pid_t wpid;

  //loop until told to stop by exit
  do {
    //print the prompt
    printf(": ");
    //call the readline function
    line = smsh_read_line();
    //call the split line function to split the line into an array or args
    args = smsh_split_line(line);
    //call the execute function with the args and the status
    status = smsh_execute(args, status);

    //free the malloc stuff
    free(line);
    free(args);
    //set wpid to -1 so we can tell if it's changed and ended a process
    wpid = -1;
    //wait for any completed background processes
    wpid = waitpid(-1, &status, WNOHANG);
    //check if any processes were waited for
    if(wpid != -1 && wpid != 0) {
      //print the bg process message if any were finished, and the status
      printf("background pid %d is done: exit value %d\n", wpid, status);
    }
  }while(status != -1);
}

int main(int argc, char** argv) {
  
  //run the prompt loop
  smsh_loop();

  //exit
  return EXIT_SUCCESS;
}

