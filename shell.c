#include <ctype.h>       // Include character type functions
#include <unistd.h>      // Include POSIX operating system API
#include <stdio.h>       // Include standard input/output library
#include <stdlib.h>      // Include standard library for memory allocation, process control, etc.
#include <string.h>      // Include string handling functions
#include <sys/param.h>   // Include system parameters
#include <sys/types.h>   // Include basic data types
#include <sys/wait.h>    // Include declarations for waiting

// Structure to keep track of child processes (jobs)
struct job {
    pid_t pid;          // Process ID
    char *name;         // Name of the job
};

// List structure to keep track of all child jobs
struct child_jobs {
    size_t len;         // Number of jobs currently running or not cleared
    size_t cap;         // Capacity - maximum number of jobs that can be stored
    struct job jobs[];  // Flexible array member to hold the jobs
} *child_jobs = NULL;   // Global pointer to the list of child jobs

// Function to remove a job by its PID
void free_job_by_pid(pid_t pid) {
    if (child_jobs != NULL && child_jobs->len > 0) {
        for (size_t i = 0, len = child_jobs->len; i < len; i++) {
            struct job *curr_job = &child_jobs->jobs[i];
            if (curr_job->pid == pid) {
                free(curr_job->name); // Free the dynamically allocated job name
                // Shift the last job to the current position if not the last one
                if (i < len - 1) {
                    memcpy(curr_job, &child_jobs->jobs[len - 1], sizeof(struct job));
                }
                child_jobs->len -= 1; // Decrement the number of jobs
            }
        }
    }
}

// Function to find a job by its PID
struct job *find_job_by_pid(pid_t pid) {
    if (child_jobs != NULL && child_jobs->len > 0) {
        for (size_t i = 0, len = child_jobs->len; i < len; i++) {
            struct job *curr_job = &child_jobs->jobs[i];
            if (curr_job->pid == pid) {
                return curr_job; // Return the found job
            }
        }
    }
    return NULL; // Return NULL if the job was not found
}

// Function to add a job to the list
void add_job(const struct job *j) {
    if (child_jobs == NULL) {
        // Allocate initial memory for the child_jobs structure
        child_jobs = malloc(2 * sizeof(size_t) + 4 * sizeof(struct job));
        child_jobs->len = 0;
        child_jobs->cap = 4;
    }

    size_t len = child_jobs->len, cap = child_jobs->cap;
    
    // Expand the jobs array if necessary
    if (len >= cap) {
        child_jobs = realloc(child_jobs, sizeof(size_t) * 2 + sizeof(struct job) * (len + 4));
        child_jobs->cap += 4; // Increase the capacity
    }
    
    struct job *new_job = &child_jobs->jobs[len]; // Pointer to the new job
    new_job->pid = j->pid;                        // Set the new job's PID
    new_job->name = strdup(j->name);              // Copy the job's name (dynamically allocated)

    child_jobs->len += 1; // Increment the number of jobs
}

// Function to free all jobs, releasing memory resources
void free_jobs(void) {
    if (child_jobs != NULL && child_jobs->len > 0) {
        for (size_t i = 0, len = child_jobs->len; i < len; i++) {
            struct job *curr_job = &child_jobs->jobs[i];
            free(curr_job->name); // Free each job's name memory
        }
    }
    free(child_jobs); // Free the entire job list structure
    child_jobs = NULL; // Set the global pointer to NULL for safety
}

// Structure to store command details
struct command {
    char *cmd;    // Command name
    char *argv[]; // Argument vector, including command as first argument
};

// Enumeration to identify the type of command parsed
enum command_type {
    FAIL,        // Parsing failed
    SPACES,      // Command contains only spaces
    FOREGROUND,  // Foreground execution command
    BACKGROUND   // Background execution command
};

// Function to generate a command structure based on the user input
enum command_type gen_command(char *line, ssize_t len, struct command **c) {
    // Variables for parsing logic
    int i = 0, start = 0;
    size_t num_args = 0;
    int is_background = 0;

