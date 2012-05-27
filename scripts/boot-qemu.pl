#!/usr/bin/perl

# ----------------------------------------------------------------------
# scripts/boot-qemu.pl
# Test the kernel and OS in QEMU, wither by automating a series of
# tests or by running a standalone instance of the diosix system
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

#
# syntax: boot-qemu.pl <arch target> [<cpu> <cpus> <memory>]
#
# Start up QEMU for the given architecture target. If only one argument is given
# then run the intergration testsuite on the target. If cpu, cpus and memory are specified
# then perform a standalone boot of the system using the given configuration within the target

# define how many seconds each run is allowed to execute for
$qemu_timeout = 10;

if($ARGV[0] eq "")
{
   print "syntax: boot-qemu.pl <i386_pc | arm_versatilepb> [<cpu model> <number of cpus> <physical RAM>]\n";
   exit 1;
}

# clean the arch target argument
$ARGV[0] =~ s/[^A-Za-z0-9_]//g;

# select the correct arch
require "test/$ARGV[0].pl" or die $!;

# should just a single standalone run be attempted?
if($ARGV[1] ne "")
{
   # clean the args
   $ARGV[1] =~ s/[^A-Za-z0-9]//g;  # cpu model
   $ARGV[2] =~ s/[^0-9]//g;        # nr of cpus
   $ARGV[3] =~ s/[^0-9MK]//g;      # phys RAM fitted
   
   print "[+] entering standalone run of $arch_name build: $ARGV[2] cpu(s) of $ARGV[1] with $ARGV[3] RAM, Live CD boot\n";
   system &arch_generate_cmdline($ARGV[1], $ARGV[2], $ARGV[3], "standalone", "-nographic");
   exit 0;
}

print "[+] entering automated integration testing for $arch_name\n";

# iterate through the different combinations specified by the arch target
foreach $model (@arch_cpu_models)
{
   &print_rule("=");
   print "[+] CPU MODEL: $model\n";
   
   foreach $cores (@arch_cpu_cores)
   {
      &print_rule("-");
      print "[+] NUMBER OF CORES: $cores\n";
      
      foreach $mem (@arch_mem_fitted)
      {
         my $result;
         print "$mem ";
         
         $result = &boot($model, $cores, $mem);
         
         print "($result) ";
      }
      
      print "\n";
   }
}

exit 0;

# boot(cpu model, number of cores, memory fitted)
# Start the specified machine in QEMU until the given timeout
sub boot
{
   my $pidfile = &generate_pathname_prefix($_[0], $_[1], $_[2], "automated");
   
   # run the QEMU instance in a separate child process
   if(fork() == 0)
   {
      system &arch_generate_cmdline($_[0], $_[1], $_[2], "automated", "-nographic");
      exit 0;
   }

   # wait some time for it to run
   sleep $qemu_timeout;
   
   # grab the contents of the file containing the QEMU pid
   open my $input, '<', "$pidfile.pid" or die "can't open $file: $!";
   $pid = <$input>;
   close $input;
   unlink "$pidfile.pid";
   
   # kill off the QEMU instance
   kill 1, $pid;

   return "done";
}

# generate_pathname_prefix
# Craft a pathname prefix to stick in front of QEMU file output for the given configuration 
#  => cpu name = QEMU shorthand cpu name [0]
#     number of cpus = number of cores present in system [1]
#     amount of RAM = physical mem fitted, expressed as "xM" or "xG" [2]
#     prefix = string to prefix to filenames for output generated by QEMU [3]
#  <= return pathname prefix to use for QEMU output
sub generate_pathname_prefix
{
   return "./test/$arch_name/$_[3]_$_[0]_$_[1]_$_[2]";
}

# prnt_rule(str)
# Print a repetition of the given string 70 times
sub print_rule
{
   my $loop;
   
   for($loop = 0; $loop < 70; $loop++)
   {
      print $_[0];
   }
   
   print "\n";
}
