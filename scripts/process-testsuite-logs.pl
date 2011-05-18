#!/usr/bin/perl

# ----------------------------------------------------------------------
# scripts/process-testsuite-logs.pl
# Process the integration testsuite logs in a system-independent manner
# Author : Chris Williams
# Date   : Tues,18 May 2011.18:14:00
#
# Copyright (c) Chris Williams and individual contributors
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#
# Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/
# ______________________________________________________________________

@tests = ( "Usermode execution" );
my $results;

# syntax: process-testsuite-logs.pl <target name>
#
# this script will process all files in the target's test directory (./test/<target>/automated_*.log)
# and output a big HTML-formatted report to stdout.

# clean the args
$ARGV[0] =~ s/[^A-Za-z0-9_]//g;

if($ARGV[0] eq "")
{
   print STDERR "syntax: process-testsuite-logs.pl <i386_pc | arm_versatilepb>\n";
   exit 1;
}

# find the boot configurations
require "./test/$ARGV[0].pl" or die $!;

# calc these to help format the table 
$cpu_model_count = scalar @arch_cpu_models;
$cores_fitted_count = scalar @arch_cpu_cores;
$mem_fitted_count = scalar @arch_mem_fitted;

# output the start of the HTML index
print "<html><head><title>Testsuite boot results for $arch_name</title></head>\n";
print "<body><h1>Diosix kernel API testsuite results for $arch_name</h1>\n";
print "<table cellspacing=\"1\" cellpadding=\"10\" bgcolor=\"#ddd\">\n";
print " <tr bgcolor=\"white\">\n";
print "  <td colspan=\"2\"><b>Test</b></td>\n";

foreach $model (@arch_cpu_models)
{
   print "  <td align=\"center\" colspan=\"".($cores_fitted_count * $mem_fitted_count)."\"><b>$model</b></td>\n";
   print "  <td width=\"1\">&nbsp;</td>\n"; # vertical separator
}

print " </tr>\n";
print " <tr bgcolor=\"white\">\n";
print "  <td colspan=\"2\">&nbsp;</td>\n";

foreach $model (@arch_cpu_models)
{
   foreach $cores (@arch_cpu_cores)
   {
      print "  <td align=\"center\" colspan=\"".($mem_fitted_count)."\"><b>$cores cpu(s)</b></td>\n";
   }
   
   print "  <td width=\"1\">&nbsp;</td>\n"; # vertical separator
}

print " </tr>\n";
print " <tr bgcolor=\"white\">\n";
print "  <td colspan=\"2\">&nbsp;</td>\n";

foreach $model (@arch_cpu_models)
{
   foreach $cores (@arch_cpu_cores)
   {
      foreach $mem (@arch_mem_fitted)
      {
         print "  <td><b>$mem</b></td>\n";
         
         # this is where we read in the test results...
         &process_log($model, $cores, $mem);
      }
   }
   
   print "  <td width=\"1\">&nbsp;</td>\n"; # vertical separator
}

print " </tr>\n";

my $testid = 0;

# this is where we output the test results...
foreach $test_name (@tests)
{
   print " <tr bgcolor=\"white\">\n";
   print " <td>($testid)</td>\n";
   print " <td><b>$test_name</b></td>\n";
   
   foreach $model (@arch_cpu_models)
   {
      foreach $cores (@arch_cpu_cores)
      {
         foreach $mem (@arch_mem_fitted)
         {
            my $result = $results[$testid]{$model."_".$cores."_".$mem};

            if($result eq "OK")
            {
               print "  <td bgcolor=\"#0f0\">$result</td>\n";
            }
            else
            {
               print "  <td bgcolor=\"#f00\">$result</td>\n";
            }
         }
      }
      
      print "  <td width=\"1\">&nbsp;</td>\n"; # vertical separator
   }
   
   print " </tr>\n";
   $testid++;
}

print "</table>\n";
print "</body></html>\n";

exit 0;


# process_log(cpu model, number of cores, phys mem fitted)
# open the log file associated with this boot configuration and process
# any test results found, storing them into an array of hashes called $results
sub process_log
{
   open(my $log, '<', "./test/$arch_name/automated_$_[0]_$_[1]_$_[2].log") or die $!;
   
   while(<$log>)
   {
      if(/__DTS__test([0-9]*) ([OKFAIL]*)/)
      {
         $results[$1]{$_[0]."_".$_[1]."_".$_[2]} = $2;
      }
   }
   
   close $log;
}