    // Parsing input to identify command type and arguments
    for (i = 0; i < len; ++i) {
        // Fail on non-printable/non-space characters
        if (!isprint(line[i]) && !isspace(line[i])) {
            return FAIL;
        }
        // Detect background execution symbol '&'
        if (line[i] == '&') {
            is_background = 1;
            break;
        } else if ((isspace(line[i + 1]) || line[i + 1] == '\0') && !isspace(line[i])) {
            num_args++; // Counting arguments based on spaces
        }
    }

    if (num_args == 0) {
        return SPACES; // No arguments, only spaces
    }

    // Allocate memory for command structure based on argument count
    *c = malloc(sizeof(struct command) + sizeof(char *) * (num_args + 2));

    // Extracting arguments and setting them in the command structure
    size_t count = 0;
    for (i = 0; i < len; ++i) {
        if (isspace(line[i]) && !isspace(line[i + 1])) {
            start = i + 1; // Start of a new argument
        } else if (!isspace(line[i]) && (isspace(line[i + 1]) || line[i + 1] == '\0')) {
            if (count == 0) {
                // Setting command name for the first argument
                (*c)->cmd = strndup(&line[start], i - start + 1);
            }
            // Storing arguments in the array
            (*c)->argv[count] = strndup(&line[start], i - start + 1);
            count++;
        }
    }
    (*c)->argv[num_args] = NULL; // Null-terminate the argument vector

    return is_background ? BACKGROUND : FOREGROUND; // Return command type
}

// Function to free memory allocated for a command structure
void free_command(struct command *c) {
    if (c) {
        free(c->cmd); // Free the command name
        for (int i = 0; c->argv[i]; i++) {
            free(c->argv[i]); // Free each argument string
        }
        free(c); // Free the command structure itself
    }
}

// Enumeration for built-in shell commands
enum built_ins {
    EXIT,     // Exit command
    PID,      // Print the current process ID
    PPID,     // Print the parent process ID
    CD,       // Change directory command
    PWD,      // Print working directory command
    JOBS,     // List background jobs
    NOT       // No built-in command executed
};

// Function to execute built-in shell commands or identify if a command is not built-in
enum built_ins run_built_in(const struct command *c) {
    // If command is "exit", signal to exit the shell
    if (!strcmp(c->cmd, "exit")) {
        return EXIT;
    }

    // If command is "pid", print the shell's process ID
    if (!strcmp(c->cmd, "pid")) {
        printf("Shell pid: %d\n", getpid());
        return PID;
    }

    // If command is "ppid", print the shell's parent process ID
    if (!strcmp(c->cmd, "ppid")) {
        printf("Shell's Parent pid: %d\n", getppid());
        return PPID;
    }

    // If command is "cd", change the current directory
    if(!strcmp(c->cmd, "cd") && !strcmp(c->argv[0], "cd")) {
        int fail = 0;
        // If an argument is provided, attempt to change to that directory; otherwise, change to HOME
        if(c->argv[1] != NULL) {
            fail = chdir(c->argv[1]);
        } 
        else {
            fail = chdir(getenv("HOME"));
        }
        // If changing directory fails, print an error message
        if (fail) {
            perror("cd");
        }
        return CD;
    }

    // If command is "pwd", print the current working directory
    if (!strcmp(c->cmd, "pwd")) {
        char cwd[MAXPATHLEN];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("%s\n", cwd);
        } else {
            perror("cd");
        }
        return PWD;
    }

    // If command is "jobs", list all background jobs
    if(!strcmp(c->cmd, "jobs")) {
        size_t i, len = child_jobs->len;
        for(i = 0; i < len; i++){
            struct job *curr = &child_jobs->jobs[i];
            printf("[%d] %s\n", curr->pid, curr->name);
        }
        return JOBS;
    }

    // If none of the above commands match, return NOT to indicate it's not a built-in command
    return NOT;
}

