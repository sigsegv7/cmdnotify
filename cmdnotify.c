/*
 * Copyright (c) 2023 Emilia Strange.
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
 * 3. Neither the name of VegaOS nor the names of its
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
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#define DEFAULT_BINDIR_PREFIX   "/bin/"
#define NOTIFY_SEND_BINLOC  DEFAULT_BINDIR_PREFIX "notify-send"
#define NOTIFY_SEND_TIMEOUT "3500"

#define SUCCESS_SUMMARY "Success"
#define FAILURE_SUMMARY "Error"

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
     *
     * XXX: It is probably a good idea to wait for the
     *      child to die before we clean up, however,
     *      there is a chance this may not be needed.
     */
    child = fork();
    if (child == 0) {
        execl(NOTIFY_SEND_BINLOC, NOTIFY_SEND_BINLOC,
              "-t", NOTIFY_SEND_TIMEOUT, summary,
              body, NULL);

        __builtin_unreachable();
    }
    while (wait(&tmp) > 0);
}

/*
 * Does the same thing as strcat()
 * but will an extra argument that specifies
 * the length of the src so it does not need
 * to be computed twice...
 *
 * XXX: Does *not* add '\0' to the end.
 *      It is up to the *caller* to do this.
 */
static void
append(char **dest_ptr, const char *src,
       size_t src_len)
{
    char *dest = *dest_ptr;

    for (size_t i = 0; i < src_len; ++i) {
        dest[i] = src[i];
    }

    *dest_ptr += src_len;
}

static void
notify_status(int status, const char *cmd)
{
    const char *const body_end_success = "has finished and returned 0\n";
    const char *const body_end_fail = "has returned non-zero value\n";
    const char *body_end_ptr = NULL;
    const char *summary_ptr = NULL;

    size_t body_len, body_end_len;
    char *body = NULL;

    if (status == 0) {
        body_end_ptr = body_end_success;
        summary_ptr = SUCCESS_SUMMARY;
    } else {
        body_end_ptr = body_end_fail;
        summary_ptr = FAILURE_SUMMARY;
    }

    body_len = strlen(cmd) + 2;                     /* Add 2 for quote pair */
    body_len += strlen(body_end_ptr) + 1;           /* Add 1 for '\0' */

    body = calloc(body_len + body_end_len, sizeof(char));
    assert(body != NULL);

    snprintf(body, body_len, "'%s' %s\n", cmd, body_end_ptr);
    notify(summary_ptr, body);

    free(body);
}

int
main(int argc, const char **argv)
{
    const char space_chr = ' ';
    char *cmdbuf = NULL, *cmdbuf_ptr = NULL;
    size_t cmdbuf_len;
    int status;

    if (argc < 2) {
        fprintf(stderr, "Error: Too few arguments!\n");
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

    cmdbuf_len = 0;

    /* Compute the size of the arg buffer */
    for (size_t i = 1; i < argc; ++i) {
        /*
         * We want arguments to be seperated
         * by a space e.g arg1 arg2 arg3
         * thus adding 1 to the length
         */
        cmdbuf_len += strlen(argv[i]) + 1;
    }

    cmdbuf = malloc(cmdbuf_len + 1);    /* Add 1 for '\0' */
    assert(cmdbuf != NULL);

    /* Append arguments to the command buffer */
    cmdbuf_ptr = cmdbuf;
    for (size_t i = 1; i < argc; ++i) {
        append(&cmdbuf_ptr, argv[i], strlen(argv[i]));
        append(&cmdbuf_ptr, &space_chr, 1);
    }

    /* Null terminate command buffer */
    *cmdbuf_ptr = '\0';

    /* Run the command and report the status! */
    status = system(cmdbuf);
    notify_status(status, argv[1]);
    free(cmdbuf);

    return status;
}
