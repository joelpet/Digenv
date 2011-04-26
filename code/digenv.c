/* TODO Lägg till kodstyckeskommentarer där det är nödvändigt, enligt http://www.ict.kth.se/courses/ID2206/LAB/cdok */
/* TODO Låt eventuellt några av de nuvarande kommentarerna ersättas av sådana kodstyckeskommentarer. */

/*
 * NAME:
 *  digenv  -   study your environment variables
 *
 * SYNTAX:
 *  digenv [parameters]
 *
 * DESCRIPTION:
 *  Digenv displays your environment variables sorted in a pager, optionally
 *  filtered through `grep` with the given input parameters, if any present. If
 *  $PAGER is present, digenv will try to use that command as pager, otherwise
 *  it tries `less` and thereafter falls back to `more`
 *
 * OPTIONS:
 *  See grep(1). All parameters will be passed directly to `grep`.
 *
 * EXAMPLES:
 *  Simply display all environment variables sorted in a pager:
 *  $ digenv
 *
 *  Display all environment variables with a name containing "user" (-i means
 *  ignore case):
 *  $ digenv -i user
 *
 * ENVIRONMENT:
 *  PAGER       The command to execute for launching a pager.
 *
 * SEE ALSO:
 *   printenv(1), grep(1), sort(1), less(1), more(1)
 *
 * DIAGNOSTICS:
 *  The exit status is 0 if everything went fine, 1 if any system call
 *  failed, e.g. creating a pipe or executing a file, or if `grep` did not find
 *  anything, and 2 if `grep` failed or a child was terminated by a signal.
 *
 * NOTES:
 *  The exit statuses could be refined in order to better indicate exactly what
 *  went wrong.
 */

/* digenv
 *
 * This module contains the whole digenv program, including main() and
 * functions for error checking and closing pipes.
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define PIPE_READ_SIDE 0
#define PIPE_WRITE_SIDE 1

/*
 * Define a pipe file descriptor type for neater code.
 */
typedef int pipe_fd_t[2];

void check_error(int, char*);
void _check_error(int, char*, int);
void close_pipes(int, pipe_fd_t*);


/* main
 *
 * main creates the necessary pipes and invokes the filters.
 */
