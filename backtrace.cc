/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2019, Arthus Leroy <arthus.leroy@epita.fr>
 * On https://github.com/arthus-leroy/Python
 * All rights reserved. */

// WARNING: need to use "sudo chmod +s /usr/bin/gdb" to allow gdb capture here
// Look at Ubuntu Wiki - Kernel hardening

# include "backtrace.hh"

# if defined(__unix) and defined(BACKTRACE)
    # include <errno.h>
    # include <stdio.h>
    # include <stdlib.h>
    # include <string.h>
    # include <sys/wait.h>
    # include <unistd.h>

    # define ERR() throw Error(strerror(errno))

    void backtrace(unsigned skip)
    {
        char pid_buf[30];
        const int pid = getpid();
        sprintf(pid_buf, "--pid=%d", pid);

        char name_buf[512];
        name_buf[readlink("/proc/self/exe", name_buf, 511)] = 0;

        int p[2];
        if (pipe(p) == -1)
            ERR();

        int child_pid = fork();
        if (child_pid == -1)
            ERR();
        else if (child_pid == 0)
        {
            if (dup2(p[1], STDOUT_FILENO) == -1)    // redirect stdout to in
                ERR();
            if (dup2(p[1], STDERR_FILENO) == -1)    // redirect stderr to in
                ERR();

            close(p[1]);                            // close now useless in
            close(p[0]);                            // and close useless out

            fprintf(stdout, "stack trace for %s pid=%d\n", name_buf, pid);
            execlp("gdb", "gdb", "--batch", "-n", "-ex", "thread", "-ex", "bt", name_buf, pid_buf, NULL);

            ERR();  // exec should have replaced the program, but failed
        }
        else
        {
            char c;

            waitpid(child_pid, NULL, 0);        // wait for out

            close(p[1]);                        // close in

            // skip "waitpid" and "backtrace" in output
            skip += 2;
            unsigned skip_space = 0;
            bool line = false;

            int err;
            while ((err = read(p[0], &c, 1)) > 0)     // and read in
            {
                // begin delimiter
                if (c == '#' && line == false)
                {
                    if (skip == 0)
                        fprintf(stderr, "    ");

                    skip_space = 0;
                    line = true;
                }

                // between '#' and '\n' after skipping X spaces and Y lines
                if (line && skip == 0 && skip_space > 2)
                    fputc(c, stderr);

                // number of spaces skipped
                if (line && c == ' ')
                    skip_space++;

                // end delimiter
                if (line && c == '\n')
                {
                    if (skip)
                        skip--;
                    line = false;
                }
            }
            if (err == -1)
                ERR();

            close(p[1]);
        }

        throw Error("");
    }
# else
    void backtrace(unsigned)
    {
        throw Error("");
    }

# endif