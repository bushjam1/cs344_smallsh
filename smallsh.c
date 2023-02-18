#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // getpgrp(), chdir(), getcwd(), fork(), execvp()
#include <stdint.h> // intmax_t
#include <string.h> // strtok
#include <stddef.h> // ptrdiff_t str_gsub
#include <errno.h>
#include <signal.h> // SIGINT etc
#include <sys/types.h> // pid_t
#include <sys/wait.h> // wait
#include <limits.h> // PATH_MAX


// NOTES/TODO:
// consider generic error exit function / incorporate into existing exit_smallsh 
// see p. 52 in LPI
// use of perror versus fprintf(stderr,..) throughout 
// REQ: Any explicitly mentioned error shall print informative error msg to stderr (fprintf) and
// $? set to a non-zero-value. Further processing of the command line stops and return to step 1 / input. 
 
// PRIOR TO TEST
// Set the IFS expansion to " \t\n", etc. 


// GLOBALS   
// "$?" expansion - req: default to 0 ("0") 
int last_fg_exit_status = 0;
// "$!" expansion 
pid_t most_rec_bg_pid; // NULL by default 


// HELPER FUNCTIONS 

// string substitution 
char *str_gsub(char *restrict *restrict haystack, char const *restrict needle, char const *restrict sub){
  char *str = *haystack;
  size_t haystack_len = strlen(str);
  size_t const needle_len = strlen(needle),
	sub_len = strlen(sub);
  for (; (str = strstr(str, needle)); ){
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
    if(strcmp(needle, "~") == 0) {break;}; // ADDED FOR HOME EXP
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
// REQ: The cd built-in takes one argument. If not provided, 
//      the argument is implied to be the expansion of “~/”, 
//      the value of the HOME environment variable
//      If shall be an error if more than one argument is provided.
//      Smallsh shall change its own current working directory 
//      to the specified or implied path. It shall be an error if the operation fails.

int open_file_smallsh(const char *filename,const int mode){

  // mode 0 = read, mode 1 = write, mode 2 = append 
  //  
  // If a filename was specified as the operand to the input (“<”) 
  // redirection operator, the specified file shall be opened for 
  // reading on stdin. It shall be an error if the file cannot be 
  // opened for reading or does not already exist.
  //
  //If a filename was specified as the operand to the output (“>”) 
  //redirection operator, the specified file shall be opened for 
  //writing on stdout. If the file does not exist, it shall be 
  //created with permissions 0777. It shall be an error if the 
  //file cannot be opened (or created) for writing.

  return 0; 

}
int cd_smallsh(const char *newWd){
  int cwd_size = PATH_MAX; 
  char cwd[cwd_size]; // TODO: good size?
  getcwd(cwd, PATH_MAX);
  printf("Current working directory (before cd): %s\n", cwd);

  // attempt cd, print error if error - chdir(2)
  errno = 0; 
  if (chdir(newWd) != 0){ 
    perror("chdir() failed\n"); // TODO: Improve this error checking?  
    printf("%i",errno); 
    return -1;
  };
  // show cwd after cd 
  getcwd(cwd, cwd_size);
  printf("Working directory (after cd): %s\n", cwd);

  // success return 
  return 0;
}


// BUILT-IN EXIT
void exit_smallsh(int fg_exit_status){
  //  NOTES: 
  //  when process successful exit == 0
  //  error condition == non-0 
  //  KILL(2) - If  pid  equals  0,  then  sig is sent to every process 
  //  in the process group of the calling process
  //  All child processes in the same process group 
  //  shall be sent a SIGINT signal before exiting 
  //  see p. 405 in LPI for e.g.
  
  // print exit to stderr
  fprintf(stderr, "\nexit\n");

  // all child processes sent SIGINT prior to exit (see KILL(2))
  // int kill(pid_t pid, int sig); 
  kill(0, SIGINT);

  //  exit immediately EXIT(3)
  exit(fg_exit_status); // NOTE: status & 0377 returned to the parent 
}


// NON-BUILT-INS
int non_built_ins(char *token_arr[], int const run_bg, char const *restrict infile, char const *restrict outfile){
    
    // printf("Parent pid: %d\n", getpid()); 
   
    // infile / outfile 
    //
    printf("infile: %s outfile: %s\n", infile, outfile);
    
    // execvp(tokenArray[0], tokenArray)
    // TODO: sort this token_arr = newargv setup 
    char *newargv[] = {token_arr[0], token_arr[1], NULL};

    int childStatus;

    // Fork a new process, 
    // if successful value of childPid is 0 in child, child's pid in parent 
    pid_t childPid = fork();
    printf("->child pid %jd\n",(intmax_t) childPid); 

    int is_bg_proc = run_bg; 

    switch(childPid){

      // Fork error
      case -1:
        perror("fork() failed"); // TODO error good?
        exit(1);
        break;

      // Child process will execute this branch
      case 0: 
        //most_rec_bg_pid = childPid;
		    printf("child (%jd) running command -- most_rec_bg_pid %jd \n", (intmax_t) getpid(), (intmax_t) most_rec_bg_pid);
        // execvp searches the PATH for the env variable with argument 1
		    execvp(newargv[0], newargv);
		    // exec only returns if there is an error
		    perror("execvp() failed");
		    exit(2); // TODO error good? 
		    break;

      // Parent process - childPid is pid of the child, parent will execute below 
      default: 
            
        // check if foreground process or background process - bg is default 

        // Foreground - '&' operator not present -> blocking wait 
        if (is_bg_proc == 0){ 

          childPid = waitpid(0, &childStatus, 0); // TODO error for waitpid

          // check error
          if (childPid == -1){fprintf(stderr, "waitpid() failed\n"); exit(1); } // TODO error good? 

          // 1) if finished/exited...
          if(WIFEXITED(childStatus)){
            fprintf(stderr, "Child %jd exited normally with status %d\n", (intmax_t) childPid, WEXITSTATUS(childStatus));
            // req: The "$?" shell variable shall be set to the exit status of the waited-for command. 
            last_fg_exit_status = WEXITSTATUS(childStatus);
          }
          // 2) if term'd by signal...
          else if(WIFSIGNALED(childStatus)){
            fprintf(stderr, "Child %jd killed by signal %d\n", (intmax_t) childPid, WTERMSIG(childStatus));
            // req: ...set '$?' to 128 + [n]/the number of terming signal to child 
            last_fg_exit_status = 128 + WTERMSIG(childStatus); 
          }  
          // 3) if stopped...
          else if(WIFSTOPPED(childStatus)){
            // req: send it the SIGCONT signal 
            kill(childPid, SIGCONT); // TODO: error -1
            // req: print following to stderr: 
            fprintf(stderr, "Child process %d stopped. Continuing.\n", childPid); // TODO: (intmax_t) childPid?
            // req: "$!"/bg shall be updated to the pid of the child process as if had been a bg command 
            most_rec_bg_pid = childPid; 
            // req: Smallsh shall no longer perform a block wait on this process, 
            // and it will continue to run in the background
            goto BACKGROUND; // TODO - ideal?
          }
        }

        // background / non-blocking wait + poll
        // NOTE: Background? record $!. Then check change of any processes before prompt.
        else if (is_bg_proc == 1){
          BACKGROUND: 
            // req: child process runs in "background", and parent smallsh process does not wait
            while ((childPid = waitpid(0, &childStatus, WUNTRACED | WNOHANG)) > 0) { // 0 versus WUNTRACED | WNOHANG) for blocking/non
            //  req: background is the default behavior of a forked process! 
            //  req: The "$!" value should be set to the pid of such a process.
           // most_rec_bg_pid = childPid;  

            // check at each iteration if
            // print value of waitpid 
            fprintf(stderr, "waitpid() returned PID=%jd childstatus=%d", (intmax_t) childPid, childStatus); 
        
            // waitpid() error
            if (childPid == -1){fprintf(stderr,"waitpid() failed\n"); exit(1); } // TODO error good? 
            // 1) finished - WIFEXITED
            if(WIFEXITED(childStatus)){
              printf("bg child %jd exited normally with status %d\n", (intmax_t) childPid, WEXITSTATUS(childStatus));
              // anything specific? 
            }
            // 2) stopped - WIFSTOPPED
            if(WIFSTOPPED(childStatus)){
              printf("bg child %jd stopped by a signal number %d\n", (intmax_t) childPid, WSTOPSIG(childStatus));
              // anything specific? 
            }
            // 3) signaled - WIFSIGNALED
            if(WIFSIGNALED(childStatus)){
              printf("bg child %jd killed by signal %d\n", (intmax_t) childPid, WTERMSIG(childStatus));
              // anything specific? 
            } 

          // LEFT OFF 230216:1600
          // LPI pp. ~548 helpful on waiting 
          //
          //             SIGNAL HANDLING 
          //             The SIGTSTP signal shall be ignored by smallsh.
          //             The SIGINT signal shall be ignored (SIG_IGN) at 
          //             all times except when reading a line of input in Step 1, 
          //             during which time it shall be registered to a signal handler 
          //             which does nothing.
          //             Explanation:
          //             SIGTSTP (CTRL-Z) normally causes a process to halt, 
          //             which is undesirable. The smallsh process should not 
          //             respond to this signal, so it sets its disposition to SIG_IGN.
          //             The SIGINT (CTRL-C) signal normally causes a process to exit 
          //             immediately, which is not desired.
          
        } // while 
        } // else 
        // parent waiting done once child exited 
        break;
        } 
  return 0;
}


// 3. EXPANSION 
char *expand_word(char *restrict *restrict word){

  // req: "~" replaced with HOME env var 
  if (strncmp(*word, "~/", 2) == 0){
    char *homeStr = getenv("HOME"); // pointer or null 
    if (!homeStr) {homeStr = "";}
    str_gsub(word, "~", homeStr);
  }

  // req: "$$" replaced with smallsh pid 
  char pidStr[12];
  pid_t pid = getpid(); // guaranteed  
  sprintf(pidStr, "%jd", (intmax_t) pid); 
  str_gsub(word, "$$", pidStr);
    
  // req: "$?" replaced with EXIT STATUS last fg command / default to 0 ("0")
  char last_fg_exit_status_str[12];
  sprintf(last_fg_exit_status_str, "%d", last_fg_exit_status); 
  str_gsub(word, "$?", last_fg_exit_status_str);
    
  // req: "$!" -> PID of most recent bg process; default to empty string (““) if not available
  char most_rec_bg_pid_str[12];
  // if there is a most_rec_bg_pid, set the most_rec_bg_pid_str val
  if (most_rec_bg_pid) {sprintf(most_rec_bg_pid_str, "%jd", (intmax_t) most_rec_bg_pid);}
  str_gsub(word, "$!", (most_rec_bg_pid ? most_rec_bg_pid_str : "")); // sub "" if no most_rec_bg_pid
 
  return *word;
}


// 4. PARSING 
int parse_words(char *word_arr[], int word_arr_len){

  // NOTE/TODO: array need to be null-terminated?
  char *infile = NULL; 
  char *outfile = NULL;
  int run_bg = 0; // set to 1 if '&' passed
                  
  // 1. fist occurrence of "#" and additional words following are comment
  int i = 0;
  for (; i < word_arr_len; i++){
    if (strncmp(word_arr[i],"#", 1) == 0){
      word_arr[i] = NULL;
      word_arr_len -= (word_arr_len - i);
      }
  }
  
  // 2. if last word is '&' it indicates the command run in bg
  if (strcmp(word_arr[word_arr_len-1],"&") == 0) {
    word_arr[word_arr_len-1] = NULL; 
    run_bg = 1;
  }

  // 3. if last word immediately preceded by "<" it shall be 
  // interpreted as filename operand of input redirection operator 
  if (strcmp(word_arr[word_arr_len-2],"<") == 0) {
    infile = word_arr[word_arr_len-1];
    printf("we have infile <\n");
    //free(word_arr[word_arr_len-2]);
    word_arr[word_arr_len-2] = NULL;
    word_arr_len -= 2;
  }

  // 4. if last word immediately prceded by ">" it shall be 
  // interpreted as filename operand of output redirection operator 
  if (strcmp(word_arr[word_arr_len-2],">") == 0) {
    outfile = word_arr[word_arr_len-1];
    printf("we have outfile >\n");
    //free(word_arr[word_arr_len-2]);
    word_arr[word_arr_len-2] = NULL;
    word_arr_len -= 2;

  }

    // steps 3/4 can occur in either order 
    // all other words regular words and form the command and its arguments 
    // tokens in 1-4 not included as command arguments 
    // if "<", ">", and "&" appear outside end-of-line context described above, they are treated
    // as regular arguments (see the description for e.g.) 
 
    // examples
    // [command] [arguments] [> outfile] [< infile ] [&] [#comment]
    // [command] [arguments] [< infile ] [> outfile] [&] [#comment]
  //} 

  //if (infile || outfile || run_bg) printf("infile or outfile or run_bg\n");
  //printf("Command arguments will be:\n");
  //for (int i = 0; i < word_arr_len; i++){printf("word_arr[%i]: %s\n", i, word_arr[i]);}
  //if (infile){printf("Infile will be: %s\n",infile);};
  //if (outfile){printf("Outfile will be: %s\n",outfile);};
  //exit(1);
  
  // execute built-ins
  // built-in cd 
  if (strcmp(word_arr[0],"cd") == 0){
    cd_smallsh(word_arr[1]); // TODO check error conditions 
    return 0;
  } 

  // built-in exit
  if (strcmp(word_arr[0], "exit") == 0){
    // req: takes 1 argument. If not provided, it is implied to be
    // expansion of “$?”, the exit status of the last foreground command.
    if (word_arr_len == 1){
      printf("Exiting passing: %d as argument\n",last_fg_exit_status); 
      exit_smallsh(last_fg_exit_status);
    }
    // req: It shall be an error if more than one argument is provided... 
    // WORKS 
    if (word_arr_len > 2) {
      fprintf(stderr, "Too many arguments for exit_smallsh()\n");
      return 1;
    }
    // exactly one argument provided 
    else{
      char *endptr;
      errno = 0; 
      long exit_arg = strtol(word_arr[1], &endptr, 10);
      // req: ..or if an argument is provided that is not an integer.
      if ((errno == ERANGE && (exit_arg == LONG_MAX || exit_arg == LONG_MIN)) || (errno != 0 && exit_arg == 0)) {
        perror("strtol");
        return 1;
      }
      else if (endptr == word_arr[1]){
        fprintf(stderr, "No digits were found exit_smallsh()\n"); 
        return 1;
      }
      exit_smallsh(exit_arg);
    }
  }
  
  // non-built-ins 
  non_built_ins(word_arr, run_bg, infile, outfile); 

  // check / free output 
  // maybe track the indices of nulled above 
  for (int i = 0; i < word_arr_len; i++){
    free(word_arr[i]); 
  }
 
  return 0; 
}


// 2. WORD SPLITTING  
int split_words(char *line, ssize_t line_length){

  char *word_arr[line_length]; // req: pointers to strings, min 512 supported
  char delim[] = " "; // TODO: {getenv("IFS") || " \t\n"};
  int n = 0;

  // tokenize line in loop, expand words 
  char *token = strtok(line, delim);
  while(token) {
    word_arr[n] = strdup(token);// NOTE: free each call to strdup
    word_arr[n][strcspn(word_arr[n], "\n")] = 0; // remove newline [1]
    // expand word, as applicable
    expand_word(&word_arr[n]); // can asssign char *word = ... to return
    // increment and get next token 
    n++; 
    token = strtok(NULL, delim);
  }
  // TODO need null termination at end of word_arr??? 
  // https://discord.com/channels/1061573748496547911/1061579120317837342/1074827823832907776

  // parse the command word array 
  // TODO consider move to main()
  parse_words(word_arr, n);

  return 0;
}


// 1. INPUT  
int main(){

  // Main loop
  char *line = NULL;
  size_t n = 0;
  pid_t pid = getpid();
  printf("smallsh PID: (%jd)\n", (intmax_t) pid); 

  for (;;) {

    // req: Check for any un-waited-for background processes in same pid 
    // group as smallsh and print following message 
    // If exited: “Child process %d done. Exit status %d.\n”, <pid>, <exit status>
    // If signaled: “Child process %d done. Signaled %d.\n”, <pid>, <signal number>
    // If stopped: smallsh send it SIGCONT and print to stderr :“Child process %d stopped. Continuing.\n”, <pid>"
    // e.g., fprintf(stderr, "Child process %jd done. Exit status %d\n", (intmax_t) pid, status); 
   
    // Display prompt from PS1	
    // REQ/TODO: If reading interrupted by signal (see sig handling) a newline is printed, then 
    // a new command prompt shall be printed (including checking for background processes) and 
    // reading input shall resume. See CLEARERR(3) and reset errno. 

    // req: print a prompt to stderr by expanding the PS1 
    const char *env_p = getenv("PS1");  // pointer or null   
    fprintf(stderr, "%s",(env_p ? env_p : ""));

    // req: after prompt, read line from stdin (getline(3) 
    ssize_t line_length = getline(&line, &n, stdin); 
    if (feof(stdin)){
      // req: EOF on stdin interpreted as implied `exit $?`
      // TODO req: if reading interrupted by signal (signal handling) 
      //   then newline printed, check for background processes, new prompt, read resume 
      //   see clearerr(3) and reset errno 
      exit_smallsh(last_fg_exit_status);  
    }
    // handle error 
    if (line_length == -1){
      free(line);
      fprintf(stderr, "getline() failed\n");
      clearerr(stdin); // TODO: Right? no EOF reset stdin (discussed somewhere + M5 see notes) 
      exit(EXIT_FAILURE); // TODO right error? 
    }

    // 2. Word Splitting 
    split_words(line, line_length);



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
 * 
 */
