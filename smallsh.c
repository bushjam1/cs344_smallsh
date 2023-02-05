#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>

int main(){
  // Main loop
 
  char *line = NULL;
  size_t n = 0;
  ssize_t nread;

  for (;;) {
    /* Display prompt*/	
    printf("%s",getenv("PS1")); 
    /* Get line of input from terminal */

    ssize_t line_length = getline(&line, &n, stdin); /* Reallocates line */
    
    /* Check for error */ 
    if (line_length == -1){
      free(line);
      perror("getline() failed");
      exit(EXIT_FAILURE);
    };

    /* Process / write line */ 
    //fwrite(line, nread, 1, stdout);
    printf("Line: %s\n",line);
    
  };
  /* Free buffer */
  free(line);
  exit(EXIT_SUCCESS);
}

