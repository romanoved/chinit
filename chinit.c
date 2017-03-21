#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <sched.h>
#include <unistd.h>
#include <getopt.h>


/* simple logging helpers */

static struct {
    char* main_cmd;
    int is_exec;
    char* sub_cmdline;
} logger_data;

void log_exit_failure(const char* msg)
{
    fprintf(stderr,"%s%s: %s (%s) cmd '%s'\n",
        logger_data.main_cmd,
        logger_data.is_exec ? " -e" : "",
        msg, strerror(errno),
        logger_data.sub_cmdline);
    exit(EXIT_FAILURE);
}

char* get_cmdline(int argc, char* const argv[], int opt_index) {
    size_t total_size = 0;
    int i;
    char* cmdline;
    char* dest;

    for (i = opt_index; i < argc; ++i)
        total_size += strlen(argv[i]) + 1;

    if (!(cmdline = (char*)malloc(total_size)))
    {
        perror("get_cmdline");
        abort();
    }

    memset(cmdline, ' ', total_size);
    cmdline[total_size - 1] = '\0';

    dest = cmdline;
    for (i = opt_index; i < argc; ++i){
        memcpy(dest, argv[i], strlen(argv[i]));
        dest += strlen(argv[i]) + 1;
    }
    return cmdline;
}



void print_help(char* chinit_cmd)
{
    fprintf(stderr, "Usage: %s cmd [--exec] [cmd_args]\n\n", chinit_cmd);

    fprintf(
        stderr,
        "Run cmd in new temporary pid namespace.\n"
        "  --exec: do plain exec instead of fork/exec pair (see comments below)\n"
        "\n"
    );

    fprintf(
        stderr,
        "Closing of new namespace (which kills all descendants)\n"
        "is guaranteed after normal exit of cmd\n"
        "or in case of killing any of auxiliary processes\n"
        "including initially launched process.\n"
        "\n"

        "It works only in case of applicability of PR_SET_PDEATHSIG.\n"
        "The most common cases of unacceptable are:\n"
        "  1. running suid binaries\n"
        "  2. run from thread which dies before process finish\n"
        "This cases can be considered as features\n"
        "\n"
    );

    fprintf(
        stderr,
        "Implementation scheme:\n"
        "  root process\n"
        "    unshare(CLONE_NEWPID)\n"
        "    fork\n"
        "    waitpid for child process 1\n"
        "  child process ('init' for new pid namespace):\n"
        "    drop privileges\n"
        "    set prctl(PR_SET_PDEATHSIG, SIGKILL)\n"
        "    execvp or fork+waitpid/execvp pair\n"
    );
}


/* helper for forking child with waiting in parrent process */

void fork_and_wait(int second_fork)
{
    pid_t pid;
    int status = 0;

    if ((pid = fork()) < (pid_t)0)
        log_exit_failure(second_fork ? "second fork failed" : "first fork failed");

    if (pid != (pid_t)0)
    {
        /* parent process waits for the completion of a child */
        if (waitpid(pid, &status, 0) < 0)
            log_exit_failure(second_fork ? "second fork: waitpid faild" : "first fork: waitpid failed");

        if (!WIFEXITED(status))
            exit(128 + WTERMSIG(status));

        exit(WEXITSTATUS(status));
    }

}

int main (int argc, char* const argv[])
{

    uid_t userid = 0;
    gid_t groupid = 0;

    int is_exec = 0;


    struct option long_options[] =
    {
        {"exec", no_argument, 0, 'e'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    while (1)
    {
        int getopt_rc;
        int option_index = 0;
        if ((getopt_rc = getopt_long(argc, argv, "+he", long_options, &option_index)) == -1)
            break;
        switch (getopt_rc)
        {
        case 'e':
            is_exec = 1;
            break;
        case 'h':
            print_help(*argv);
            return EXIT_FAILURE;
        case '?':
            /* getopt_long already printed an error message. */
            return EXIT_FAILURE;
        default:
            fprintf(stderr, "%s: unhandled return value of getopt_long: %d\n", argv[0], getopt_rc);
            return EXIT_FAILURE;
        }
    }

    if (argc == 1)
    {
        print_help(*argv);
        return EXIT_FAILURE;
    }

    /* prepare cmdline for pretty exception printing */
    logger_data.main_cmd = argv[0];
    logger_data.is_exec = is_exec;
    logger_data.sub_cmdline = get_cmdline(argc, argv, optind);


    /* after unshare(CLONE_NEWPID) fork of current process */
    /* well be an analogue of the init for new pid namespace */

    if (unshare(CLONE_NEWPID)<0)
        log_exit_failure("unshare(CLONE_NEWPID) failed");

    /* drop privileges */
    groupid = getgid();
    if (setgid(groupid) != 0)
        log_exit_failure("dropping privileges: setgid failed");

    userid = getuid();
    if (setuid(userid) != 0)
        log_exit_failure("dropping privileges: setuid failed");

    fork_and_wait(0);

    /* child process after fork */

    /* expect SIGKILL in child if parent process fail */

    if (prctl(PR_SET_PDEATHSIG, SIGKILL))
        log_exit_failure("prctl failed");

    /* without --exec do additional fork */
    if (!is_exec)
        fork_and_wait(1);

    /* arguments of sub_command*/
    argv += optind;

    execvp(*argv, argv);

    /* this code should never be reached if execvp successful */
    log_exit_failure("execvp failed");
    return EXIT_FAILURE;
}
