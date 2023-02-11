#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // getpgrp()
#include <stdint.h> // intmax_t
#include <string.h> // strtok
#include <stddef.h> // ptrdiff_t str_gsub 



// string substitution 
char *str_gsub(char *restrict *restrict haystack, char const *restrict needle, char const *restrict sub){
  char *str = *haystack;
  size_t haystack_len = strlen(str);
  size_t const needle_len = strlen(needle),
	sub_len = strlen(sub);
  //int homeEx = strncmp(*haystack, "~/", 2); // HOME EXP
  for (int i = 0; (str = strstr(str, needle)); i++){ // HOME EXP: i, i++
    ptrdiff_t off = str - *haystack;
    if (sub_len > needle_len) {
      str = realloc(*haystack, sizeof **haystack * (haystack_len + sub_len - needle_len + 1));
      //if (homeEx == 0 && i == 1) { // HOME EXP
      //  goto exit;}; // HOME EXP
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



int expandword(char *restrict *restrict word){

  // home exp 
  char *homeStr = getenv("HOME");
  if (strncmp(*word, "~/", 2) == 0){
    str_gsub(word, "~", homeStr);
  };

  // "$$" -> pid 
  char pidStr[12]; // TODO: good size? 
  sprintf(pidStr, "%d", getpid());
  str_gsub(word, "$$", pidStr);
    
  // "$?" -> exit status last fg command 
  // shall default to 0 (“0”) 
  char fgExitStatus[12]; // TODO good size?
  sprintf(fgExitStatus, "%d", 0); // TODO: need fg exit status 
  str_gsub(word, "$?", fgExitStatus);
    
  // "$!" -> pid of most recent bg process
  // shall default to an empty string (““) if no background process ID is available
  char pidRecentBgProc[12]; // TODO good size?
  sprintf(pidRecentBgProc, "%d", 1111); 
  str_gsub(word, "$!", pidRecentBgProc); 
 
  return 0;

}


// 2. WORD SPLITTING  
int splitwords(char *line, ssize_t line_length){

  // create list of pointers to strings
  char *word_arr[line_length]; // 512??
  int n = 0;

  // declare homeStr for later expansion
  //char *homeStr = getenv("HOME");
  //strcat(homeStr, "/"); 
  
  // tokenize line in loop, expand words 
  char delim[] = " ";// TODO: = {getenv("IFS") || " \t\n"};
  char *token = strtok(line, delim);
  while(token) {
    word_arr[n] = strdup(token); // NOTE: remember to free each call to strdup
    

    expandword(&word_arr[n]); 
    // 3. EXPANSION - TODO? move to str_gsub/
    
    // "~/" -> $HOME
    //if (strncmp(word_arr[n], "~/", 2) == 0){
    //  str_gsub(&word_arr[n], "~/", homeStr);
    //};
    
    // "$$" -> pid 
    //char pidStr[12]; // TODO: good size? 
    //sprintf(pidStr, "%d", getpid());
    //str_gsub(&word_arr[n], "$$", pidStr);
    
    // "$?" -> exit status last fg command 
    // shall default to 0 (“0”) 
    //char fgExitStatus[12]; // TODO good size?
    //sprintf(fgExitStatus, "%d", 0); // TODO: need fg exit status 
    //str_gsub(&word_arr[n], "$?", fgExitStatus);
    
    // "$!" -> pid of most recent bg process
    // shall default to an empty string (““) if no background process ID is available
    //char pidRecentBgProc[12]; // TODO good size?
    //sprintf(pidRecentBgProc, "%d", 1111); 
    //str_gsub(&word_arr[n], "$!", pidRecentBgProc); 
                                                                          
    //printf("RET: %s", ret);
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

  };

  /* Free buffer */
  // 230210 - still has one block unfreed at end
  // I think that the quit needs to free before exiting
  free(line);
  exit(EXIT_SUCCESS);
}

