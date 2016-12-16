/* I am trying to follow the path of the requested memory until it
reaches the database. In the BULL version of slurm this item is
not commited to the database.
*/
#include <string.h>
#include <stdio.h>
#include <getopt.h>
#include <stdint.h>
#include <stdlib.h>
#include "src/common/xstring.h"
#include "src/common/proc_args.h"
#include <slurm/slurm.h>

#define NO_VAL               		(0xfffffffe)
#define MEM_PER_CPU          		0x80000000 /*ddcr: with 32-bit int, this val is 'unsigned int' */
#define LONG_OPT_MEM         		0x107
#define LONG_OPT_MEM_PER_CPU 		0x13a
#define DEFAULT_MEM_PER_CPU         0
#define DEFAULT_MAX_MEM_PER_CPU     0

/*
[src/common/read_config.c]
static void _init_slurm_conf(const char *file_name) =>
s_p_parse_file(conf_hashtbl, name) =>
static void _validate_and_set_defaults(slurm_ctl_conf_t *conf, s_p_hashtbl_t *hashtbl)
*/

/*
GLOBAL VARIABLES RETRIEVED FROM slurm.conf and slurmdbd.conf of cluster
veredas:
		DefMemPerCPU=2000
        DefMemPerNode not defined
        MaxMemPerCPU not defined
        MaxMemPerNode not defined
        MaxMemPerTask not defined

The last three are not defined in `veredas` so they imply
that max_mem_per_task is equal to DEFAULT_MAX_MEM_PER_CPU
*/
static uint32_t def_mem_per_task = 2000;  /* default MB memory per spawned task */
/*
	This option is not selected in veredas cluster
	static uint32_t def_mem_per_task = DEFAULT_MEM_PER_CPU;
*/
static uint32_t max_mem_per_task = DEFAULT_MAX_MEM_PER_CPU; /* maximum MB memory per spawned task */


static struct option long_options[] = {
	{"mem",           required_argument, 0, LONG_OPT_MEM},
	{"mem-per-cpu",   required_argument, 0, LONG_OPT_MEM_PER_CPU},
	{NULL, 0, 0, 0}
};

char *opt_string = ":h";

/* prototypes */
static bool _valid_job_min_mem_slurm_205(uint32_t);
static void  _help(void);

/*  routine cloned from _valid_job_min_mem(...) [job_mgr.c /SLURM 2.0.5-Bull] */
static bool _valid_job_min_mem_slurm_205(uint32_t job_min_memory)
{
	uint32_t base_size = job_min_memory;
	// extern slurmctld_conf [src/common/read_config.h]
	// slurm_ctl_conf_t is defined in <slurm/slurm.h>
	// uint32_t size_limit = slurmctld_conf.max_mem_per_task;
	uint32_t size_limit = max_mem_per_task;
	uint16_t cpus_per_node;

	if (size_limit == 0)
		return true;

	if ((base_size  & MEM_PER_CPU) && (size_limit & MEM_PER_CPU)) {
		base_size  &= (~MEM_PER_CPU);
		size_limit &= (~MEM_PER_CPU);
		if (base_size <= size_limit)
			return true;
		return false;
	}

	if (((base_size  & MEM_PER_CPU) == 0) &&
	    ((size_limit & MEM_PER_CPU) == 0)) {
		if (base_size <= size_limit)
			return true;
		return false;
	}
	/* Our size is per CPU and limit per node or vise-versa.
	 * CPU count my vary by node, but we don't have a good
	 * way to identify specific nodes for the job at this
	 * point, so just pick the first node as a basis for
	 * enforcing MaxMemPerCPU. */

	/*	if (slurmctld_conf.fast_schedule)
			cpus_per_node = node_record_table_ptr[0].config_ptr->cpus;
		else
			cpus_per_node = node_record_table_ptr[0].cpus;
	*/
	cpus_per_node = 8;  // @author ddcr

	// if (job_desc_msg->num_procs != NO_VAL)
	// 	cpus_per_node = MIN(cpus_per_node, job_desc_msg->num_procs);

	// if (base_size & MEM_PER_CPU) {
	// 	base_size &= (~MEM_PER_CPU);
	// 	base_size *= cpus_per_node;
	// } else {
	// 	size_limit &= (~MEM_PER_CPU);
	// 	size_limit *= cpus_per_node;
	// }

	// if (base_size <= size_limit)
	// 	return true;

	return true;
	// return false;
}

