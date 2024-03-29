#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>
#include <signal.h> 
#include <sys/types.h>
#include <sys/wait.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/stat.h> 


/*
Author:      James Bush
Date:        2023-03-01
Description: Interactive shell program for CS344, Operating Systems 
*/

//-------------------------------------------------------
// Globals 
//-------------------------------------------------------
// "$?" expansion - req: default to 0 ("0") 
int last_fg_exit_status = 0;
// "$!" expansion 
pid_t most_rec_bg_pid; // NULL by default
int debug = 0;
int command_no = 0; 

//-------------------------------------------------------
// Signal Handler - SIGNINT
//-------------------------------------------------------
void dummy_function(int signo){
	//char* message = "Caught SIGINT\n";
	//write(STDOUT_FILENO, message, 15);
}

//-------------------------------------------------------
// String Substitution -- https://www.youtube.com/watch?v=-3ty5W_6-IQ
//-------------------------------------------------------
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

//------------------------------------------------------- 
// Built-in CD
//-------------------------------------------------------
int cd_smallsh(char *token_arr[], int token_arr_len){
  // req: takes one argument.
  if (token_arr_len < 2) {
    fprintf(stderr, "cd_smallsh(): too few arguments\n"); // Likely imppossible
    return -1; 
  }
  if (token_arr_len > 3) {
    fprintf(stderr, "cd_smallsh(): too many arguments\n"); 
    return -1; 
  }
  char newWd[PATH_MAX]; 
  
  // req: If not provided, the argument is implied to be the expansion of “~/”
  if (token_arr_len == 2 && token_arr[1] == NULL){
    char *homeStr = getenv("HOME"); // pointer or null 
    if (!homeStr) {homeStr = "";}
    strcpy(newWd, homeStr); 
  } 

  // Otherwise, first argument is token_arr[1]
  else {
    strcpy(newWd, token_arr[1]); 
  }

  if (debug == 1) {
   int cwd_size = PATH_MAX; 
   char cwd[cwd_size]; // TODO: good size?
   getcwd(cwd, PATH_MAX);
   printf("\ncd_smallsh(): working directory (before cd): %s\n", cwd);
  }

  // attempt cd, print error if error - chdir(2)
  errno = 0; 
  if (chdir(newWd) != 0){ 
    perror("chdir() failed");
    return -1;
  };
 
  if (debug == 1) {
   int cwd_size = PATH_MAX; 
   char cwd[cwd_size];
   getcwd(cwd, PATH_MAX);
   printf("cd_smallsh(): working directory (after cd): %s\n", cwd);
  }
  // success return 
  return 0;
}


//------------------------------------------------------- 
// Built-in Exit
//------------------------------------------------------- 
int exit_smallsh(char *token_arr[], int token_arr_len){
  // NOTEe: p. 405 in LPI 
  // req: takes one argument.
  if (token_arr_len < 2) {
    fprintf(stderr, "exit_smallsh(): too few arguments\n"); // Likely imppossible
    return -1; 
  }

  if (token_arr_len > 3) {
    fprintf(stderr, "exit_smallsh(): too many arguments\n"); 
    return -1; 
  }

  // no error -- exit will happen 
  // req: print "\nexit\n" to stderr
  fprintf(stderr, "\nexit\n");
  // all child processes sent SIGINT prior to exit (see KILL(2))
  //kill(0, SIGINT); // int kill(pid_t pid, int sig); pid=0 sends to group

  // req: takes 1 argument. If not provided, implied to be expansion of “$?”
  if (token_arr_len == 2 && token_arr[1] == NULL){
    if (debug == 1) printf("A:\n"); 
    exit(last_fg_exit_status); 
  } 

  // Otherwise, exactly one argument provided, first argument is token_arr[1]  
  if (token_arr_len == 3 && token_arr[2] == NULL){
    char *endptr;
    errno = 0; 
    long exit_arg = strtol(token_arr[1], &endptr, 10);
    // req: ..or if an argument is provided that is not an integer.
    if ((errno == ERANGE && (exit_arg == LONG_MAX || exit_arg == LONG_MIN)) || (errno != 0 && exit_arg == 0)) {
      perror("exit_smallsh(): strtol()");
      return 1;
    }
    else if (endptr == token_arr[1]){
      fprintf(stderr, "exit_smallsh(): No digits were found in argument.\n"); 
      return 1;
    
    }
    if (debug == 1) printf("exit arg is %i\n",(int) exit_arg); 
    exit(exit_arg); 
  }
  return 0;
}


