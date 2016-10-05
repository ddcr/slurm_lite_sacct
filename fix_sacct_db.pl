#!/usr/bin/perl
#
# Hi Paul,

# Paul Edmon <ped...@cfa.harvard.edu> writes:

# > If you could send me the script that would be appreciated.  Thanks.
# >
# > -Paul Edmon-

# So first off, I'm providing this code as used successfully by me, but,
# to quote the GPL:

#     This program is distributed in the hope that it will be useful,
#     but WITHOUT ANY WARRANTY; without even the implied warranty of
#     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

# So if it deletes all your data, sets fire to your computer, or makes
# your house fall down, it's not my fault.

# You are going to have to make the following changes:

# 1. The relevant database tables all have the name of the cluster as part
#    of the table name.  So, since our cluster is called 'soroban', the
#    job information is stored in the table 'soroban_job_table'.  You
#    probably need to do

#    use slurm_acct_db;
#    show tables;

#    in MySQL to see what the correct names are.

# 2. The main program consists of a loop over all the jobs selected by the
#    SELECT statement right at the beginning of the program.  Then there
#    is a loop over all the corresponding steps. You then have to decide
#    which cases you want to deal with and how to deal with them.  This is
#    all contained in the horrible mess of 'if's and 'else's.

# 3. There is an argument to the program, maxjobid, used in the main
#    SELECT.  This is because I had to distinguish between jobs which
#    really were running and those which were not, but had no end_time.
#    So I only considered ID lower than the lowest I knew really was
#    running.  Only jobs with lower IDs are considered for modification.

# The program generates a file to make the changes, 'update.sql', and a
# file to undo the changes, 'update_undo.sql.  You can apply them with the
# following

# mysql -u root -p slurm_acct_db < /path/to/update.sql

# Note that if you run the program, apply the changes, then run the
# program again, you will be prompted whether you want to overwrite the
# existing files.  However, since you are going to be trying this out on a
# test system using a dump of the production database, it is not going to
# matter, right?

# If you have any problems, let me know.

# Cheers,

# Loris

use strict;
use warnings;

use Carp;
use Getopt::Long;
use Pod::Usage;

use DBI;
use DBD::mysql;
use Term::ReadKey;
use List::Util qw /max min/;

###############################################################################
# Configuration
###############################################################################

my $platform = 'mysql';
my $database = 'slurm_acct_db';
my $host     = 'localhost';
my $port     = '3306';
my $user     = 'root';
my $dsn      = "dbi:$platform:$database:$host:$port";
my %states_by_code =
  (0 => 'PENDING',  1 => 'RUNNING',
   2 => 'SUSPENDED',3 => 'COMPLETED',
   4 => 'CANCELLED',5 => 'FAILED',
   6 => 'TIMEOUT',  7 => 'NODE_FAIL',
   8 => 'PREEMPTED');
my %states_by_string = reverse %states_by_code;

my $output_dir = './';
my $update_file = $output_dir . 'update.sql';
my $undo_file   = $output_dir . 'update_undo.sql';

my @jobs_without_end_times;
my @jobs_without_end_times_fixed;
my @sql_new_values;
my @sql_old_values;

###############################################################################
# Options and Arguments
###############################################################################

my $help;
#my $max_jobid = 123400;
my $max_jobid = 148213;
my $std_time = 1; # Thu Jan  1 01:00:01 1970

GetOptions('help|h'       => \$help,
           'maxjobid|m=i' => \$max_jobid,
          );
if ($help) {
  pod2usage(-verbose => 2);
  exit;
}

###############################################################################
# Main Program
###############################################################################

print "Enter " . $user. "'s $platform password: ";
ReadMode 'noecho';
my $password = ReadLine 0;
chomp $password;
ReadMode 'normal';
print "\n";

my $dbh = DBI->connect($dsn, $user, $password,
                       {
                        'RaiseError' => 1});
my $select_jobs_stmt =
  'select job_db_inx,id_job,' .
  'time_start,time_end,state' .
  ' from soroban_job_table' .
  ' where id_job<' . $max_jobid .
  ' and time_end=0';
my $job_href =
  $dbh->selectall_hashref($select_jobs_stmt,'id_job');