static void _help(void)
{
	printf (
"Usage: test_memory_issues.exe [OPTIONS...]\n"
"\n"
"Options:\n"
"      --mem=MB                minimum amount of real memory\n"
"      --mem-per-cpu=MB        maximum amount of real memory per CPU\n"
"                              allocated to the job.\n"
"\n"
"\n"
"Manpage sbatch.1:\n"
"       --mem=<MB>\n"
"              Specify the real memory required per node in MegaBytes.  Default\n"
"              value is DefMemPerNode and the maximum value  is  MaxMemPerNode.\n"
"              If configured, both of parameters can be seen using the scontrol\n"
"              show config command.  This parameter would generally be used  of\n"
"              whole  nodes  are  allocated to jobs (SelectType=select/linear).\n"
"              Also see --mem-per-cpu.  --mem and  --mem-per-cpu  are  mutually\n"
"              exclusive.\n"
"\n"
"       --mem-per-cpu=<MB>\n"
"              Mimimum memory required per allocated CPU in MegaBytes.  Default\n"
"              value is DefMemPerCPU and the maximum value is MaxMemPerCPU.  If\n"
"              configured,  both  of  parameters can be seen using the scontrol\n"
"              show config command.  This parameter would generally be used  of\n"
"              individual   processors   are   allocated   to   jobs   (Select-\n"
"              Type=select/cons_res).  Also see --mem.  --mem and --mem-per-cpu\n"
"              are mutually exclusive.\n"
"\n"
"\n");
}


int main(int argc, char **argv)
{
	uint32_t job_min_memory; /* minimum real memory per node OR
							  * real memory per CPU | MEM_PER_CPU,
							  * default=0 (no limit)
							  * */
	uint32_t num_procs;      /* number of processors required by job */
	long job_min_memory_to_str;
	int option_index = 0;
	int opt_char;
	char *mem_type;

	int mem_per_cpu = -1;        /* --mem-per-cpu=n              */
	int realmem     = -1;        /*  --mem=n                      */

	/* ====== INITIALIZE ====== */
	/*	slurm_init_job_desc_msg */
	/* ======================== */
	job_min_memory    = NO_VAL;
	num_procs         = NO_VAL;

	optind = 0;
	while((opt_char = getopt_long(argc, argv, opt_string,
		long_options, &option_index)) != -1) {
		switch (opt_char) {
			case 'h':
				_help();
				exit(0);
				break;
			case LONG_OPT_MEM:
				realmem = (int) str_to_bytes(optarg);
				if (realmem < 0) {
					error("invalid memory constraint %s",
					      optarg);
					exit(1);
				}
				break;
			case LONG_OPT_MEM_PER_CPU:
				mem_per_cpu = (int) str_to_bytes(optarg);
				if (mem_per_cpu < 0) {
					error("invalid memory constraint %s",
					      optarg);
					exit(1);
				}
				break;
			default:
				printf("Unrecognized command line parameter %c\n", opt_char);
				_help();
				exit(1);
		}
	}

	printf("choice: realmem = %d, mem_per_cpu = %d\n", realmem, mem_per_cpu);
	if ((realmem > -1) && (mem_per_cpu > -1)) {
		if (realmem < mem_per_cpu) {
			info("mem < mem-per-cpu - resizing mem to be equal "
     			 "to mem-per-cpu");
			realmem = mem_per_cpu;
		}
	}

	/* ======================== */
	/*	fill_job_desc_from_opts */
	/* ======================== */
    if (realmem > -1)
        job_min_memory = realmem;  /* Not per CPU */
    else if (mem_per_cpu > -1)
        job_min_memory = mem_per_cpu | MEM_PER_CPU; /* marked as per CPU */


	/* =============================================================================== */
	/* slurmctld_req -> [_slurm_rpc_submit_batch_job | _slurm_rpc_allocate_resources |
	                     _slurm_rpc_job_will_run] -> job_allocate ->
						_job_create -> _validate_job_desc */
	/* =============================================================================== */
    if (job_min_memory == NO_VAL) {
        /* Default memory limit is DefMemPerCPU (if set) or no limit */
        job_min_memory = def_mem_per_task | MEM_PER_CPU; /*from slurm.conf and mark it as per cpu*/
    } else if (!_valid_job_min_mem_slurm_205(job_min_memory)) {
        printf("ESLURM_INVALID_TASK_MEMORY\n");
        return 1;
    }

	/*
		[job_mgr.c]
	    IN job_specs - job specification from RPC
	    void dump_job_desc(job_desc_msg_t * job_specs)
	    extern int job_allocate(job_desc_msg_t * job_specs, int immediate,
	    static int _job_create(job_desc_msg_t * job_desc, int allocate, int will_run,
		int update_job(job_desc_msg_t * job_specs, uid_t uid)
	*/
    if (job_min_memory == NO_VAL) {
            job_min_memory_to_str = -1L;
            mem_type = "job";
    } else if (job_min_memory & MEM_PER_CPU) {
            job_min_memory_to_str = (long) (job_min_memory &
                               		       (~MEM_PER_CPU));
            mem_type = "cpu";
    } else {
            job_min_memory_to_str = (long) job_min_memory;
            mem_type = "job";
    }
    printf("   min_memory_%s=%ld\n", mem_type, job_min_memory_to_str);

	// Check routine _print_job_min_memory()
	// this is simple. Just print job_min_memory & (~MEM_PER_CPU)

	return 0;
}