#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // getpgrp(), chdir(), getcwd()
#include <stdint.h> // intmax_t
#include <string.h> // strtok
#include <stddef.h> // ptrdiff_t str_gsub
#include <errno.h>
#include <signal.h> // SIGINT etc



/* NOTES:
* Call exec() on: 
* Call fork() on non-built ins:
* https://discord.com/channels/1061573748496547911/1061579120317837342/1073601319874601072
* https://discord.com/channels/1061573748496547911/1061579120317837342/1072019376036909066:w
*
*/


// string substitution 
char *str_gsub(char *restrict *restrict haystack, char const *restrict needle, char const *restrict sub){
  char *str = *haystack;
  size_t haystack_len = strlen(str);
  size_t const needle_len = strlen(needle),
	sub_len = strlen(sub);
  for (; (str = strstr(str, needle)); ){ // HOME EXP: i, i++
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
    if(strcmp(needle, "~") == 0) {break;}; // ADDED HOME EXP
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

// BUILT-IN CD
int cd_smallsh(const char *newWd){
  //CHDIR(2)

  // show cwd
  int cwd_buf_size = 200; // TODO: good size?
  char cwd[cwd_buf_size]; // TODO: good size?
  getcwd(cwd, cwd_buf_size);
  printf("Current working directory (before cd): %s\n", cwd);

  // attempt cd, print error if error
  errno = 0; 
  if (chdir(newWd) != 0){
    perror("chdir() failed\n"); // TODO: Improve this error checking?  
    printf("%i",errno); 
    return -1;
  };
  getcwd(cwd, cwd_buf_size);
  printf("Working directory (after cd): %s\n", cwd);
    //exit(EXIT_SUCCESS);
  return 0;
}

// BUILT-IN EXIT
void exit_smallsh(int fg_exit_status){
  //  KILL(2) - If  pid  equals  0,  then  sig is sent to every process 
  //  in the process group of the calling process
  //  All child processes in the same process group 
  //  shall be sent a SIGINT signal before exiting 
  //  see p. 405 in LPI for e.g.
  
  // print exit to stderr
  perror("\nexit\n");

  // all child processes sent SIGINT prior to exit (see KILL(2))
  //  int kill(pid_t pid, int sig); 
  kill(0, SIGINT);

  //  exit immediately EXIT(3)
  exit(fg_exit_status); // CORRECT ??
}



// 3. EXPANSION 
char *expand_word(char *restrict *restrict word){

  // "~" -> home

  char *homeStr = getenv("HOME"); // does ret NULL if not found. TODO: error?
  if (!homeStr) homeStr = "";
  if (strncmp(*word, "~/", 2) == 0){
    str_gsub(word, "~", homeStr);
  };

  // "$$" -> pid 
  char pidStr[12]; // TODO: good size?
  
  sprintf(pidStr, "%d", getpid()); // guaranteed return
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
 
  return *word;

}


// 2. WORD SPLITTING  
int split_words(char *line, ssize_t line_length){

  // create list of pointers to strings
  char *word_arr[line_length]; // 512??
  int n = 0;

  // tokenize line in loop, expand words 
  char delim[] = " ";//{getenv("IFS") || " \t\n"};
  char *token = strtok(line, delim);
  while(token) {
    word_arr[n] = strdup(token);// NOTE: remember to free each call to strdup
    word_arr[n][strcspn(word_arr[n], "\n")] = 0; // remove newline [1]
    
    // expand word, as applicable
    char *word = expand_word(&word_arr[n]); 

    printf("word_arr[n] after expansion: >%s<\n",word); 
    
    // execute cd - *works
    //if (strncmp(word, "~/", 2) == 0 || strncmp(word, "/", 1) == 0) cd_smallsh(word); 
    //cd_smallsh(word);
    
    // execute exit - *works
    // default exit code is 
    //int exit_code = 0; //NOTE/TODO: see expanson of "$?" exit status of last foreground command 
    if (strcmp(word, "exit") == 0) exit_smallsh(0);

    n++;
    token = strtok(NULL, delim);
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

    /* Check for any un-waited-for background processes in same pid 
     * group as smallsh and print following message 
     * If exited: “Child process %d done. Exit status %d.\n”, <pid>, <exit status>
     * If signaled: “Child process %d done. Signaled %d.\n”, <pid>, <signal number>
    */
    pid_t pid_smallsh = getpid(); 
    pid_t pid_grp_smallsh = getpgrp(); 
    

    /* Display prompt from PS1 */	
    const char *env_p = getenv("PS1");  // TODO: error check?
    fprintf(stderr, "%s",(env_p ? env_p : ""));

    /* Get line of input from stdin */
    ssize_t line_length = getline(&line, &n, stdin); /* Reallocates line */
    if (feof(stdin)){ 
      exit_smallsh(0); // TODO exit status? Check against base64 rev video. 
    }
    if (line_length == -1){
      free(line);
      perror("getline() failed");
      exit(EXIT_FAILURE);
    }

    /* Split words from line */ 
    split_words(line, line_length);

    //printf("Executing CD...\n");
    //cd_smallsh(getenv("HOME")); 
    
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


/* SOURCES
 * [1] https://stackoverflow.com/questions/2693776/removing-trailing-newline-character-from-fgets-input
 *
 */
