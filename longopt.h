/**
 * @file longopt.h
 *
 * @brief longopt is a set of C(C99) functions to handling program options.
 *
 * It handles both short options(such as -h) and long options(such as --help)
 * as normal GNU/Linux programs do.
 *
 * @b Licence: The MIT License.
 *             http://www.opensource.org/licenses/mit-license.php
 *
 * @author wuxi
 * @date 2008
 *
 * @version 0.1 wuxi
 * @version 0.1.1 wuxi, optional argument bug fixed.
 */

#ifndef _WX_LONGOPT_H_
#define _WX_LONGOPT_H_

#include <stdio.h>

/** max number of arguments */
#define MAX_ARG_COUNT 256


/**
 * option parameter.
 */
extern char* optparam;
/**
 * index of options which error occurs.
 */
extern int errindex;

enum {
    LONGOPT_NOPARAM = 0,
    LONGOPT_REQUIRE = 1,
    LONGOPT_OPTIONAL = 2
};

/**
 * @brief long option struct.
 */
typedef struct longoption {
    int         val;      /**< equivalent short option char */
    const char* name;     /**< same as name in 'struct option' */
    int         has_arg;  /**< same as name in 'struct option' */
    const char* arg_name; /**< option arg name */
    const char* desc;     /**< option description */
} option_t;

/**
 * fprint_opt: print options to fp;
 *
 * @param FILE* fp, output stream pointer.
 * @param const option_t* opts, a pointer to the first element of an array of
 *                              option_t declared in <longopt.h>.
 *
 * @return void.
 */
void fprint_opt(FILE* fp, const option_t* opts);

/**
 * print_opt: print options to stdout.
 *
 * @see fprint_opt() for more information.
 */
void print_opt(const option_t* opts);

enum {
    LONGOPT_DONE = -1,
    LONGOPT_ARG = -2,
    LONGOPT_NEED_PARAM = -3,
    LONGOPT_UNKNOWN_OPT = -4
};

/**
 * get a option.
 *
 * @param int argc, argument count, see argc in main().
 * @param char* argv, see argv in main().
 * @param const option_t* opts, a pointer to the first element of an array of
 *                              option_t declared in <longopt.h>.
 *
 * @return int\n
 *         Returns a number >=0, if an option was successfully found, and
 *         the number is the index in opts indicates the retrieved option.
 *         And optparam is set to the beginning of parameter char* if this
 *         option requires a parameter, otherwise optparam is set to NULL.
 *         \n\n
 *         Returns LONGOPT_DONE(-1), if all command-line options have been
 *         parsed.
 *         \n\n
 *         Returns LONGOPT_ARG(-2), if a non-option argument is found, and
 *         optparam is set to the beginning of argument string.
 *         \n\n
 *         Returns LONGOPT_NEED_PARAM(-3), if no parameter followed the
 *         option, but it requires one. And errindex is set to the index
 *         in opts indicates the option that error occurs.
 *         \n\n
 *         Returns LONGOPT_UNKNOWN_OPT(-4), if longopt() encounters an
 *         option that was not in opts. And optparam is set to point to
 *         the beginning of a static char* buffer, which you don't need to
 *         use free() to release, contains the unknown option string.
 */
int longopt(int argc, char* argv[], const option_t* opts);


#endif