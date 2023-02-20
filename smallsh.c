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
#include <fcntl.h> // open()
#include <sys/stat.h> // fstat()
                      //


// NOTES/TODO:
// consider generic error exit function / incorporate into existing exit_smallsh 
// see p. 52 in LPI
// use of perror versus fprintf(stderr,..) throughout 
// REQ: Any explicitly mentioned error shall print informative error msg to stderr (fprintf) and
// $? set to a non-zero-value. Further processing of the command line stops and return to step 1 / input. 
 
// PRIOR TO TEST
// Set the IFS expansion to " \t\n", etc.
//
//


// GLOBALS   
// "$?" expansion - req: default to 0 ("0") 
int last_fg_exit_status = 0;
// "$!" expansion 
pid_t most_rec_bg_pid; // NULL by default

// HELPER FUNCTIONS / GLOBALS
int debug = 0;


// print a string pointer array out with null values
void print_arr(char *arr[], int size){
  for (int i = 0; i < size; i++){
    if (arr[i] == NULL){
      printf("arr[%i] is NULL\n",i); 
    } else {
      printf("arr[%i]: %s\n",i, arr[i]); 
    }
  }
}

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

//------------------------------------------------------- 
// OPEN FILE
//-------------------------------------------------------
int open_smallsh(const char *file_path, const int mode){

 
  // mode 0 = infile, mode 1 = outfile

	// INFILE - Open source file
  // req: "If a filename was specified as the operand to the input (“<”) 
  // redirection operator, the specified file shall be opened for 
  // reading on stdin. It shall be an error if the file cannot be 
  // opened for reading or does not already exist." 
  if (mode == 0) {
    printf("infile a\n"); 
	  int sourceFD = open(file_path, O_RDONLY);
	  if (sourceFD == -1) { 
		  perror("source open()"); 
		  exit(1); 
	  }
	  // Debug - Written to terminal
	  //printf("sourceFD == %d\n", sourceFD); 

	  // Redirect stdin to source file
    // dup2(<old fd>, <new fd>) 0=stdin, 1=stdout, 2=strerr; newfd points to oldfd
	  int result = dup2(sourceFD, 0);
	  if (result == -1) { 
		  perror("source dup2()"); 
		  exit(2); 
	  }
    printf("dup success"); 
    return result; 
    //exit(1);
  }

  // OUTFILE - Open/write to target file
  // req: "If a filename was specified as the operand to the output (“>”) 
  // redirection operator, the specified file shall be opened for 
  // writing on stdout. If the file does not exist, it shall be 
  // created with permissions 0777. It shall be an error if the 
  // file cannot be opened (or created) for writing."
	if (mode == 1) {
  
	  int targetFD = open(file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	  if (targetFD == -1) { 
		  perror("target open()"); 
		  exit(1); 
	  }
	  printf("targetFD == %d\n", targetFD); // Written to terminal
  
	  // Redirect stdout to target file
	  int result = dup2(targetFD, 1);
	  if (result == -1) { 
		  perror("target dup2()"); 
		  exit(2); 
	  }
	  // Run the sort program using execlp.
	  // The stdin and stdout are pointing to files
	  execlp("sort", "sort", NULL);
	  return(0);
  }
  


  return 0; 

  // Open the file using the open system call
  // The flag O_RDWR means the file should be opened for reading and writing
  // The flag O_CREAT means that if the file doesn't exist, open should create it
  // The flag O_TRUNC means that if the file already exits, open should truncate it.
  
  
  //return 0; 
  }
//------------------------------------------------------- 
// BUILT-IN CD
//-------------------------------------------------------
int cd_smallsh(char *token_arr[], int token_arr_len){
  // req: takes one argument.
  if (token_arr_len < 2) {
    fprintf(stderr, "cd_smallsh(): too few arguments\n"); // NOTE: Likely imppossible
    return -1; 
  }
  if (token_arr_len > 3) {
    fprintf(stderr, "cd_smallsh(): too many arguments\n"); 
    return -1; 
  }

   //char *newWd[PATH_MAX];
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
    perror("chdir() failed"); // TODO: Improve this error checking?  
    //printf("%i",errno); 
    // reset errno etc?? 
    return -1;
  };
  //show cwd after cd 
  
 
  if (debug == 1) {
   int cwd_size = PATH_MAX; 
   char cwd[cwd_size]; // TODO: good size?
   getcwd(cwd, PATH_MAX);
   printf("cd_smallsh(): working directory (after cd): %s\n", cwd);
  }
  // success return 
  return 0;
}


