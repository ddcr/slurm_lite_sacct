#include <string.h>
#include <stdio.h>
#include <getopt.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

#include <grp.h>
#include <pwd.h>
#include "compat_pwdgrp.h"

#include <unistd.h>
#include "src/common/getopt.h"
#include "src/common/xmalloc.h"
#include "src/common/xstring.h"
#include "src/common/list.h"
// #include <slurm/slurm.h>


char *opt_string = ":h";

/* prototypes */
static void  _help_msg(void);
void _usage(void);
void do_help(int);
static char *_convert_to_id(char*, bool);
static char *_convert_to_name(int, bool);

static int _addto_id_char_list(List, char*, bool);
static int _addto_name_char_list(List, char*, bool);
void destroy_char(void *);


static void _help_msg(void)
{
        printf("\
test_getuid [<OPTION>]                                                      \n\
    Valid <OPTION> values are:                                              \n\
     -G, --group:                                                           \n\
                   Use this comma separated list of group names to display  \n\
                   info.                                                    \n\
     -g, --gid:                                                             \n\
                   Use this comma separated list of gids to display info.   \n\
     -U, --user:                                                            \n\
                   Use this comma seperated list of user names to display   \n\
                   info.                                                    \n\
     -u, --uid:                                                             \n\
                   Use this comma seperated list of uids to display info.   \n\
     -h, --help:   Print this description of use.                           \n\
     -H --usage:   Display brief usage message.                             \n\
\n");
}

void _usage(void) {
	printf("Usage: test_getuid [options]\n\tUse --help for help\n");
}

void do_help(int opt_help)
{
	switch (opt_help) {
		case 1:
			_help_msg();
			break;
		case 2:
			_usage();
			break;
			default:
		debug("test_getuid bug: opt_help=%d\n", opt_help);
	}
}

void destroy_char(void *object){
	char*tmp = (char *)object;
	xfree(tmp);
}

static char *_convert_to_name(int id, bool gid)
{
	char *name = NULL;
	if(gid) {
		struct group *grp;
		// if (!(grp=getgrgid(id))) {
		if (!(grp=compat_getgrgid(id))) {
			fprintf(stderr, "Invalid group id: %s\n", name);
			exit(1);
		}
		name = xstrdup(grp->gr_name);
	} else {
		struct passwd *pwd;
		// if (!(pwd=getpwuid(id))) {
		if (!(pwd=compat_getpwuid(id))) {
			fprintf(stderr, "Invalid user id: %s\n", name);
			exit(1);
		}
		name = xstrdup(pwd->pw_name);
	}
	return name;
}

static char *_convert_to_id(char *name, bool gid)
{
	if(gid) {
		struct group *grp;
		// if (!(grp=getgrnam(name))) {
		if (!(grp=compat_getgrnam(name))) {
        	fprintf(stderr, "Invalid group id: %s\n", name);
        	exit(1);
		}
		xfree(name);
		name = xstrdup_printf("%d", grp->gr_gid);
	} else {
		struct passwd *pwd;
		// if (!(pwd=getpwnam(name))) {
		if (!(pwd=compat_getpwnam(name))) {
        	fprintf(stderr, "Invalid user id: %s\n", name);
        	exit(1);
		}
		xfree(name);
		name = xstrdup_printf("%d", pwd->pw_uid);
	}
	return name;
}

static int _addto_id_char_list(List char_list, char *names, bool gid)
{
	int i=0, start=0;
	char *name = NULL, *tmp_char = NULL;
	ListIterator itr = NULL;
	char quote_c = '\0';
	int quote = 0;
	int count = 0;

	if(!char_list) {
		error("No list was given to fill in");
		return 0;
	}

	itr = list_iterator_create(char_list);
	if(names) {
		if (names[i] == '\"' || names[i] == '\'') {
			quote_c = names[i];
			quote = 1;
			i++;
		}
		start = i;
		while(names[i]) {
			// info("got %d - %d = %d", i, start, i-start);
			if(quote && names[i] == quote_c)
				break;
			else if (names[i] == '\"' || names[i] == '\'')
				names[i] = '`';
			else if(names[i] == ',') {
				if((i-start) > 0) {
					name = xmalloc((i-start+1));
					memcpy(name, names+start, (i-start));
					//info("got %s %d", name, i-start);
					if (!isdigit((int) *name)) {
						name = _convert_to_id(name, gid);
					}

					while((tmp_char = list_next(itr))) {
						if(!strcasecmp(tmp_char, name))
							break;
					}

					if(!tmp_char) {
						list_append(char_list, name);
						count++;
					} else
						xfree(name);
					list_iterator_reset(itr);
				}
				i++;
				start = i;
				if(!names[i]) {
					info("There is a problem with "
					     "your request.  It appears you "
					     "have spaces inside your list.");
					break;
				}
			}
			i++;
		}
		if((i-start) > 0) {
			name = xmalloc((i-start)+1);
			memcpy(name, names+start, (i-start));
			if (!isdigit((int) *name)) {
				name = _convert_to_id(name, gid);
			}

			while((tmp_char = list_next(itr))) {
				if(!strcasecmp(tmp_char, name))
					break;
			}

			if(!tmp_char) {
				list_append(char_list, name);
				count++;
			} else
				xfree(name);
		}
	}
	list_iterator_destroy(itr);
	return count;
}

