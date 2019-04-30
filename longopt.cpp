/**
 *
 *
 */
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "longopt.h"


#define INNER_MAX_WIDTH 28

#ifdef _WIN32
#define snprintf _snprintf
#endif


char* optparam = NULL;
int errindex = -1;
static int cursor    = 1; /* always begin at 1 */
static int rememberi = 1; /* to deal with '-abc' option */
static char errbuf[1024]; /* error option string buffer */


void fprint_opt(FILE* fp, const option_t* opts)
{
    assert(fp);
    assert(opts);

    fprintf(fp, "Mandatory arguments to long options are mandatory for short options too.\n");

    char buf[1024];
    int i;
    int len[MAX_ARG_COUNT] = {0};
    int max_width[MAX_ARG_COUNT] = {0};
    int group = 0;

    /* find the max width of the option name of each group */
    for (i=0; opts[i].name!=NULL||opts[i].val!=0; ++i) {
        if (opts[i].val=='-') { /* next group of options */
            ++group;
            continue;
        }

        len[i] = 4;
        if (opts[i].name!=NULL) len[i] += 4+(int)strlen(opts[i].name);
        if (opts[i].arg_name!=NULL) len[i] += 1+(int)strlen(opts[i].arg_name);
        if (opts[i].has_arg==LONGOPT_OPTIONAL) len[i] += 2;
        if (len[i]>max_width[group])
            max_width[group] = len[i];
    }
        
    /* display each options */
    group = 0;
    for (i=0; opts[i].name!=NULL||opts[i].val!=0; ++i) {
        if (opts[i].val=='-') {
            ++group;
            
            /* print a blank line and `desc`(if it not empty) */
            if (opts[i].desc==NULL)
                fprintf(fp, "\n");
            else
                fprintf(fp, "\n%s\n", opts[i].desc);
            continue;
        }

        char flag = 0;
        if (opts[i].val!=0) {
            fprintf(fp, "  -%c", opts[i].val);
            flag = ' ';
        }
        else {
            fprintf(fp, "    ");
        }
        if (opts[i].name!=NULL) {
            fprintf(fp, "%c  --%s", flag?',':' ', opts[i].name);
            flag = '=';
        }
        if (opts[i].arg_name!=NULL) {
            if (opts[i].has_arg==LONGOPT_OPTIONAL) {
                fprintf(fp, "[%c%s]", flag, opts[i].arg_name);
            }
            else
                fprintf(fp, "%c%s", flag, opts[i].arg_name);
        }

        /* print enough whitespaces */
        int m = max_width[group]+2<INNER_MAX_WIDTH ? max_width[group] + 2
                                                   : INNER_MAX_WIDTH;
        m += flag==' ';
        if (len[i]+2>INNER_MAX_WIDTH)
            snprintf(buf, 1024, "\n%%%dc", INNER_MAX_WIDTH+1);
        else
            snprintf(buf, 1024, "%%%dc", m-len[i]);
        fprintf(fp, buf, ' ');

        /* print description */
        fprintf(fp, "%s\n", opts[i].desc);
    }
}

void print_opt(const option_t* opts)
{
    fprint_opt(stdout, opts);
}

/* locate input option char or long option string in struct opts */
static int find_opt(const option_t* opts, char c, char* s, size_t p)
{
    int i;
    for (i=0; opts[i].val!=0||opts[i].name!=NULL; ++i) {
        if ( (c!=0 && c!='-' && opts[i].val==c) ||
             ( s!=NULL && opts[i].name!=NULL &&
               strlen(opts[i].name)==p-2 &&
               strncmp(opts[i].name, s+2, p-2)==0) )
            return i;
    }  
    return -1;
}

/* get option parameter */
static int get_arg(int argc, char* argv[], char* s, int i, size_t p)
{
    if (s[1]=='-') { /* long option */
        if (p<strlen(s)) { /* inline option argument */
            optparam = s+p+1;
        }
        else if (cursor<argc) { /* enough argument to read */
            if (argv[cursor][0]!='-' || strlen(argv[cursor])==1)
                optparam = argv[cursor++];
            else
                return -1; /* no argument followed. */
        }
        else { /* not enough arg */
            return -1;
        } /* end of inner if */
    }
    else { /* short option */
        if (s[i+1]!='\0') { /* inline option argument */
            optparam = s+i+1;
        }
        else if (cursor<argc) { /* enough argument to read */
            if (argv[cursor][0]!='-' || strlen(argv[cursor])==1)
                optparam = argv[cursor++];
            else
                return -1; /* no argument followed. */
        }
        else { /* not enough argument */
            return -1;
        } /* end of inner if */
    }
    
    return 0;
}

int longopt(int argc, char* argv[], const option_t* opts)
{
    if (cursor>=argc) /* no more options */
        return LONGOPT_DONE;

    /* Parse command-line string: */
    char* s = argv[cursor++];
    
    if (s[0]!='-') { /* non-option argument */
        optparam = s;
        return LONGOPT_ARG;
    }

    int index;
    if (s[rememberi]=='\0') rememberi = 1;
    size_t p = 0;
    if (s[1]=='-') { /* long option */
        p = strcspn(s, "=");
        index = find_opt(opts, 0, s, p);
    }
    else { /* short option */
        index = find_opt(opts, s[rememberi], NULL, 0);
    }
    if (index==-1) { /* option not recognized */
        if (s[1]=='-') {
            strncpy(errbuf, s, 1024);
        }
        else {
            errbuf[0] = '-';
            errbuf[1] = s[rememberi];
            errbuf[2] = '\0';
        }
        optparam = errbuf;
        rememberi = 1;
        return LONGOPT_UNKNOWN_OPT;
    }

    /* dealing option parameter */
    switch (opts[index].has_arg) {
    case LONGOPT_REQUIRE: /* require argument */
        if (get_arg(argc, argv, s, rememberi, p)!=0) {
            errindex = index;
            optparam = argv[cursor-1];
            return LONGOPT_NEED_PARAM;
        }
        break;
    case LONGOPT_OPTIONAL: /* optional argument */
        if (get_arg(argc, argv, s, rememberi, p)!=0)
            optparam = NULL;
        break;
    default: /* no argument require */
        if (s[1]!='-') {
            ++rememberi;
            if (s[rememberi]!='\0')
                --cursor;
            else
                rememberi = 1;
        }
    }
    
    return index;
}