# Loop over jobs
# --------------
my @job_ids = sort {$a <=> $b} keys(%{$job_href});
foreach my $job_id (@job_ids) {
  printf '%12s%12s%12s%12s%12s','job_db_inx','Job/StepID','Start','End','State';
  print "\n------------  ----------  ----------  ----------  ----------\n";
  my @nonzero_step_start_times;
  my @nonzero_step_end_times;
  my $job = $job_href->{$job_id};
  printf '%12d%12d%12d%12d%12s',$job->{job_db_inx},$job_id,
    $job->{time_start},$job->{time_end},$states_by_code{$job->{'state'}};
  print "\n";

  my $select_steps_stmt =
    'select job_db_inx,id_step,time_start,time_end,state' .
      ' from soroban_step_table' .
        ' where job_db_inx=' . $job->{job_db_inx};
  my $step_href =
    $dbh->selectall_hashref($select_steps_stmt,'job_db_inx');

  # Loop over job steps
  # -------------------
  while (my ($step_db_inx, $step) = each %{$step_href}) {
    my $step_id = $step->{id_step};
    printf '%12d%12d%12d%12d%12s',$step_db_inx,$step_id,
      $step->{time_start},$step->{time_end},
        $states_by_code{$step->{state}};
    print "\n";

    # Collect nonzero step start/end times
    if ( $step->{time_start} > 0 ) {
      push(@nonzero_step_start_times,$step->{time_start});
    }
    if ( $step->{time_end} > 0 ) {
      push(@nonzero_step_end_times,$step->{time_end});
    }

    # Solutions to inconsistencies
    # ----------------------------
    # 1. Jobs/step with RUNNING/PENDING should be set to FAILED
    # 2. Jobs with a valid step_end_time use that
    # 3. Jobs w/o  a valid step_end_time use job_start_time

    # Check step start/end times
    if ($step->{time_start} == 0) {
      print "WARNING: Step start time = $step->{time_start}!\n" .
        "NO SOLUTION DECIDED YET!\n";
      exit;
    }
    if ($step->{time_end} == 0) {
      print "Change step end time to step start time:" .
        " $step->{time_end} ->  $step->{time_start}\n";
      update_step(\@sql_old_values,$job->{job_db_inx},$step_id,
                  'time_end',$step->{time_end});
      update_step(\@sql_new_values,$job->{job_db_inx},$step_id,
                  'time_end',$step->{time_start});

    }
    if ($step->{time_start} == 0 and $step->{time_end} == 0) {
      print "Both step start/end times = 0!\n" .
        "NO SOLUTION DECIDED YET!\n";
      exit;
    }

    # Fix step state
    # --------------
    if ( $step->{state} == $states_by_string{PENDING} or
         $step->{state} == $states_by_string{RUNNING}) {
      if ( $job->{state} == $states_by_string{PENDING} or
           $job->{state} == $states_by_string{RUNNING}) {
        print "Change step state: $states_by_code{$step->{state}}" .
          " -> FAILED\n";
        update_step(\@sql_old_values,$job->{job_db_inx},$step_id,
                    'state',$step->{state});
        update_step(\@sql_new_values,$job->{job_db_inx},$step_id,
                    'state',$states_by_string{FAILED});
      } else {
        print "Change step state to job state:" .
          " $states_by_code{$step->{state}} -> $states_by_code{$job->{state}}\n";
        update_step(\@sql_old_values,$job->{job_db_inx},$step_id,
                    'state',$step->{state});
        update_step(\@sql_new_values,$job->{job_db_inx},$step_id,
                    'state',$job->{state});
      }
    }
  }
# ----------------------
# End of loop over steps

  # Fix job state
  # -------------
  if ( $job->{state} == $states_by_string{PENDING} or
       $job->{state} == $states_by_string{RUNNING}) {
    print "Change job state from $states_by_code{$job->{state}}" .
      " to FAILED\n";
    update_job(\@sql_old_values,$job->{job_db_inx},'state',$job->{state});
    update_job(\@sql_new_values,$job->{job_db_inx},'state',
               $states_by_string{FAILED});
  }

  # Fix job start/end_time
  # ----------------
  if ($job->{time_start} > 0 ) {                 # if start_time is non-0
    if (scalar(@nonzero_step_end_times)<1) {     # & no non-0 step end_times
      if (scalar(@nonzero_step_start_times)<1) { # & no non-0 step start_times
        # Set job_end_time to job_start_time
        print "Change job end time to job start time:" .
          " 0 -> $job->{time_start}\n";
        push(@jobs_without_end_times,$job_id);
        update_job(\@sql_old_values,$job->{job_db_inx},'time_end',0);
        update_job(\@sql_new_values,$job->{job_db_inx},'time_end',
                   $job->{time_start});
      } else {
        # Set job_end_time to max step_start_time
        my $time = max(@nonzero_step_start_times);
        print "Change job end time to max step start time:" .
          " 0 -> $time\n";
        update_job(\@sql_old_values,$job->{job_db_inx},'time_end',0);
        update_job(\@sql_new_values,$job->{job_db_inx},'time_end',$time);
      }
    } else {
      # otherwise use latest step end time for job
      my $time = max(@nonzero_step_end_times);
      print "Change job end time to last step end time:" .
        " 0 -> $time\n";
      update_job(\@sql_old_values,$job->{job_db_inx},'time_end',0);
      update_job(\@sql_new_values,$job->{job_db_inx},'time_end',$time);
    }
  } else { # job start time == 0
    if (scalar(@nonzero_step_start_times)<1) {
      # Use now
      print "Change job start/end time both to $std_time:" .
        " 0 -> " . localtime($std_time) . "\n";
      update_job(\@sql_old_values,$job->{job_db_inx},'time_start',0);
      update_job(\@sql_new_values,$job->{job_db_inx},
                 'time_start',$std_time);
      update_job(\@sql_old_values,
                 $job->{job_db_inx},
                 'time_end',0);
      update_job(\@sql_new_values,
                 $job->{job_db_inx},
                 'time_end',$std_time);
    } else {
      # otherwise use earliest/latest step start time for job
      my $min_start_time = min(@nonzero_step_start_times);
      print "Change job start time to first step start time:" .
        "$job->{time_start} -> $min_start_time\n";
      update_job(\@sql_old_values,
                 $job->{job_db_inx},
                 'time_start',0);
      update_job(\@sql_new_values,
                 $job->{job_db_inx},
                 'time_start',$min_start_time);
      my $max_start_time = max(@nonzero_step_start_times);
      print "Change job end time to last step start time:" .
        " $job->{time_end} -> $max_start_time\n";
      update_job(\@sql_old_values,
                 $job->{job_db_inx},
                 'time_end',0);
      update_job(\@sql_new_values,
                 $job->{job_db_inx},
                 'time_end',$max_start_time);
    }
  }
  print "------------------------------------------------------------\n";
}
# ---------------------
# End of loop over jobs