//------------------------------------------------------- 
// 5. Execution
//------------------------------------------------------- 
int execute_commands(char *token_arr[], int const token_arr_len, int const run_bg, char const *restrict infile, char const *restrict outfile){
        
    // Check for built-in cd / exit function calls
    if (debug == 1) printf("\nexecute_command() no fork() yet current process %jd\n", (intmax_t) getpid()); 
    // built-in cd 
    // if (strcmp(token_arr[0],"cd") == 0){
    //   cd_smallsh(token_arr, token_arr_len); // TODO check error conditions 
    //   return 0;
    // } 
    // built-in exit 
    if (strcmp(token_arr[0],"exit") == 0){
      exit_smallsh(token_arr, token_arr_len);
      return 0; 
    }
    
    // Otherwise, exit foreground/background process
    int childStatus;
    // Fork a new process...if successful, childPid=0 in child, child's pid in parent 
    pid_t childPid = fork();
    //if (run_bg == 1){most_rec_bg_pid = childPid;}; // NEW check after the fork if error could be -1 check 
    if (debug == 1) printf("\nexecute_commands() current process %jd forked new child pid: %jd\n",(intmax_t) getpid(), (intmax_t) childPid); 

    switch(childPid){
      // Fork error
      case -1:
        perror("\nfork() failed");
        exit(1);
        break;

      // Child process will execute this branch
      case 0: 
        // Register as handler for SIGINT (ctrl-c) (for child)
	      signal(SIGINT, SIG_DFL);
        if (debug == 1) printf("\nexecute_commands() in fork branch 0 current pid %jd 'childPid' %jd\n",(intmax_t) getpid(), (intmax_t) childPid); 
        if (infile){
        	// INFILE - Open source file
	          int sourceFD = open(infile, O_RDONLY);
	          if (sourceFD == -1) { 
		          perror("source open()"); 
		          exit(1); 
	          }
	          // Redirect stdin to source file
            // dup2(<old fd>, <new fd>) 0=stdin, 1=stdout, 2=strerr; newfd points to oldfd
	          int result = dup2(sourceFD, 0);
	          if (result == -1) {   
		          perror("source dup2()"); 
		          exit(2); 
	          }
        }
        // OUTFILE 
        if (outfile){
	        int targetFD = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0777);
	        if (targetFD == -1) { 
		        perror("target open()"); 
		        exit(1); 
	        }
	        //printf("targetFD == %d\n", targetFD); // Written to terminal
	        // Redirect stdout to target file
	        int result = dup2(targetFD, 1);
	        if (result == -1) { 
		        perror("target dup2()"); 
		        exit(2); 
	        }
        }

        // Execute commands
        //most_rec_bg_pid = childPid;
		    if (debug == 1) printf("\nexecute_commands() current process %jd running command <%s> -- most_rec_bg_pid %jd \n",(intmax_t) getpid(), token_arr[0], (intmax_t) most_rec_bg_pid);
        // execvp searches the PATH for the env variable with argument 1
		    execvp(token_arr[0], token_arr);
		    // exec only returns if there is an error
		    perror("execvp() failed");
		    exit(1); 
		    break;

      // Parent process - childPid is pid of the child, parent will execute below 
      default: 
        if (debug == 1) printf("\nexecute_commands() default branch current process %jd\n", (intmax_t) getpid());
        // Foreground - '&' operator not present -> blocking wait 
        // req: If a non-built-in command was executed, and the “&” operator was not present, 
        // the smallsh parent process shall perform a blocking wait (see WAITPID(2)) 
        // on the foreground child process.   
        if (run_bg == 0){
          if (debug == 1) printf("\nexecute_commands() default branch run FG current process %jd\n", (intmax_t) getpid());

          childPid = waitpid(childPid, &childStatus, 0); // TODO error for waitpid LEFT OFF HEERE ****
          //most_rec_bg_pid = getpid(); 

          // check error
          if (childPid == -1){fprintf(stderr, "waitpid() failed\n"); exit(1); } // TODO error good? 

          // 1) if finished/exited...
          if(WIFEXITED(childStatus)){
            if (debug == 1) fprintf(stderr, "\n*FG*Child %jd exited normally with status %d\n", (intmax_t) childPid, WEXITSTATUS(childStatus));
            // req: The "$?" shell variable shall be set to the exit status of the waited-for command. 
            last_fg_exit_status = WEXITSTATUS(childStatus);
          }

          // 2) if term'd by signal...
          else if(WIFSIGNALED(childStatus)){
            if (debug == 1) fprintf(stderr, "Child %jd killed by signal %d\n", (intmax_t) childPid, WTERMSIG(childStatus));
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
            if (debug == 1) printf("\ngoing to background from foreground current process %jd\n",(intmax_t) getpid());
            goto BACKGROUND; 
          }
        }

        // background / non-blocking wait + poll
        // If brackground record $!, then check change of any processes before prompt.
        else if (run_bg == 1){
          if (debug == 1) printf("\nexecute_commands() running BG current process %jd \n",(intmax_t) getpid());
          BACKGROUND:
          //printf("RUNNING BG PID %jd\n", (intmax_t) childPid);
            // req: child process runs in "background", and parent smallsh process does not wait
            //   while ((childPid = waitpid(0, &childStatus, WUNTRACED | WNOHANG)) > 0) { 
            //  req: background is the default behavior of a forked process! TODO - what does this mean? 
            //  req: The "$!" value should be set to the pid of such a process.
            if (debug == 1) printf("\nin bg branch - current most_recent_bg_pid <%jd> will become childPid <%jd>\n", (intmax_t) most_rec_bg_pid, (intmax_t) childPid);
            most_rec_bg_pid = childPid; 
            
            if (debug == 1) printf("\nin bg branch - current most_recent_bg_pid now <%jd> \n", (intmax_t) most_rec_bg_pid);

        } // else 
        // parent waiting done once child exited
        if (debug == 1) printf("\nexecute_commands() breaking from fork current process %jd\n", (intmax_t) getpid());
        break;
        } 
  if (debug == 1) printf("\nreturning from execute_commands() current process %jd\n", (intmax_t) getpid());
  return 0;
}


