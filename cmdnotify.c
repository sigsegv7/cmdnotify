/*
 * Copyright (c) 2023-2024 Ian Marco Moffett.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "config.h"

#define DEFAULT_BINDIR_PREFIX   "/bin/"
#define NOTIFY_SEND_BINLOC  DEFAULT_BINDIR_PREFIX "notify-send"

#define SUCCESS_SUMMARY "Success"
#define FAILURE_SUMMARY "Error"

static char *create_progpath(const char *progname);

/*
 * Runs the program, returns its status
 * code.
 */
static int
run_prog(const char *progname, char *argv[])
{
    pid_t child;
    int status = 0;
    char *progpath = create_progpath(progname);

    child = fork();
    assert(child >= 0);

    if (child == 0) {
        /* Child side */
        execv(progpath, argv);
        __builtin_unreachable();
    }

    /* Parent side */
    while (waitpid(child, &status, 0) > 0);
    free(progpath);

    return WEXITSTATUS(status);
}

/*
 * Creates full program path.
 *
 * For example, if `progname' is "ls", this
 * function will return "/bin/ls"
 *
 * XXX: Make sure to call free() when done using the
 *      value returned.
 */
static char *
create_progpath(const char *progname)
{
    char *progpath = NULL;
    size_t pathlen = strlen(DEFAULT_BINDIR_PREFIX);

    /* Allocate our path */
    pathlen += strlen(progname);
    progpath = malloc(pathlen + 1);
    assert(progpath != NULL);

    /* Create the full path */
    strcat(progpath, DEFAULT_BINDIR_PREFIX);
    strcat(progpath, progname);
    return progpath;
}

/*
 * Returns true if the program exists,
 * otherwise, false.
 *
 * @progname: Name of program, e.g., "ls".
 */
static bool
prog_exists(const char *progname)
{
    char *path = create_progpath(progname);
    bool exists;

    exists = access(path, F_OK) == 0;
    free(path);
    return exists;
}

/*
 * XXX: Used internally by notify_status()
 */
static void
notify(const char *summary, const char *body)
{
    int child;
    int tmp;

    /*
     * Forking will create a child that will
     * then be overwritten by execl() therefore
     * allowing us to continue this main thread
     * and cleanup
     */
    child = fork();
    if (child == 0) {
        /* Child side */
        execl(NOTIFY_SEND_BINLOC, NOTIFY_SEND_BINLOC,
              "-t", NOTIFY_SEND_TIMEOUT, "-u",
              NOTIFY_SEND_URGENCY, summary, body,
              NULL);

        __builtin_unreachable();
    }
    while (wait(&tmp) > 0);
}

/*
 * Causes notification of program status.
 *
 * @status: Status code.
 * @cmd: Command that was ran.
 */
static void
notify_status(int status, const char *cmd)
{
    const size_t MAX_BODY_BUFSIZE = 256;
    char *summary, *body;

    if (status == 0) {
        summary = SUCCESS_SUMMARY;
    } else {
        summary = FAILURE_SUMMARY;
    }

    /* Length will be plus 3 to account for quote pair and '\0' */
    body = malloc(MAX_BODY_BUFSIZE + strlen(cmd));

    snprintf(body, MAX_BODY_BUFSIZE, "'%s' returned %d", cmd, status);
    notify(summary, body);
    free(body);
}

int
main(int argc, char **argv)
{
    const char space_chr = ' ';
    char **argbuf = NULL;
    size_t argbuf_entries = 1, newsize = 0;
    int status = 0;

    if (argc < 2) {
        fprintf(stderr, "Error: Too few arguments!\n");
        return 1;
    }

    if (geteuid() == 0) {
        fprintf(stderr, "Please do not run as root.\n");
        return 1;
    }

    /*
     * We depend on the notify-send
     * binary, give an error if it
     * isn't there...
     */
    if (access("/bin/notify-send", F_OK) != 0) {
        fprintf(stderr, "Error: notify-send not found!\n");
        return 1;
    }

    /*
     * If the program that we wanna run
     * does not exist, give an error.
     */
    if (!prog_exists(argv[1])) {
        fprintf(stderr, "Failed to execute %s%s\n",
               DEFAULT_BINDIR_PREFIX, argv[1]);
        perror("access");
        return 1;
    }

    argbuf = malloc(sizeof(char *));
    argbuf[0] = argv[1];

    for (int i = 2; i < argc; ++i) {
        ++argbuf_entries;
        newsize = argbuf_entries * (sizeof(char *));
        argbuf = realloc(argbuf, newsize);
        /*
         * argv[] example:
         *
         * {"cmdnotify",    "sleep",    "1"}
         *   ^ Ignore       ^ Program    ^ Program argument
         *
         * The program arguments start at argv[i] and end at argv[n],
         * where `i' equals 2, and `n' is the number of program arguments
         * plus 2. Our argbuf index here is equal to i - 1. For example,
         * if we are at the first command argument in argv (argv[2]) then we
         * will be at the second argument in our argbuf, so argbuf[1].
         * This is sort of hacky and it kinda sucks to be honest...
         */
        argbuf[i - 1] = argv[i];
    }

    /* Denote end of arglist */
    argbuf = realloc(argbuf, sizeof(char *) + (argbuf_entries + 1));
    argbuf[argbuf_entries] = NULL;

    /* Run the command and report the status! */
    status = run_prog(argv[1], argbuf);
    notify_status(status, argv[1]);
    free(argbuf);

    return status;
}
