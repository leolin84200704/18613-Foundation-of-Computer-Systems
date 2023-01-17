/**
 * @file tsh.c
 * @brief A tiny shell program with job control
 *
 * TODO: Delete this comment and replace it with your own.
 * <The line above is not a sufficient documentation.
 *  You will need to write your program documentation.
 *  Follow the 15-213/18-213/15-513 style guide at
 *  http://www.cs.cmu.edu/~213/codeStyle.html.>
 *
 * @author Your Name <andrewid@andrew.cmu.edu>
 * TODO: Include your name and Andrew ID here.
 */

#include "csapp.h"
#include "tsh_helper.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/*
 * If DEBUG is defined, enable contracts and printing on dbg_printf.
 */
#ifdef DEBUG
/* When debugging is enabled, these form aliases to useful functions */
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_requires(...) assert(__VA_ARGS__)
#define dbg_assert(...) assert(__VA_ARGS__)
#define dbg_ensures(...) assert(__VA_ARGS__)
#else
/* When debugging is disabled, no code gets generated for these */
#define dbg_printf(...)
#define dbg_requires(...)
#define dbg_assert(...)
#define dbg_ensures(...)
#endif

/* Function prototypes */
void eval(const char *cmdline);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);
void sigquit_handler(int sig);
void cleanup(void);
int builtin_command(char **argv);
pid_t Fork(void);
void unix_error(char *msg);
void Kill(pid_t pid, int signum);
pid_t Wait(int *status);
pid_t Waitpid(pid_t pid, int *iptr, int options);
void Sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
void Sigemptyset(sigset_t *set);
void Sigfillset(sigset_t *set);
void Sigaddset(sigset_t *set, int signum);
void Sigdelset(sigset_t *set, int signum);
int Sigismember(const sigset_t *set, int signum);
int Sigsuspend(const sigset_t *set);
static void sio_reverse(char s[]);
static void sio_ltoa(long v, char s[], int b);
static size_t sio_strlen(char s[]);
ssize_t sio_puts(char s[]);
ssize_t sio_putl(long v);
void sio_error(char s[]);
ssize_t Sio_putl(long v);
ssize_t Sio_puts(char s[]);
void Sio_error(char s[]);
// static struct job_t *get_job(jid_t jid);

struct job_t {
    pid_t pid;       // Job PID
    jid_t jid;       // Job ID [1, 2, ...] defined in tsh_helper.c
    job_state state; // UNDEF, BG, FG, or ST
    char *cmdline;   // Command line
};

// static struct job_t job_list[MAXJOBS]; // The job list
pid_t gpid;

/**
 * @brief <Write main's function header documentation. What does main do?>
 *
 * TODO: Delete this comment and replace it with your own.
 *
 * "Each function should be prefaced with a comment describing the purpose
 *  of the function (in a sentence or two), the function's arguments and
 *  return value, any error cases that are relevant to the caller,
 *  any pertinent side effects, and any assumptions that the function makes."
 */
int main(int argc, char **argv) {
    char c;
    char cmdline[MAXLINE_TSH]; // Cmdline for fgets
    bool emit_prompt = true;   // Emit prompt (default)

    // Redirect stderr to stdout (so that driver will get all output
    // on the pipe connected to stdout)
    if (dup2(STDOUT_FILENO, STDERR_FILENO) < 0) {
        perror("dup2 error");
        exit(1);
    }

    // Parse the command line
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
        case 'h': // Prints help message
            usage();
            break;
        case 'v': // Emits additional diagnostic info
            verbose = true;
            break;
        case 'p': // Disables prompt printing
            emit_prompt = false;
            break;
        default:
            usage();
        }
    }

    // Create environment variable
    if (putenv("MY_ENV=42") < 0) {
        perror("putenv error");
        exit(1);
    }

    // Set buffering mode of stdout to line buffering.
    // This prevents lines from being printed in the wrong order.
    if (setvbuf(stdout, NULL, _IOLBF, 0) < 0) {
        perror("setvbuf error");
        exit(1);
    }

    // Initialize the job list
    init_job_list();

    // Register a function to clean up the job list on program termination.
    // The function may not run in the case of abnormal termination (e.g. when
    // using exit or terminating due to a signal handler), so in those cases,
    // we trust that the OS will clean up any remaining resources.
    if (atexit(cleanup) < 0) {
        perror("atexit error");
        exit(1);
    }

    // Install the signal handlers
    Signal(SIGINT, sigint_handler);   // Handles Ctrl-C
    Signal(SIGTSTP, sigtstp_handler); // Handles Ctrl-Z
    Signal(SIGCHLD, sigchld_handler); // Handles terminated or stopped child

    Signal(SIGTTIN, SIG_IGN);
    Signal(SIGTTOU, SIG_IGN);

    Signal(SIGQUIT, sigquit_handler);

    // Execute the shell's read/eval loop
    while (true) {
        if (emit_prompt) {
            sio_printf("%s", prompt);

            // We must flush stdout since we are not printing a full line.
            fflush(stdout);
        }

        if ((fgets(cmdline, MAXLINE_TSH, stdin) == NULL) && ferror(stdin)) {
            perror("fgets error");
            exit(1);
        }

        if (feof(stdin)) {
            // End of file (Ctrl-D)
            printf("\n");
            return 0;
        }

        // Remove any trailing newline
        char *newline = strchr(cmdline, '\n');
        if (newline != NULL) {
            *newline = '\0';
        }

        // Evaluate the command line
        eval(cmdline);
        fflush(stdout);
    }

    return -1; // control never reaches here
}