// Main function: the entry point of the shell program
int main(int argc, char *argv[]) {
    struct command *c = NULL;  // Pointer to store the command structure
    char *line = NULL,         // Buffer for the input line
         *prompt = "308sh> ";  // Default prompt string
    size_t thats_cap = 0;      // Capacity for getline function
    ssize_t len;               // Length of the input line
    int error = 0;             // Error flag
    int status;                // Status of the child process
    pid_t pid;                 // Process ID
    enum command_type type = FAIL;  // Type of the command

    // Validate command-line arguments for setting a custom prompt
    if (argc != 1 && argc != 3) {
        printf("Incorrect usage: \n./shell [-p prompt]\n");
        goto Exit;
    } else if (argc == 3 && !strcmp(argv[1], "-p")) {
        prompt = argv[2];  // Set the custom prompt
    }

    // Main loop of the shell
    while (1) {
        usleep(1000);  // Delay for stability
        printf("%s", prompt);  // Display the prompt
        // Read the command line
        if ((len = getline(&line, &thats_cap, stdin)) <= 0) {
            fprintf(stderr, "Failed to read command line\n");
            error = -1;
            continue;  // Continue to the next iteration on error
        }
        
        // Parse the input line into a command structure
        if ((type = gen_command(line, len, &c)) == FAIL) {
            fprintf(stderr, "Could not parse command from line\n");
            error = -2;
            goto FreeLine;  // Free resources if parsing fails
        }

        // Skip execution if the line contains only spaces
        if (type == SPACES) {
            goto FreeLine;
        }
        
        // Execute built-in commands, if any
        switch (run_built_in(c)) {
            default:
                goto FreeCommand;  // Free resources for non-built-in commands
            case EXIT:
                goto ExitCommand;  // Exit the shell for the EXIT command
            case NOT:
               break;  // Continue to process non-built-in commands
        }

        // Create a child process for non-built-in commands
        if ((pid = fork()) < 0) {
            perror("Fork Failed");
            error = -3;
            goto FreeCommand;  // Handle fork failure
        } else if (pid == 0) {  // Child process
            // Execute the command
            printf(">>> [%d] %s\n", getpid(), c->cmd);
            execvp(c->cmd, c->argv);
            perror("Command Not Found");
            error = -4;
            goto ExitCommand;
        } else if (type == FOREGROUND) {  // Parent process: foreground execution
            // Wait for the child process to complete
            waitpid(pid, &status, 0);
            // Report the exit status
            if(WIFEXITED(status)){
                printf(">>> [%d] %s Exited %d\n", pid, c->cmd, WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)){
                printf(">>> [%d] %s Killed %d\n", pid, c->cmd, WTERMSIG(status));
            } else if (WIFSTOPPED(status)){
                printf(">>> [%d] %s Stopped %d\n", pid, c->cmd, WSTOPSIG(status));
            } else if (WIFCONTINUED(status)){
                printf(">>> [%d] %s Continued %d\n", pid, c->cmd, status);
            }
        } else {  // Parent process: background execution
            // Add the background job to the job list
            struct job j = {.pid = pid, .name = c->cmd};
            add_job(&j);
        }

        // Label to free the command structure
FreeCommand:
        free_command(c);
        c = NULL;

        // Label to free the input line
FreeLine:
        free(line);
        line = NULL;

        // Check for completed background processes and remove them from the job list
        while((child = waitpid(-1, &status, WNOHANG)) > 0) {
            struct job *j = find_job_by_pid(child);
            if (j != NULL) {
                // Report the status of background jobs
                if(WIFEXITED(status)){
                    printf(">>> [%d] %s Exited %d\n", child, j->name, WEXITSTATUS(status));
                } else if (WIFSIGNALED(status)){
                    printf(">>> [%d] %s Killed %d\n", child, j->name, WTERMSIG(status));
                } else if (WIFSTOPPED(status)){
                    printf(">>> [%d] %s Stopped %d\n", child, j->name, WSTOPSIG(status));
                } else if (WIFCONTINUED(status)){
                    printf(">>> [%d] %s Continued %d\n", child, j->name, status);
                }
                free_job_by_pid(child);
            }
        }

    }  // End of main loop

    // Decide what to needs to freed based on if they are allocated or not
    if(c == NULL && line == NULL) {
        goto Exit;
    } else if(c == NULL) { 
        // Line must be alloced if we get the command output
        goto ExitLine;
    }

// Multiple ways to exit depending on what needs to be deallocated
ExitCommand:
    free_command(c);

ExitLine:    
    free(line);

Exit:
    // Helps to free global jobs list
    free_jobs();
    return error;
}