static int _addto_name_char_list(List char_list, char *names, bool gid)
{
	int i=0, start=0;
	char *name = NULL, *tmp_char = NULL;
	ListIterator itr = NULL;
	char quote_c = '\0';
	int quote = 0;
	int count = 0;

	if(!char_list) {
		error("No list was given to fill in");
		return 0;
	}

	itr = list_iterator_create(char_list);
	if(names) {
		if (names[i] == '\"' || names[i] == '\'') {
			quote_c = names[i];
			quote = 1;
			i++;
		}
		start = i;
		while(names[i]) {
			//info("got %d - %d = %d", i, start, i-start);
			if(quote && names[i] == quote_c)
				break;
			else if (names[i] == '\"' || names[i] == '\'')
				names[i] = '`';
			else if(names[i] == ',') {
				if((i-start) > 0) {
					name = xmalloc((i-start+1));
					memcpy(name, names+start, (i-start));
					//info("got %s %d", name, i-start);
					if (isdigit((int) *name)) {
						int id = atoi(name);
						xfree(name);
						name = _convert_to_name(id, gid);
					}

					while((tmp_char = list_next(itr))) {
						if(!strcasecmp(tmp_char, name))
							break;
					}

					if(!tmp_char) {
						list_append(char_list, name);
						count++;
					} else
						xfree(name);
					list_iterator_reset(itr);
				}
				i++;
				start = i;
				if(!names[i]) {
					info("There is a problem with "
					     "your request.  It appears you "
					     "have spaces inside your list.");
					break;
				}
			}
			i++;
		}
		if((i-start) > 0) {
			name = xmalloc((i-start)+1);
			memcpy(name, names+start, (i-start));

			if (isdigit((int) *name)) {
				int id = atoi(name);
				xfree(name);
				name = _convert_to_name(id, gid);
			}

			while((tmp_char = list_next(itr))) {
				if(!strcasecmp(tmp_char, name))
					break;
			}

			if(!tmp_char) {
				list_append(char_list, name);
				count++;
			} else
				xfree(name);
		}
	}
	list_iterator_destroy(itr);
	return count;
}



int main(int argc, char **argv)
{
	int c, optionIndex = 0;
	int verbosity;
	int opt_uid;
	static int opt_help = 0;
	List groupid_list = NULL;
	List userid_list = NULL;
	ListIterator itr=NULL;
	char *start = NULL;

    static struct option long_options[] = {
	            {"gid", 1, 0, 'g'},
    	        {"group", 1, 0, 'G'},
            	{"uid", 1, 0, 'u'},
            	{"user", 1, 0, 'U'},
            	{"usage", 0, &opt_help, 2},
        	    {"help", 0, 0, 'h'},
            	{0, 0, 0, 0}
    };

    opt_uid = getuid();
    verbosity = 0;
    opterr = 1;

    while (1) {
    	c = getopt_long(argc, argv,
    					"U:u:G:g:h",
    					long_options, &optionIndex);
    	if (c == -1)
    		break;
    	switch(c) {
    		case 'G':
    			if(!groupid_list)
    				groupid_list = list_create(destroy_char);
    			_addto_id_char_list(groupid_list, optarg, 1);
    			break;
    		case 'g':
    			if(!groupid_list)
    				groupid_list = list_create(destroy_char);
    			_addto_name_char_list(groupid_list, optarg, 1);
    			break;
    		case 'U':
    			if(!userid_list)
    				userid_list = list_create(destroy_char);
    			_addto_id_char_list(userid_list, optarg, 0);
    			break;
    		case 'u':
    			if(!userid_list)
    				userid_list = list_create(destroy_char);
    			_addto_name_char_list(userid_list, optarg, 0);
    			break;
    		case 'h':
    			opt_help = 1;
    			break;
    		case 'H':
    			opt_help = 2;
    			break;
    	}
    }

    // info("Options selected:\n"
    // 	"\topt_help=%d\n\n",
    // 	opt_help);

    if (opt_help)
    	do_help(opt_help);

	if (groupid_list && list_count(groupid_list)) {
		// info("Groupids requested:\n");
		itr = list_iterator_create(groupid_list);
		while((start = list_next(itr)))
			// info("\t: %s\n", start);
			printf("%s\n", start);
		list_iterator_destroy(itr);
	}

    if (userid_list && list_count(userid_list)) {
    	// info("Userids requested:");
    	itr = list_iterator_create(userid_list);
    	while((start = list_next(itr)))
    		// info("\t: %s\n", start);
    		printf("%s\n", start);
    	list_iterator_destroy(itr);
    }

	return 0;
}