/**
 * @brief This method evaluates the input, decides if the function is built-in,
 * fg, or bg and direct to other function Wait for the fg job to finish and
 * return to main method.
 *
 * NOTE: The shell is supposed to be a long-running process, so this function
 *       (and its helpers) should avoid exiting on error.  This is not to say
 *       they shouldn't detect and print (or otherwise handle) errors!
 */
void eval(const char *cmdline) {
    sigset_t mask_all, prev_one, mask_one;
    char buf[MAXLINE];
    // pid_t pid;

    parseline_return parse_result;
    struct cmdline_tokens token;

    strcpy(buf, cmdline);

    // Parse command line
    parse_result = parseline(cmdline, &token);
    // printf("%d\n", parse_result);
    if (parse_result == PARSELINE_ERROR || parse_result == PARSELINE_EMPTY) {
        return;
    }
    Sigfillset(&mask_all);
    Sigemptyset(&mask_one);
    Sigaddset(&mask_one, SIGCHLD);
    Sigaddset(&mask_one, SIGINT);
    Sigaddset(&mask_one, SIGTSTP);
    Sigaddset(&mask_one, SIGTERM);
    if (!builtin_command(token.argv)) {
        Sigprocmask(SIG_BLOCK, &mask_one, &prev_one);
        if ((gpid = Fork()) == 0) {
            Sigprocmask(SIG_SETMASK, &prev_one, NULL);
            setpgid(gpid, gpid);
            if (execve(token.argv[0], token.argv, environ) < 0) {
                printf("%s: Command not found.\n", token.argv[0]);
                exit(1);
            }
        }
        Sigprocmask(SIG_BLOCK, &mask_all, NULL);

        if (parse_result != PARSELINE_BG) {
            add_job(gpid, FG, cmdline);
            gpid = 0;
            while (fg_job()) {
                Sigsuspend(&prev_one);
            }
            Sigprocmask(SIG_SETMASK, &prev_one, NULL);
            // printf("done\n");
        } else {
            // printf("backend\n");
            add_job(gpid, BG, cmdline);
            // Sio_puts();
            sio_printf("[%d] (%d) %s\n", job_from_pid(gpid), gpid, cmdline);
            // printf("[%d] (%d) %s\n", getjobid(gpid), gpid, cmdline);
            Sigprocmask(SIG_SETMASK, &prev_one, NULL);
        }

        return;
    } else {
        if (token.builtin == BUILTIN_JOBS) {
            // int fd;
            Sigprocmask(SIG_BLOCK, &mask_all, NULL);
            list_jobs(STDERR_FILENO);
            Sigprocmask(SIG_SETMASK, &prev_one, NULL);
        }
    }

    // TODO: Implement commands here.
}

/*****************
 * Signal handlers
 *****************/

/**
 * @brief This function will notify and reaps the child after a child process
 * terminates, It will reap all the zombie children.
 *
 */
