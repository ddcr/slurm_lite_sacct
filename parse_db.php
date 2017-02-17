#!/usr/bin/env php
<?php

static $columnNames = array(
    'job_id',
    'job_id_raw',
    'cluster_name',
    'partition_name',
    'account_name',
    'group_name',
    'gid_number',
    'user_name',
    'uid_number',
    'submit_time',
    'eligible_time',
    'start_time',
    'end_time',
    'elapsed',
    'exit_code',
    'state',
    'nnodes',
    'ncpus',
    'req_cpus',
    'req_mem',
    // 'timelimit',
    'node_list',
    'job_name'
    );

$columnCount = count($columnNames);

$timeZone = new DateTimeZone('America/Sao_Paulo');

function parseDateTime($dateTimeStr)
{
    global $timeZone;
    $dateTimeObj = DateTime::createFromFormat(
        'Y-m-d?H:i:s',
        $dateTimeStr,
        $timeZone
    );

    if ($dateTimeObj === false) {
        echo "Failed to parse datetime '$dateTimeStr'" . PHP_EOL;
        return null;
    }

    return (int)$dateTimeObj->format('U');
}

function parseTimeField($time)
{
    if ($time === '') {
        return null;
    }

    if (strcasecmp($time, 'UNLIMITED') === 0) {
        return null;
    }

    $pattern = '
        /
        ^
        (?:
            (?:
                (?<days> \d+ )
                -
            )?
            (?<hours> \d+ )
            :
        )?
        (?<minutes> \d+ )
        :
        (?<seconds> \d+ )
        (?: \. \d+ )?
        $
        /x
    ';

    // Instead of adding a special case for every non-time formatted
    // time field value, return null if the value doesn't match the
    // expected pattern.  Time fields may be stored as strings in
    // future versions of Open XDMoD and then parsed later in the
    // ETL process.
    if (!preg_match($pattern, $time, $matches)) {
        return null;
    }

    $days = $hours = 0;

    if (!empty($matches['days'])) {
        $days = $matches['days'];
    }

    if (!empty($matches['hours'])) {
        $hours = $matches['hours'];
    }

    $minutes = $matches['minutes'];
    $seconds = $matches['seconds'];

    return $days * 24 * 60 * 60
        + $hours * 60 * 60
        + $minutes * 60
        + $seconds;
}

function shredLine($line)
{
	global $columnNames;
	global $columnCount;

    // echo "Shredding line '$line'" . PHP_EOL;

    $fields = explode('|', $line, $columnCount);

    if (count($fields) != $columnCount) {
        throw new Exception("Malformed Slurm sacct line: '$line'");
    }

    $job = array();

    // Map numeric $fields array into a associative array.
    foreach ($columnNames as $index => $name) {
        $job[$name] = $fields[$index];
    }

    // Skip job steps.

    // Skip jobs that haven't ended.

    // Skip jobs that have no nodes assigned.

    // echo 'Parsed data: ' . json_encode($job) . PHP_EOL;

    $date = substr($job['end_time'], 0, 10);

    // Convert datetime strings into unix timestamps.
    $dateKeys = array(
        'submit_time',
        'eligible_time',
        'start_time',
        'end_time',
    );

    foreach ($dateKeys as $key) {
        $job[$key] = parseDateTime($job[$key]);
    }

    // Convert slurm time fields into number of seconds.
    $timeKeys = array(
        'elapsed',
        // 'timelimit',
    );

    foreach ($timeKeys as $key) {
        $job[$key] = parseTimeField($job[$key]);
    }

    // checkJobData($line, $job);

    // Check for job arrays.
    return $job;
}


function shredFile($file)
{
    echo "Shredding file '$file'" . PHP_EOL;

    if (!is_file($file)) {
        echo "'$file' is not a file" . PHP_EOL;
        return false;
    }

    $fh = fopen($file, 'r');

    if ($fh === false) {
        throw new Exception("Failed to open file '$file'");
    }

    $fh_csv = fopen('php://output', 'w');
    $requiredkeys = array('job_id', 'submit_time',
                          'eligible_time', 'start_time',
                          'end_time');

    $recordCount = 0;
    $duplicateCount = 0;

    try {
        $lineNumber = 0;

        while (($line = fgets($fh)) !== false) {
            $lineNumber++;
            $recordCount++;

            // Remove trailing whitespace.
            $line = rtrim($line);

            // Remove control characters.
            $line = preg_replace('/[\x00-\x1F\x7F]/', '', $line);

            try {
                $job = shredLine($line);
                fputcsv($fh_csv, array($job['job_id'], 
                                       $job['submit_time'],
                                       $job['eligible_time'],
                                       $job['start_time'],
                                       $job['end_time']), '|');
            } catch (PDOException $e) {

                // Ignore duplicate key errors.
                if ($e->getCode() == 23000) {
                    $msg = 'Skipping duplicate data: ' . $e->getMessage();
                    echo 'message ' . $msg;
                    echo 'file' . $file;
                    echo 'line_number ' . $lineNumber;
                    echo 'line ' . $line;
                    $duplicateCount++;
                    continue;
                } else {
                    throw $e;
                }
            }
        }

        echo 'Committing database transaction' . PHP_EOL;
    } catch (Exception $e) {
        // Close file handle, but don't throw an exception if it
        // fails since an exception will be thrown below.
        fclose($fh);

        $msg = sprintf(
            'Failed to shred line %d of file %s "%s": %s',
            $lineNumber,
            $file,
            $line,
            $e->getMessage()
        );

        throw new Exception($msg, 0, $e);
    }

    if (fclose($fh) === false) {
        throw new Exception("Failed to close file '$file'");
    }

    echo "Shredded $recordCount records" . PHP_EOL;

    if ($duplicateCount > 0) {
        $msg = "Skipped $duplicateCount duplicate records";
        echo $msg;
    }

    return $recordCount;
}

// Instantiate a DateTime with microseconds.
// $d = new DateTime('2011-01-01T15:03:01.012345Z');

// Output the microseconds.
// echo $d->format('u'); // 012345

// Output the date with microseconds.
// echo $d->format('Y-m-d\TH:i:s.u') . PHP_EOL; // 2011-01-01T15:03:01.012345

shredFile('slurm-2013-05-01.log');
// echo $d=parseDateTime('2013-05-31T14:31:15') . PHP_EOL;
