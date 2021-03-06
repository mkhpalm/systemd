/* SPDX-License-Identifier: LGPL-2.1+ */

#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdlib.h>

#include "string-table.h"
#include "util.h"
#include "virt.h"

static bool arg_quiet = false;
static enum {
        ANY_VIRTUALIZATION,
        ONLY_VM,
        ONLY_CONTAINER,
        ONLY_CHROOT,
        ONLY_PRIVATE_USERS,
} arg_mode = ANY_VIRTUALIZATION;

static void help(void) {
        printf("%s [OPTIONS...]\n\n"
               "Detect execution in a virtualized environment.\n\n"
               "  -h --help             Show this help\n"
               "     --version          Show package version\n"
               "  -c --container        Only detect whether we are run in a container\n"
               "  -v --vm               Only detect whether we are run in a VM\n"
               "  -r --chroot           Detect whether we are run in a chroot() environment\n"
               "     --private-users    Only detect whether we are running in a user namespace\n"
               "  -q --quiet            Don't output anything, just set return value\n"
               "     --list             List all known and detectable types of virtualization\n"
               , program_invocation_short_name);
}

static int parse_argv(int argc, char *argv[]) {

        enum {
                ARG_VERSION = 0x100,
                ARG_PRIVATE_USERS,
                ARG_LIST,
        };

        static const struct option options[] = {
                { "help",          no_argument, NULL, 'h'               },
                { "version",       no_argument, NULL, ARG_VERSION       },
                { "container",     no_argument, NULL, 'c'               },
                { "vm",            no_argument, NULL, 'v'               },
                { "chroot",        no_argument, NULL, 'r'               },
                { "private-users", no_argument, NULL, ARG_PRIVATE_USERS },
                { "quiet",         no_argument, NULL, 'q'               },
                { "list",          no_argument, NULL, ARG_LIST          },
                {}
        };

        int c;

        assert(argc >= 0);
        assert(argv);

        while ((c = getopt_long(argc, argv, "hqcvr", options, NULL)) >= 0)

                switch (c) {

                case 'h':
                        help();
                        return 0;

                case ARG_VERSION:
                        return version();

                case 'q':
                        arg_quiet = true;
                        break;

                case 'c':
                        arg_mode = ONLY_CONTAINER;
                        break;

                case ARG_PRIVATE_USERS:
                        arg_mode = ONLY_PRIVATE_USERS;
                        break;

                case 'v':
                        arg_mode = ONLY_VM;
                        break;

                case 'r':
                        arg_mode = ONLY_CHROOT;
                        break;

                case ARG_LIST:
                        DUMP_STRING_TABLE(virtualization, int, _VIRTUALIZATION_MAX);
                        return 0;

                case '?':
                        return -EINVAL;

                default:
                        assert_not_reached("Unhandled option");
                }

        if (optind < argc) {
                log_error("%s takes no arguments.", program_invocation_short_name);
                return -EINVAL;
        }

        return 1;
}

int main(int argc, char *argv[]) {
        int r;

        /* This is mostly intended to be used for scripts which want
         * to detect whether we are being run in a virtualized
         * environment or not */

        log_parse_environment();
        log_open();

        r = parse_argv(argc, argv);
        if (r <= 0)
                return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;

        switch (arg_mode) {

        case ONLY_VM:
                r = detect_vm();
                if (r < 0) {
                        log_error_errno(r, "Failed to check for VM: %m");
                        return EXIT_FAILURE;
                }

                break;

        case ONLY_CONTAINER:
                r = detect_container();
                if (r < 0) {
                        log_error_errno(r, "Failed to check for container: %m");
                        return EXIT_FAILURE;
                }

                break;

        case ONLY_CHROOT:
                r = running_in_chroot();
                if (r < 0) {
                        log_error_errno(r, "Failed to check for chroot() environment: %m");
                        return EXIT_FAILURE;
                }

                return r ? EXIT_SUCCESS : EXIT_FAILURE;

        case ONLY_PRIVATE_USERS:
                r = running_in_userns();
                if (r < 0) {
                        log_error_errno(r, "Failed to check for user namespace: %m");
                        return EXIT_FAILURE;
                }

                return r ? EXIT_SUCCESS : EXIT_FAILURE;

        case ANY_VIRTUALIZATION:
        default:
                r = detect_virtualization();
                if (r < 0) {
                        log_error_errno(r, "Failed to check for virtualization: %m");
                        return EXIT_FAILURE;
                }

                break;
        }

        if (!arg_quiet)
                puts(virtualization_to_string(r));

        return r != VIRTUALIZATION_NONE ? EXIT_SUCCESS : EXIT_FAILURE;
}
