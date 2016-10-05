#include "sacct.h"


char *find_hostname(uint32_t pos, char *hosts)
{
	hostlist_t hostlist = NULL;
	char *temp = NULL, *host = NULL;

	if(!hosts || (pos == (uint32_t)NO_VAL))
		return NULL;
	
	hostlist = hostlist_create(hosts);
	temp = hostlist_nth(hostlist, pos);
	if(temp) {
		host = xstrdup(temp);
		free(temp);
	} 
	hostlist_destroy(hostlist);
	return host;
}

void aggregate_sacct(sacct_t *dest, sacct_t *from)
{
	if(dest->max_vsize < from->max_vsize) {
		dest->max_vsize = from->max_vsize;
		dest->max_vsize_id = from->max_vsize_id;
	}
	dest->ave_vsize += from->ave_vsize;
	
	if(dest->max_rss < from->max_rss) {
		dest->max_rss = from->max_rss;
		dest->max_rss_id = from->max_rss_id;
	}
	dest->ave_rss += from->ave_rss;
	
	if(dest->max_pages < from->max_pages) {
		dest->max_pages = from->max_pages;
		dest->max_pages_id = from->max_pages_id;
	}
	dest->ave_pages += from->ave_pages;
	
	if((dest->min_cpu > from->min_cpu) 
	   || (dest->min_cpu == (float)NO_VAL)) {
		dest->min_cpu = from->min_cpu;
		dest->min_cpu_id = from->min_cpu_id;
	}
	dest->ave_cpu += from->ave_cpu;
}
