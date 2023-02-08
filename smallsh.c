#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // getpgrp()
#include <stdint.h> // intmax_t
#include <string.h> // strtok

int splitwords(char *line, ssize_t line_length){
  // create list of pointers to strings
  char *word_arr[line_length];
  int n = 0;
   //char s[] = {"**"};//{getenv("IFS") || " \t\n"};

  char *token = strtok(line, " ");
  while(token) {
    //char *word = strdup(token);
    word_arr[n] = strdup(token);
    // NOTE: remember to free each call to strdup
    token = strtok(NULL, " "); // ?? casting appropriately
    n++;
  }
  for (int i = 0; i < n; i++) {printf("line_arr[%i] of %lu lines | val: %s\n",i, line_length, word_arr[i]); free(word_arr[i]);};
  //free(*word_arr);
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
    printf("LINE LEN: %lu",line_length);

    /* Check getline() for error */
    if (line_length == -1){
      free(line);
      perror("getline() failed");
      exit(EXIT_FAILURE);
    }

    /* Do things / write line */ 
    //fwrite(line, nread, 1, stdout);
    printf("Line: %s\n",line);
    splitwords(line, line_length);
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