int main(
        int argc,       /* number of given arguments */
        char** argv)    /* array of argument char arrays */
{

    pid_t childpid;
    /* Array of three pipe file descriptors. */
    pipe_fd_t pipe_fds[3];
    int num_filters = argc > 1 ? 4 : 3;
    int num_pipes = num_filters - 1;
    int return_value;
    int status;
    int exit_code = 0;
    int i;
    /* The index of the pipe_fd that follows the current filter. */
    int cur_pipe = 0;
    char* pager;

    if (argc > 1) {
        num_pipes = 3;
        num_filters = 4;
    } else {
        num_pipes = 2;
        num_filters = 3;
    }

    /* Create pipes. */
    for (i = 0; i < num_pipes; ++i) {
        return_value = pipe(pipe_fds[i]);
        check_error(return_value, "Could not initialize pipe.\n");
    }

    /* Create child process that will eventually execute `printenv`. */
    childpid = fork();

    if (0 == childpid) {
        /* Replace stdout med duplicated write side of the pipe. */
        return_value = dup2(pipe_fds[cur_pipe][PIPE_WRITE_SIDE], STDOUT_FILENO);
        check_error(return_value,  "printenv&: Could not duplicate write side of first pipe.\n");
    
        /* printenv should not read from pipe -- close the read side. */
        return_value = close(pipe_fds[cur_pipe][PIPE_READ_SIDE]);
        check_error(return_value,  "Could not close read side of pipe.\n");

        /* stdout now points to write side of pipe -- close duplicated file desc. */
        return_value = close(pipe_fds[cur_pipe][PIPE_WRITE_SIDE]);
        check_error(return_value,  "Could not close write side of pipe.\n");

        /* Execute printenv and give it "printenv" as first parameter. */
        (void) execlp("printenv", "printenv", (char *) 0);

        fprintf(stderr, "Could not execute printenv.\n");
        exit(1);
    }

    check_error(childpid,  "Could not fork printenv.\n");

    /* Forking `printenv` went fine. */
    /* Done with one filter, move cur_pipe one step ahead. */
    ++cur_pipe; /* == 1 */

    /* If arguments where given, use grep with those. */
    if (argc > 1) {
        /* Create child process that will eventually execute `grep`. */
        childpid = fork();

        if (0 == childpid) {
            return_value = dup2(pipe_fds[cur_pipe-1][PIPE_READ_SIDE], STDIN_FILENO);
            check_error(return_value, "grep&: Could not duplicate read side of pipe.\n");

            return_value = close(pipe_fds[cur_pipe-1][PIPE_WRITE_SIDE]);
            check_error(return_value, "grep&: Could not close write side of pipe.");

            return_value = dup2(pipe_fds[cur_pipe][PIPE_WRITE_SIDE], STDOUT_FILENO);
            check_error(return_value, "grep&: Could not duplicate write side of pipe.\n");

            return_value = close(pipe_fds[cur_pipe][PIPE_READ_SIDE]);
            check_error(return_value, "grep&: Could not close read side of second pipe.");
            
            /* digenv is longer than "grep", so no buffer overflow */
            argv[0] = "grep";
            (void) execvp("grep", argv);

            fprintf(stderr, "Could not execute grep.\n");
            exit(1);
        }

        check_error(childpid,  "Could not fork grep.\n");
 
        /* Another filter done -- increment cur_pipe and close pipes. */
        ++cur_pipe;
        close_pipes(cur_pipe, pipe_fds);
    }

    /* Create child process that will eventually execute `sort`. */
    childpid = fork();

    if (0 == childpid) {
        /* Replace stdin with duplicated read side of first pipe. */
        return_value = dup2(pipe_fds[cur_pipe-1][PIPE_READ_SIDE], STDIN_FILENO);
        check_error(return_value, "sort&: Could not duplicate read side of first pipe.\n");

        /* sort should not write to first pipe -- close the write side. */
        return_value = close(pipe_fds[cur_pipe-1][PIPE_WRITE_SIDE]);
        check_error(return_value, "sort&: Could not close write side of first pipe.");

        /* Replace stdout with duplicated write side of second pipe. */
        return_value = dup2(pipe_fds[cur_pipe][PIPE_WRITE_SIDE], STDOUT_FILENO);
        check_error(return_value, "sort&: Could not duplicate write side of second pipe.\n");

        /* sort should not read from second pipe, but stdin -- close the read side. */
        return_value = close(pipe_fds[cur_pipe][PIPE_READ_SIDE]);
        check_error(return_value, "sort&: Could not close read side of second pipe.");

        /* Execute sort and give it "sort" as first parameter. */
        (void) execlp("sort", "sort", (char *) 0);

        fprintf(stderr, "Could not execute sort.\n");
        exit(1);
    }

    check_error(childpid,  "Could not fork sort.\n");

    /* Forking `sort` went fine. */
    /* Another filter forked -- move cur_pipe forward and close pipes if necessary. */
    ++cur_pipe;
    close_pipes(cur_pipe, pipe_fds);

    /* Create child process that will eventually execute `less`. */
    childpid = fork();

    if (0 == childpid) {
        /* Replace stdin with duplicated read side of previous pipe. */
        return_value = dup2(pipe_fds[cur_pipe-1][PIPE_READ_SIDE], STDIN_FILENO);
        check_error(return_value, "less&: Could not duplicate read side of pipe.\n");

        /* less should not write to write side of previous pipe -- close it. */
        return_value = close(pipe_fds[cur_pipe-1][PIPE_WRITE_SIDE]);
        check_error(return_value, "less&: Could not close write side of pipe.\n");

        /* less uses STDIN_FILENO to read from pipe, so we can close this. */
        return_value = close(pipe_fds[cur_pipe-1][PIPE_READ_SIDE]);
        check_error(return_value, "less&: Could not close write side of pipe.\n");

        pager = getenv("PAGER");
        /* Try first with $PAGER (if set), then "less" and then with "more". */
        if (NULL != pager) {
            (void) execlp(pager, pager, (char *) 0);
        }
        (void) execlp("less", "less", (char *) 0);
        (void) execlp("more", "more", (char *) 0);

        fprintf(stderr, "Could not execute pager.\n");
        exit(1);
    }

    check_error(childpid, "Could not fork.\n");

    /* Forking `less` went fine. */
    /* Another filter forked -- increment cur_pipe and close pipes if necessary. */
    ++cur_pipe;
    close_pipes(cur_pipe, pipe_fds);

    /* Wait for filter children to exit and check their exit statuses. */
    for (i = 0; i < num_filters; ++i) {
        childpid = wait(&status);
        check_error(childpid, "wait() failed unexpectedly.\n");

        if (WIFEXITED(status)) {
            /* Child terminated normally, by calling exit(), _exit() or by return from main(). */
            int child_status = WEXITSTATUS(status);
            if (0 != child_status && 0 == exit_code) {
                /* 
                 * Child terminated with non-zero status, indicating some
                 * problem. However, it does not have to be fatal; grep exits
                 * with status 1 if no matches were found. Save exit status.
                 */
                exit_code = child_status;
            }
        } else {
            if (WIFSIGNALED(status) && 0 == exit_code) {
                /* Child was terminated by a signal (WTERMSIG(status)). */
                /* Exit with non-zero status to indicate this. */
                exit_code = 2;
            }
        }
    }

    /* Exit with first non-zero child exit status, or 0 if everything went fine. */
    exit(exit_code);
}


/* check_error
 *
 * check_error calls _check_error() with a predefined exit code.
 */
void check_error(
        int return_value,   /* return value to check for -1 value */
        char* error_prefix) /* short string to prefix the error message with */
{
    _check_error(return_value, error_prefix, 1);
}

/* _check_error
 *
 * _check_error checks if return_value is -1 and takes appropriate actions,
 * such as printing an error message and exiting with the given exit_code.
 */
void _check_error(
        int return_value,   /* return value to check for -1 value */
        char* error_prefix, /* short string to prefix the error message with */
        int exit_code)      /* the code to exit the program with */
{
    if (-1 == return_value) {
        perror(error_prefix);
        exit(exit_code);
    }
}

/* close_pipes
 *
 * close_pipes closes pipes that pressumably are not going to be used.
 */
void close_pipes(
        int cur_pipe,           /* index of the pipe in pipe_fds that is going
                                   to be initiated next */
        pipe_fd_t* pipe_fds)    /* array of pipe file descriptors */
{
    int return_value;
    if (cur_pipe >= 2) {
        return_value = close(pipe_fds[cur_pipe-2][PIPE_WRITE_SIDE]);
        check_error(return_value, "Could not close write side of pipe.\n");
        return_value = close(pipe_fds[cur_pipe-2][PIPE_READ_SIDE]);
        check_error(return_value, "Could not close read side of pipe.\n");
    }
}
