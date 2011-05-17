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


# syntax: process-testsuite-logs.pl <target name>
#
# this script will process all files in the target's test directory (./test/<target>/automated_*.log)
# and output a big HTML-formatted report to stdout.

# clean the args
$ARGV[0] =~ s/[^A-Za-z0-9_]//g;

if()

print "[+] entering automated integration testing for $diosix_arch\n";

foreach $model (@cpu_models)
{
   &print_rule("=");
   print "[+] CPU MODEL: $model\n";
   
   foreach $cores (@cpu_cores)
   {
      &print_rule("-");
      print "[+] NUMBER OF CORES: $cores\n";
      
      foreach $mem (@mem_fitted)
      {
         my $result;
         print "$mem ";

         print "($result) ";
      }
      
      print "\n";
   }
}

exit 0;


# prnt_rule(str) - print a line of 70 x str
sub print_rule
{
   my $loop;
   
   for($loop = 0; $loop < 70; $loop++)
   {
      print $_[0];
   }
   
   print "\n";
}
