#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // getpgrp()
#include <stdint.h> // intmax_t
#include <string.h> // strtok
#include <stddef.h> // ptrdiff_t str_gsub 



// 3. EXPANSION 
// str_gsub should work here


char *str_gsub(char *restrict *restrict haystack, char const *restrict needle, char const *restrict sub){
  char *str = *haystack;
  size_t haystack_len = strlen(str);
  size_t const needle_len = strlen(needle),
	 sub_len = strlen(sub);

  for (; (str = strstr(str, needle));){
    ptrdiff_t off = str - *haystack;
    if (sub_len > needle_len) {
      str = realloc(*haystack, sizeof **haystack * (haystack_len + sub_len - needle_len + 1));
      if (!str) goto exit;
      *haystack = str;
      str = *haystack + off;
    }
    memmove(str + sub_len, str + needle_len, haystack_len + 1 - off - needle_len);
    memcpy(str, sub, sub_len);
    haystack_len = haystack_len + sub_len - needle_len;
    str += sub_len; 
  }
  str = *haystack;
  if (sub_len < needle_len) {
    str = realloc(*haystack, sizeof **haystack * (haystack_len + 1));
    if (!str) goto exit;
    *haystack = str;
  } 
  exit:
    return str;
}

// 2. WORD SPLITTING  
int splitwords(char *line, ssize_t line_length){

  // TESTING GSUB
  //char *lineTest = "subsituting needle";
  //char const *needle = "needle";
  //char const *sub = "sub";

  //char *ret = str_gsub(&line, "needle", "sub");
  //if (!ret) exit(1);
  //printf("ret: %s", ret);

  // create list of pointers to strings
  char *word_arr[line_length];

  // tokenize line in loop 
  int n = 0;
  char delim[] = " ";// TODO: = {getenv("IFS") || " \t\n"};
  char *token = strtok(line, delim);//" ");
  while(token) {
    word_arr[n] = strdup(token);
    // NOTE: remember to free each call to strdup
    char *ret = str_gsub(&word_arr[n], "needle", "sub");
    printf("RET: %s", ret);
    n++;
    token = strtok(NULL, delim);//" "); // ?? casting appropriately
  }
  // Checking line 
  for (int i = 0; i < n; i++) {printf("line_arr[%i] of %lu lines | val: %s\n",i, line_length, word_arr[i]); free(word_arr[i]);};
  return 0;
}


// 1. INPUT  
int main(){

  // Main loop
  char *line = NULL;
  size_t n = 0;

  for (;;) {

    /* Display prompt from PS1 */	
    const char *env_p = getenv("PS1");  // TODO: error check? 
    fprintf(stderr, "%s",(env_p ? env_p : ""));

    /* Get line of input from stdin */
    ssize_t line_length = getline(&line, &n, stdin); /* Reallocates line */
    if (line_length == -1){
      free(line);
      perror("getline() failed");
      exit(EXIT_FAILURE);
    }

    /* Split words from line */ 
    splitwords(line, line_length);
    
    /* Check line after split */ 
    printf("Line after splitting: '");
    for(ssize_t n = 0; n < line_length; ++n)
        line[n] ? putchar(line[n]) : fputs("\\0", stdout);
    puts("'");

    /* TODO: PID / PGID logic */
    // print process if, process group id
    //pid_t pid = getpid();
    //pid_t pgid = getpgrp();
    //fprintf(stderr, "PID: %jd PGID: %jd",(intmax_t) pid, (intmax_t) pgid);
    //if (pid == pgid) printf("Process leader");       

    free(line);
    break;
  };

  /* Free buffer */
  //free(line);
  exit(EXIT_SUCCESS);
}