//------------------------------------------------------- 
// 3. Expansion
//------------------------------------------------------- 
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


//------------------------------------------------------- 
// 4. Parsing 
//------------------------------------------------------- 
int parse_words(char *word_arr[], int word_arr_len){

  char *infile = NULL; 
  char *outfile = NULL;
  int run_bg = 0; // 1 if '&' passed
                  
  // 1. fist occurrence of "#" and additional words following are comment
  int i = 0;
  for (; i < word_arr_len; i++){
    if (word_arr[i] == NULL) break;
    else if (strncmp(word_arr[i],"#", 1) == 0){
      word_arr[i] = NULL;
      word_arr_len -= (word_arr_len - i - 1);
     }
  }
  
  // 2. if last word is '&' it indicates the command run in bg
  if (strcmp(word_arr[word_arr_len-2],"&") == 0) {
    word_arr[word_arr_len-2] = NULL;
    word_arr_len -= 1; 
    run_bg = 1;
  }

  // steps 3/4 can occur in either order 
  while( (strcmp(word_arr[word_arr_len-3], "<") == 0) || (strcmp(word_arr[word_arr_len-3], ">") == 0) ){
    
    // infile at end of current arr
    if (strcmp(word_arr[word_arr_len-3],"<") == 0) {
      infile = word_arr[word_arr_len-2];
      word_arr[word_arr_len-3] = NULL;
      word_arr_len -= 2;
    }

    // outfile at end of current arr 
    if (strcmp(word_arr[word_arr_len-3],">") == 0) {
      outfile = word_arr[word_arr_len-2];
      word_arr[word_arr_len-3] = NULL;
      word_arr_len -= 2;
    }
  }

  // req: If at this point no command word is present, 
  // smallsh shall silently return to step 1 and print a new prompt message.
  if (word_arr_len == 0 || word_arr[0] == NULL || strcmp(word_arr[0], "") == 0) return 0; // -1? 
  //else {printf("There are commands:\n"); print_arr(word_arr, word_arr_len);}
  //exit(0);
  
  // execute command list  
  execute_commands(word_arr, word_arr_len, run_bg, infile, outfile); 

  // check / free output 
  // maybe track the indices of nulled above 
  for (int i = 0; i < word_arr_len; i++){
    free(word_arr[i]); 
  }
 
  return 0; 
}

//------------------------------------------------------- 
// 2. Word Splitting 
//------------------------------------------------------- 
int split_words(char *line, ssize_t line_length){
  if (debug == 1) {
    printf("in split_words line: %s line_length %lu\n", line, line_length); 
  }
  char *word_arr[line_length]; // req: pointers to strings, min 512 supported
  char delim_alt[] = " \t\n";
  char *delim = getenv("IFS");  // pointer or null   
  if (delim == NULL) delim = delim_alt; 
  
  // tokenize line in loop, expand words 
  int n = 0;
  char *token = strtok(line, delim);
  while(token) {
    word_arr[n] = strdup(token);// NOTE: free each call to strdup
    // https://stackoverflow.com/questions/2693776/removing-trailing-newline-character-from-fgets-input
    word_arr[n][strcspn(word_arr[n], "\n")] = 0; // remove newline 
    // expand word, as applicable
    expand_word(&word_arr[n]); // can asssign char *word = ... to return
    // increment and get next token 
    n++; 
    token = strtok(NULL, delim);
  }
  word_arr[n] = NULL; 
  // parse the command word array 
  // TODO consider moving this call to main()
  parse_words(word_arr, n+1);

  return 0;
}