// BUILT-IN EXIT
int exit_smallsh(char *token_arr[], int token_arr_len){

  // NOTE:  see p. 405 in LPI 

  // req: takes one argument.
  if (token_arr_len < 2) {
    fprintf(stderr, "exit_smallsh(): too few arguments\n"); // NOTE: Likely imppossible
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
  if (debug == 1) printf("this a\n");

  //kill(0, SIGINT); // int kill(pid_t pid, int sig); pid=0 sends to group

  if (debug == 1) printf("this b\n"); 

  // req: takes 1 argument. If not provided, it is implied to be
  // expansion of “$?”, the exit status of the last foreground command.
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


// NON-BUILT-INS
int execute_commands(char *token_arr[], int const token_arr_len, int const run_bg, char const *restrict infile, char const *restrict outfile){
        
    // printf("Parent pid: %d\n", getpid());
    //if (run_bg || infile || outfile) printf("---\n");  
    

    // CHECK FOR BUILT-INS (CD / EXIT_SMALLSH)  

    // built-in cd 
    if (strcmp(token_arr[0],"cd") == 0){
      cd_smallsh(token_arr, token_arr_len); // TODO check error conditions 
      return 0;
    } 
    // built-in exit 
    if (strcmp(token_arr[0],"exit") == 0){
      exit_smallsh(token_arr, token_arr_len);
      return 0; 
    }
    
    // OTHEWISE FG/BG COMMAND 

    int childStatus;

    // Fork a new process...if successful, childPid=0 in child, child's pid in parent 
    pid_t childPid = fork();

    if (debug == 1) printf("->child pid %jd\n",(intmax_t) childPid); 

    switch(childPid){

      // Fork error
      case -1:
        perror("fork() failed"); // TODO error good?
        exit(1);
        break;

      // Child process will execute this branch
      case 0: 
        // Req: 
        // Perform all redirection inside the child process (after calling fork), 
        // so that you don’t change the parent process’s file descriptors. 
        // Keep in mind, open() will assign to the lowest available file descriptor, 
        // so simply closing a standard stream file descriptor and then opening 
        // a new file is a common approach to redirection. 
        // See dup2(2) for more information about how to associate a specific open 
        // file with a specific file descriptor, in a more general sense. 
        // Either approach should be sufficient.

        // Don’t forget the mode argument to open() when using the O_CREAT flag! Very common mistake.

        if (infile){
        	// INFILE - Open source file
          // req: "If a filename was specified as the operand to the input (“<”) 
          // redirection operator, the specified file shall be opened for 
          // reading on stdin. It shall be an error if the file cannot be 
          // opened for reading or does not already exist." 
	          int sourceFD = open(infile, O_RDONLY);
	          if (sourceFD == -1) { 
		          perror("source open()"); 
		          exit(1); 
	          }
	          // Debug - Written to terminal
	          //printf("sourceFD == %d\n", sourceFD); 

	          // Redirect stdin to source filehe
            // dup2(<old fd>, <new fd>) 0=stdin, 1=stdout, 2=strerr; newfd points to oldfd
	          int result = dup2(sourceFD, 0);
	          if (result == -1) {   
		          perror("source dup2()"); 
		          exit(2); 
	          }
          //exit(1);
        }
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
        // LEFT OFF HERE 230219 - looks like outfile is working - remember to tweak file permissions etc. 
	      // Run the sort program using execlp.
	      // The stdin and stdout are pointing to files
	      //execlp("sort", "sort", NULL);
	      //return(0);
      }


        

        // OUTFILE 
        // req: If a filename was specified as the operand to the output (“>”) redirection operator, 
        // the specified file shall be opened for writing on stdout. If the file does not exist, 
        // it shall be created with permissions 0777. It shall be an error if the file cannot be 
        // opened (or created) for writing. 

        //most_rec_bg_pid = childPid;
		    if (debug == 1) printf("child (%jd) running command -- most_rec_bg_pid %jd \n", (intmax_t) getpid(), (intmax_t) most_rec_bg_pid);
        // execvp searches the PATH for the env variable with argument 1
		    execvp(token_arr[0], token_arr);
		    // exec only returns if there is an error
		    perror("execvp() failed");
        print_arr(token_arr, token_arr_len); // DEBUG test script  
		    exit(1); // TODO error good? TODO use own process to exit? 
		    break;


      // NOTE: LPI pp. ~548 helpful on waiting 

      // Parent process - childPid is pid of the child, parent will execute below 
      default: 
            
        // Foreground - '&' operator not present -> blocking wait 
        if (run_bg == 0){ 

          childPid = waitpid(0, &childStatus, 0); // TODO error for waitpid

          // check error
          if (childPid == -1){fprintf(stderr, "waitpid() failed\n"); exit(1); } // TODO error good? 

          // 1) if finished/exited...
          if(WIFEXITED(childStatus)){
            if (debug == 1) fprintf(stderr, "Child %jd exited normally with status %d\n", (intmax_t) childPid, WEXITSTATUS(childStatus));
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
            if (debug == 1) fprintf(stderr, "Child process %d stopped. Continuing.\n", childPid); // TODO: (intmax_t) childPid?
            // req: "$!"/bg shall be updated to the pid of the child process as if had been a bg command 
            most_rec_bg_pid = childPid; 
            // req: Smallsh shall no longer perform a block wait on this process, 
            // and it will continue to run in the background
            goto BACKGROUND; // TODO - ideal?
          }
        }

        // background / non-blocking wait + poll
        // NOTE: Background? record $!. Then check change of any processes before prompt.
        else if (run_bg == 1){
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
  //printf("\nAFTER #\n"); 
  //print_arr(word_arr, word_arr_len); 
  //exit(1);
  
  // 2. if last word is '&' it indicates the command run in bg
  if (strcmp(word_arr[word_arr_len-2],"&") == 0) {
    word_arr[word_arr_len-2] = NULL;
    word_arr_len -= 1; 
    run_bg = 1;

    //printf("\nAFTER &\n"); 
    //print_arr(word_arr, word_arr_len); 
    //if (run_bg == 1) printf("run bg\n"); 
    //exit(0); 
  }

  // steps 3/4 can occur in either order 
  // all other words regular words and form the command and its arguments 
  // tokens in 1-4 not included as command arguments 
  // if "<", ">", and "&" appear outside end-of-line context described above, they are treated
  // as regular arguments (see the description for e.g.) 
  

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

  // -------------v OLD VERSION v-----------------------
  // 3. if last word immediately preceded by "<" it shall be 
  // interpreted as filename operand of input redirection operator 
//  if (strcmp(word_arr[word_arr_len-3],"<") == 0) {
//    infile = word_arr[word_arr_len-2];
    //printf("we have infile(<): %s\n", infile);
    //free(word_arr[word_arr_len-2]);
//    word_arr[word_arr_len-3] = NULL;
//    word_arr_len -= 2;

    //printf("\nAFTER < \n"); 
    //print_arr(word_arr, word_arr_len); 
    //exit(0);
//  }

  // NOTE w/ 'else if' assuming '<' and '>' cannot be a filename 
  // 4. if last word immediately prceded by ">" it shall be 
  // interpreted as filename operand of output redirection operator 
//  if (strcmp(word_arr[word_arr_len-3],">") == 0) {
//    outfile = word_arr[word_arr_len-2];
    //printf("we have outfile(>): %s \n", outfile);
    //free(word_arr[word_arr_len-2]);
//    word_arr[word_arr_len-3] = NULL;
//    word_arr_len -= 2;

    //printf("\nAFTER > \n"); 
    //print_arr(word_arr, word_arr_len); 
    //exit(0);
//  }
  // -------------^ OLD VERSION ^-----------------------

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


// 2. WORD SPLITTING  
int split_words(char *line, ssize_t line_length){

  char *word_arr[line_length]; // req: pointers to strings, min 512 supported
  char delim_alt[] = " \t\n";
  char *delim = getenv("IFS");  // pointer or null   
  if (delim == NULL) delim = delim_alt; 
  //fprintf(stderr, "%s",(env_p ? env_p : ""));
  
  //char *delim[] = getenv("IFS");//{getenv("IFS") || " \t\n"};
  //if (delim == NULL)strcpy(delim, "hehe"); //" \t\n");
  //printf("delim is %s\n",delim); 
  //exit(1); 
  //char delim[] = " ";
  
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
  word_arr[n] = NULL; 

  // parse the command word array 
  // TODO consider moving this call to main()
  parse_words(word_arr, n+1);

  return 0;
}


// 1. INPUT  
int main(){

  // Main loop
  char *line = NULL;
  size_t n = 0;
  pid_t pid = getpid();
  if (debug == 1) printf("smallsh PID: (%jd)\n", (intmax_t) pid);


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


    // LPI pp. ~548 helpful on waiting 
    //    SIGNAL HANDLING 
    //    The SIGTSTP signal shall be ignored by smallsh.
    //    The SIGINT signal shall be ignored (SIG_IGN) at 
    //    all times except when reading a line of input in Step 1, 
    //    during which time it shall be registered to a signal handler 
    //    which does nothing.
    //    Explanation:
    //    SIGTSTP (CTRL-Z) normally causes a process to halt, 
    //    which is undesirable. The smallsh process should not 
    //    respond to this signal, so it sets its disposition to SIG_IGN.
    //    The SIGINT (CTRL-C) signal normally causes a process to exit 
    //    immediately, which is not desired.

  for (;;) {
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
      exit(last_fg_exit_status);  
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


// SOURCES
// [1] https://stackoverflow.com/questions/2693776/removing-trailing-newline-character-from-fgets-input