void sigchld_handler(int sig) {
    int olderrno = errno;
    int status;
    sigset_t mask_all, prev_all;
    Sigfillset(&mask_all);
    while ((gpid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
        if (WIFEXITED(status)) {
            delete_job(job_from_pid(gpid));
        } else if (WIFSTOPPED(status)) {
            sio_printf("Job [%d] (%d) stopped by signal %d\n",
                       job_from_pid(gpid), gpid, WSTOPSIG(status));
            job_set_state(job_from_pid(gpid), ST);
        } else if (WTERMSIG(status)) {
            sio_printf("Job [%d] (%d) terminated by signal %d\n",
                       job_from_pid(gpid), gpid, WTERMSIG(status));
            delete_job(job_from_pid(gpid));
        }

        // delete_job(job_from_pid(gpid));
        Sigprocmask(SIG_SETMASK, &prev_all, NULL);
    }

    errno = olderrno;
}

/**
 * @brief When the user inserts Ctrl + c, SigInt will terminate all the fg job
 * and return to main method.
 *
 *
 *
 * TODO: Delete this comment and replace it with your own.
 */
void sigint_handler(int sig) {
    int olderrno = errno;
    sigset_t mask_all, prev_all;
    Sigfillset(&mask_all);
    Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
    jid_t fgjob = fg_job();
    if (fgjob) {
        kill(-job_get_pid(fgjob), sig);
        Sigprocmask(SIG_SETMASK, &prev_all, NULL);
    }
    errno = olderrno;
}

/**
 * @brief When the user inserts Ctrl + z, SigInt will terminate all the fg job
 * and return to main method.
 *
 * TODO: Delete this comment and replace it with your own.
 */
void sigtstp_handler(int sig) {
    int olderrno = errno;
    sigset_t mask_all, prev_all;
    Sigfillset(&mask_all);
    Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
    jid_t fgjob = fg_job();
    if (fgjob) {
        kill(-job_get_pid(fgjob), sig);
        Sigprocmask(SIG_SETMASK, &prev_all, NULL);
    }
    errno = olderrno;
}

/**
 * @brief Attempt to clean up global resources when the program exits.
 *
 * In particular, the job list must be freed at this time, since it may
 * contain leftover buffers from existing or even deleted jobs.
 */
void cleanup(void) {
    // Signals handlers need to be removed before destroying the joblist
    Signal(SIGINT, SIG_DFL);  // Handles Ctrl-C
    Signal(SIGTSTP, SIG_DFL); // Handles Ctrl-Z
    Signal(SIGCHLD, SIG_DFL); // Handles terminated or stopped child

    destroy_job_list();
}

int builtin_command(char **argv) {
    if (!strcmp(argv[0], "quit")) {
        exit(0);
    }
    if (!strcmp(argv[0], "&") || !strcmp(argv[0], "jobs") ||
        !strcmp(argv[0], "bg") || !strcmp(argv[0], "fg")) {
        return 1;
    }
    return 0;
}

void unix_error(char *msg) {
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

pid_t Fork(void) {
    pid_t pid;

    if ((pid = fork()) < 0)
        unix_error("Fork error");
    return pid;
}

void Kill(pid_t pid, int signum) {
    int rc;

    if ((rc = kill(pid, signum)) < 0)
        unix_error("Kill error");
}

/* $begin wait */
pid_t Wait(int *status) {
    pid_t pid;

    if ((pid = wait(status)) < 0)
        unix_error("Wait error");
    return pid;
}
/* $end wait */

pid_t Waitpid(pid_t pid, int *iptr, int options) {
    pid_t retpid;

    if ((retpid = waitpid(pid, iptr, options)) < 0)
        unix_error("Waitpid error");
    return (retpid);
}

void Sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
    if (sigprocmask(how, set, oldset) < 0)
        unix_error("Sigprocmask error");
    return;
}

void Sigemptyset(sigset_t *set) {
    if (sigemptyset(set) < 0)
        unix_error("Sigemptyset error");
    return;
}

void Sigfillset(sigset_t *set) {
    if (sigfillset(set) < 0)
        unix_error("Sigfillset error");
    return;
}

void Sigaddset(sigset_t *set, int signum) {
    if (sigaddset(set, signum) < 0)
        unix_error("Sigaddset error");
    return;
}

void Sigdelset(sigset_t *set, int signum) {
    if (sigdelset(set, signum) < 0)
        unix_error("Sigdelset error");
    return;
}

int Sigismember(const sigset_t *set, int signum) {
    int rc;
    if ((rc = sigismember(set, signum)) < 0)
        unix_error("Sigismember error");
    return rc;
}

int Sigsuspend(const sigset_t *set) {
    int rc = sigsuspend(set); /* always returns -1 */
    if (errno != EINTR)
        unix_error("Sigsuspend error");
    return rc;
}

static void sio_reverse(char s[]) {
    int c, i, j;

    for (i = 0, j = strlen(s) - 1; i < j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

/* sio_ltoa - Convert long to base b string (from K&R) */
static void sio_ltoa(long v, char s[], int b) {
    int c, i = 0;
    int neg = v < 0;

    if (neg)
        v = -v;

    do {
        s[i++] = ((c = (v % b)) < 10) ? c + '0' : c - 10 + 'a';
    } while ((v /= b) > 0);

    if (neg)
        s[i++] = '-';

    s[i] = '\0';
    sio_reverse(s);
}

/* sio_strlen - Return length of string (from K&R) */
static size_t sio_strlen(char s[]) {
    int i = 0;

    while (s[i] != '\0')
        ++i;
    return i;
}
/* $end sioprivate */

/* Public Sio functions */
/* $begin siopublic */

ssize_t sio_puts(char s[]) /* Put string */
{
    return write(STDOUT_FILENO, s, sio_strlen(s)); // line:csapp:siostrlen
}

ssize_t sio_putl(long v) /* Put long */
{
    char s[128];

    sio_ltoa(v, s, 10); /* Based on K&R itoa() */ // line:csapp:sioltoa
    return sio_puts(s);
}

void sio_error(char s[]) /* Put error message and exit */
{
    sio_puts(s);
    _exit(1); // line:csapp:sioexit
}
/* $end siopublic */

/*******************************
 * Wrappers for the SIO routines
 ******************************/
ssize_t Sio_putl(long v) {
    ssize_t n;

    if ((n = sio_putl(v)) < 0)
        sio_error("Sio_putl error");
    return n;
}

ssize_t Sio_puts(char s[]) {
    ssize_t n;

    if ((n = sio_puts(s)) < 0)
        sio_error("Sio_puts error");
    return n;
}

void Sio_error(char s[]) {
    sio_error(s);
}