//------------------------------------------------------- 
// 1. Main / Input
//------------------------------------------------------- 

int main(){

  // Signal handling 
  struct sigaction dummy_action = {0}, default_action = {0}, ignore_action = {0};

  // dummy_action
	dummy_action.sa_handler = dummy_function; 
	// Block all catchable signals while dummy_function is running
	sigfillset(&dummy_action.sa_mask); 
	// No flags
	dummy_action.sa_flags = 0;
  // Initially register dummy_action for SIGINT (ctrl-c) (temporarily)
	sigaction(SIGINT, &dummy_action, NULL);
  // default action 
	default_action.sa_handler = SIG_DFL;
  // Block all catchable signals while dummy_function is running ?
	// sigfillset(&dummy_action.sa_mask); 
  // ignore_action 
	ignore_action.sa_handler = SIG_IGN;
	// Register as handler for SIGTSTP (ctrl-z) (permanently)
	sigaction(SIGTSTP, &ignore_action, NULL);

  // For getting input from getline()
  char *line = NULL;
  size_t n = 0;
  pid_t pid = getpid();
  pid_t gpid = getpgrp(); 
  
  // Main loop
  for (;;) {   
    if (debug == 1) printf("\nmain() for() loop current process %jd most recent bg process %jd\n", (intmax_t) getpid(), (intmax_t) most_rec_bg_pid);

    pid_t childPid = most_rec_bg_pid; 
    int childStatus;
    
    // PPre-prompt check for any un-waited-for background processes in the same process group ID...
    // as smallsh, and print the following informative message to stderr for each:
    while ((childPid =  waitpid(0, &childStatus, WUNTRACED | WNOHANG)) > 0){
      if (debug == 1) printf("main() pre-prompt while loop: smallsh pid: %jd groupid %jd childPid %jd\n", (intmax_t) pid, (intmax_t) gpid, (intmax_t) childPid);
      // error 
      if (childPid == -1){
       fprintf(stderr,"waitpid() failed\n"); 
       exit(1); 
      }       
      // If exited: “Child process %d done. Exit status %d.\n”, <pid>, <exit status>
      if(WIFEXITED(childStatus)){
        fprintf(stderr, "Child process %jd done. Exit status %d.\n", (intmax_t) childPid, WEXITSTATUS(childStatus));
      }
      // If a child process is stopped, smallsh shall send it the SIGCONT signal and print 
      // the following message to stderr: 
      if(WIFSTOPPED(childStatus)){
        fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t) childPid); //WSTOPSIG(childStatus));
        kill(childPid, SIGCONT); 
      }
      // If signaled: “Child process %d done. Signaled %d.\n”, <pid>, <signal number>
      if(WIFSIGNALED(childStatus)){
        fprintf(stderr, "Child process %jd done. Signaled %d.\n", (intmax_t) childPid, WTERMSIG(childStatus));
      } 
    }

    // Prompt input 
    if (debug == 1) printf("\nprint prompt\n");
    // req: print a prompt to stderr by expanding the PS1 
    const char *env_p = getenv("PS1");  // pointer or null   
    fprintf(stderr, "%s",(env_p ? env_p : "\033[0;36m$ ")); 

    //command_no ++; 
    //fprintf(stderr, "%i %jd %s",command_no, (intmax_t) pid, (env_p ? env_p : ""));

    // Now register dummy_action for SIGINT 
    sigaction(SIGINT, &dummy_action, NULL);

    // req: after prompt, read line from stdin (getline(3) 
    ssize_t line_length = getline(&line, &n, stdin);

    // end of input
    if (feof(stdin)){
      if (debug == 1) printf("EOF on stdin\n");
     // req: EOF on stdin interpreted as implied `exit $?`
      fprintf(stderr, "\nexit\n");
      exit(last_fg_exit_status);  
    }

    // May be triggered by signals
    if (line_length == -1){
      //free(line);
      fprintf(stderr, "main() getline() failed not EOF\n");
      //free(line);
      clearerr(stdin); // TODO: Right? no EOF reset stdin (discussed somewhere + M5 see notes) 
      //exit(EXIT_FAILURE); // TODO right error?
      fprintf(stderr, "\n"); 
      //exit(last_fg_exit_status); 
      continue; 
    }

    // Now register ignore_action for SIGINT 
    sigaction(SIGINT, &ignore_action, NULL);

    // 2. Word Splitting 
    if (debug == 1) printf("\nmain() call to split_words()\n");
    split_words(line, line_length);
        
    if (debug == 1) printf("\nback to main() from split_words()\n");
  };
  
  // Free buffer - 230210 - still has one block unfreed at end
  // I think that the quit needs to free before exiting
  free(line);
  exit(EXIT_SUCCESS);
}


