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


//-------------------------------------------------------
//SIGNAL HANDLERS
//-------------------------------------------------------
/* Our signal handler for SIGINT */
/* Our signal handler for SIGINT */
void handle_SIGINT(){
	char* message = "Caught SIGINT, sleeping for 3 seconds\n";
  //We are using write rather than printf
  write(STDOUT_FILENO, message, 39);
	sleep(3);
}
int there(){
  printf("There\n"); 
  return 0;
}

int main(){
    // SIGNAL
    // -------------------
    // If reading is interrupted by a signal (see signal handling), a newline shall be printed, 
    // then a new command prompt shall be printed (including checking for background processes), 
    // and reading a line of input shall resume. (See CLEARERR(3), and don’t forget to reset errno).

    // From https://canvas.oregonstate.edu/courses/1901764/pages/exploration-signal-handling-api?module_item_id=22777109
	  


	  // Initialize SIGINT_action struct to be empty
	  struct sigaction SIGINT_action = {0}, ignore_action = {0}; 

	  // Fill out the SIGINT_action struct
	  // Register handle_SIGINT as the signal handler
	  SIGINT_action.sa_handler = handle_SIGINT;
	  // Block all catchable signals while handle_SIGINT is running
	  sigfillset(&SIGINT_action.sa_mask);
	  // No flags set
	  SIGINT_action.sa_flags = 0;
	  // Install our signal handler
	  sigaction(SIGINT, &SIGINT_action, NULL);

    
    // The ignore_action struct as SIG_IGN as its signal handler
	  ignore_action.sa_handler = SIG_IGN;


	  // Register the ignore_action as the handler for SIGTSTP
	  // So all three of these signals will be ignored.
	  sigaction(SIGTSTP, &ignore_action, NULL);



	  //printf("Send the signal SIGINT to this process by entering Control-C. That will cause the signal handler to be invoked\n");
	  fflush(stdout);




    // MAIN LOOP
    printf("Here\n"); 
    for(;;){
        char *line = NULL;
    size_t n = 0;
      // req: print a prompt to stderr by expanding the PS1 
      const char *env_p = getenv("PS1");  // pointer or null   
      fprintf(stderr, "%s",(env_p ? env_p : ""));

      //char *buf[300]; 
      getline(&line, &n, stdin);
      
      //for (int i =0; i < len; i++){
      //  printf(line[i]); 
      //}
      printf("%s\n", line);

      free(line); 
 
	    // Calling pause() causes the process to sleep until it is sent a signal.
	    // Enter Control-C to end pause
	    //pause();
	    // If pause is ended by SIGINT, then after handle_SIGINT ends, the execution
	    // proceeds with the following statement.
	    //printf("pause() ended. The process will now end.\n");
	    //-------------------------------
       
    }
    return 0;
}

// LEFT OFF HERR GETTING SEG FAULT WITH ENTRY OF LINE, THEN SIGINT 