print <<END

Summary
=======

END
  ;
print "Total number of jobs w/o end time:      " .
  scalar(keys(%{$job_href})) . "\n";
print "Job w/o job and w/o step end time: " .
  scalar(@jobs_without_end_times) . "\n";
# foreach (@jobs_without_end_times) {
#   print "\t$_\n";
# }

write_sql(\@sql_new_values,$update_file);
write_sql(\@sql_old_values,$undo_file);

$dbh->disconnect();
exit;

###############################################################################
# Subroutines
###############################################################################

sub update_job {
  my ($aref,$job_db_inx,$col,$val) = @_;
  my $stmt =
    'update soroban_job_table' .
      ' set ' . $col . '=' . $val .
        ' where job_db_inx=' . $job_db_inx;
  push(@{$aref},$stmt);
}

sub update_step {
  my ($aref,$job_db_inx,$id_step,$col,$val) = @_;
  my $stmt =
    'update soroban_step_table' .
      ' set ' . $col . '=' . $val .
        ' where job_db_inx=' . $job_db_inx .
          ' and id_step=' . $id_step;
  push(@{$aref},$stmt);
}

sub write_sql {
  my ($sql_statements_aref,$file_name) = @_;
  if (-r $file_name) {
    print "$file_name exists. Overwrite? [y/n]: ";
    my $ans = <STDIN>;
    chomp $ans;
    return if $ans ne 'y';
  }
  open(my $output,'>',$file_name)
    or croak "Can't open $file_name: $!";
  foreach my $sql (@$sql_statements_aref) {
    print $output "$sql;\n";
  }
  close($output);
}

__END__

##############################################################################
# POD
##############################################################################

=head1 NAME

fix_sacct_db.pl - fixes jobs/steps w/o end time

=head1 SYNOPSIS

   fix_sacct_db -m

=head1 DESCRIPTION

The program fixes errors in the sacct database, which result in the command
'sacct' showing jobs as running which have in fact come to an end.  All the=
 jobs
with zero end time are selected and a suitable finite end time determined.

=head1 OPTIONS

   -h  --help   this help
   -m  --maxid  max. Job ID considered

=cut

# Local Variables:
# cperl-indent-level: 2
# End:
