#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // getpgrp()
#include <stdint.h> // intmax_t
#include <string.h> // strtok

int splitWords(char *line, ssize_t line_length){
  char *line_arr[line_length];
  int n = 0;
  //line_arr[0] = &line;
  printf("%jd\n",line_length);

  char *token = strtok(line, " ");
  // TODO: check error
  line_arr[n] = token;
  printf("token: %p",token); // LEFT OFF HERE - trying to figure spliiting out each word
  while(token) {
    line_arr[n] = token;
    token = strtok(NULL, " ");
    n++;
    //printf("token: %p",token);
  }
  for (int i = 0; i < line_length; i++) printf("line_arr[%i] %p\n",i, line_arr[i]);

  return 0;
}



int main(){
  // Main loop
  char *line = NULL;
  size_t n = 0;
  //ssize_t nread; ??

  for (;;) {
    /* Display shell prompt from PS1*/	
    const char *env_p = getenv("PS1");  // TODO: error check? 
    fprintf(stderr, "%s",(env_p ? env_p : ""));

    /* Get line of input from terminal */
    ssize_t line_length = getline(&line, &n, stdin); /* Reallocates line */

    /* Check getline() for error */
    if (line_length == -1){
      free(line);
      perror("getline() failed");
      exit(EXIT_FAILURE);
    }

    /* Do things / write line */ 
    //fwrite(line, nread, 1, stdout);
    printf("Line: %s\n",line);
    splitWords(line, line_length);
    //printf("Line now: %s", line);
    
 
    printf("Line after splitting: '");
    for(ssize_t n = 0; n < line_length; ++n)
        line[n] ? putchar(line[n]) : fputs("\\0", stdout);
    puts("'");

    // print process if, process group id
    //pid_t pid = getpid();
    //pid_t pgid = getpgrp();
    //fprintf(stderr, "PID: %jd PGID: %jd",(intmax_t) pid, (intmax_t) pgid);
    //if (pid == pgid) printf("Process leader");                                               ;
  };

  /* Free buffer */
  free(line);
  exit(EXIT_SUCCESS);
}

