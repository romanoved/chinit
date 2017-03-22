#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <sched.h>
#include <unistd.h>

#include <getopt.h>

/* simple logging helpers */

static struct {
    const char* main_cmd;
    int is_proc;
    int is_exec;
    const char* sub_cmdline;
} logger_data;

char* get_cmdline(int argc, char* const argv[], int opt_index) {
    size_t total_size = 0;
    int i;
    char* cmdline;
    char* dest;

    for (i = opt_index; i < argc; ++i)
        total_size += strlen(argv[i]) + 1;

    if (!(cmdline = (char*)malloc(total_size))) {
        perror("get_cmdline");
        abort();
    }

    memset(cmdline, ' ', total_size);
    cmdline[total_size - 1] = '\0';

    dest = cmdline;
    for (i = opt_index; i < argc; ++i) {
        memcpy(dest, argv[i], strlen(argv[i]));
        dest += strlen(argv[i]) + 1;
    }
    return cmdline;
}

int log_exit_failure(const char* msg) {
    fprintf(stderr, "%s %s%s: %s (%s); cmd '%s'\n",
            logger_data.main_cmd,
            logger_data.is_proc ? "-p" : "-P",
            logger_data.is_exec ? "e" : "",
            msg, strerror(errno),
            logger_data.sub_cmdline);
    return EXIT_FAILURE;
}

void print_help(char* chinit_cmd) {
    fprintf(
        stderr,
        "Usage: %s [options] [command]\n\n"
        "Execute command in new temporary pid namespace.\n\n"
        "Options:\n"
        "    -h, --help: show this help and exit;\n"
        "    -e, --exec: do plain exec instead of fork/exec pair (see man page for details);\n"
        "    -p, --proc: remout /proc (default for now);\n"
        "    -P, --no-proc: do not remout /proc;\n\n"
        ""
        "See man page for details about --exec/--proc/--no-proc options.\n",
        chinit_cmd);
}

int main(int argc, char* const argv[]) {
    int is_exec = 0;
    int is_proc = 1;

    if (argc == 1) {
        print_help(*argv);
        return EXIT_FAILURE;
    }

    while (1) {
        const struct option long_options[] =
            {
                {"help", no_argument, 0, 'h'},
                {"exec", no_argument, 0, 'e'},
                {"proc", no_argument, 0, 'p'},
                {"no-proc", no_argument, 0, 'P'},
                {0, 0, 0, 0}};

        int getopt_rc;
        int option_index = 0;
        if ((getopt_rc = getopt_long(argc, argv, "+hepP", long_options, &option_index)) == -1)
            break;
        switch (getopt_rc) {
            case 'h':
                print_help(*argv);
                return EXIT_FAILURE;
            case 'e':
                is_exec = 1;
                break;
            case 'p':
                is_proc = 1;
                break;
            case 'P':
                is_proc = 0;
                break;
            case '?':
                /* getopt_long already printed an error message. */
                return EXIT_FAILURE;
            default:
                fprintf(stderr, "%s: unhandled return value of getopt_long: %d\n", argv[0], getopt_rc);
                return EXIT_FAILURE;
        }
    }

    /* prepare cmdline for pretty exception printing */
    logger_data.main_cmd = argv[0];
    logger_data.is_proc = is_proc;
    logger_data.is_exec = is_exec;
    logger_data.sub_cmdline = get_cmdline(argc, argv, optind);

    /* arguments of sub_command*/
    argv += optind;

    /* fork of current process after unshare */
    /* will be an analogue of the init for new pid namespace */

    if (is_proc) {
        if (unshare(CLONE_NEWPID | CLONE_NEWNS) < 0)
            return log_exit_failure("unshare(CLONE_NEWPID|CLONE_NEWNS) failed");
    } else {
        if (unshare(CLONE_NEWPID) < 0)
            return log_exit_failure("unshare(CLONE_NEWPID) failed");
    }

    {
        pid_t pid;
        int status = 0;

        if ((pid = fork()) < (pid_t)0)
            return log_exit_failure("first fork failed");

        if (pid != (pid_t)0) {
            /* parent process waits for the completion of a child */
            if (waitpid(pid, &status, 0) < 0)
                return log_exit_failure("first fork: waitpid failed");
            return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
        }
    }

    /* chid after fork - init of new pid namespace */

    /* remount proc */

    if (is_proc) {
        if (umount("/proc"))
            return log_exit_failure("failed to unmount old /proc");

        if (mount("proc", "/proc", "proc", 0, NULL))
            return log_exit_failure("failed to mount new /proc");
    }

    /* drop_privileges */
    {
        uid_t userid = 0;
        gid_t groupid = 0;

        groupid = getgid();
        if (setgid(groupid) != 0)
            return log_exit_failure("dropping privileges: setgid failed");

        userid = getuid();
        if (setuid(userid) != 0)
            return log_exit_failure("dropping privileges: setuid failed");
    }

    /* expect SIGKILL in child if parent process fail */

    if (prctl(PR_SET_PDEATHSIG, SIGKILL))
        return log_exit_failure("prctl failed");

    /* without --exec do additional fork */
    if (!is_exec) {
        pid_t pid;
        int status = 0;

        if ((pid = vfork()) < (pid_t)0)
            return log_exit_failure("second vfork failed");

        if (pid != (pid_t)0) {
            if (waitpid(pid, &status, 0) < 0)
                return log_exit_failure("first fork: waitpid failed");
            return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
        }
    }

    execvp(*argv, argv);

    /* this code should never be reached if execvp successful */
    _exit(log_exit_failure("execvp failed"));
}
