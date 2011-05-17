#!/usr/bin/perl

# automate test system.
# running this script with no params will begin the test suite.
# alternatively, call with ./boot-i386_pc.pl <cpu> <cpus> <ram> to try the live cd with that one requested system

# describe the possible qemu combinations
@cpu_models = ( "n270",
                "athlon",
                "pentium3",
                "pentium2",
                "pentium",
                "486",
                "coreduo",
                "qemu32",
                "core2duo",
                "phenom",
                "qemu64" );
@cpu_cores  = ( "1", "2" );
@mem_fitted = ( "16M", "32M", "512M" );

# define the qemu to use, the kernel arch and how many seconds each run is allowed to execute for
$qemu_exe = "qemu-system-i386";
$diosix_arch = "i386_pc";
$qemu_timeout = 3;

# should just a single standalone run be attempted?
if($ARGV[0] ne "")
{
   # clean the args
   $ARGV[0] =~ s/[^A-Za-z0-9]//g;
   $ARGV[1] =~ s/[^0-9]//g;
   $ARGV[2] =~ s/[^0-9MK]//g;
   
   $qemu_exe = "$qemu_exe -M pc -cpu $ARGV[0] -m $ARGV[2] -smp $ARGV[1] -nographic -serial file:./test/$diosix_arch/standalone_$ARGV[0]_$ARGV[1]_$ARGV[2].log -name diosix_$diosix_arch -boot order=d -cdrom release/$diosix_arch/cd.iso";
   
   print "[+] entering standalone run of $diosix_arch build: $ARGV[1] cpu(s) on $ARGV[0] with $ARGV[2] RAM, Live CD boot";
   system $qemu_exe;
   exit 0;
}

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
         
         $result = &boot($model, $cores, $mem);
         
         print "($result) ";
      }
      
      print "\n";
   }
}

exit 0;

# boot(cpu model, number of cores, memory fitted) - boot the specified machine in qemu
sub boot
{
   my $boot_output = "./test/$diosix_arch/automated_$_[0]_$_[1]_$_[2]";
   
   my $boot_exe = "$qemu_exe -M pc -cpu $_[0] -m $_[2] -smp $_[1] -nographic -pidfile $boot_output.pid -serial file:$boot_output.log -name diosix_$diosix_arch -boot order=d -cdrom release/$diosix_arch/cd.iso >/dev/null 2>/dev/null";
      
   # run the QEMU instance in a separate child process
   if(fork() == 0)
   {
      system $boot_exe;
      exit 0;
   }

   # wait some time for it to run
   sleep $qemu_timeout;
   
   # grab the contents of the file containing the QEMU pid
   open my $input, '<', "$boot_output.pid" or die "can't open $file: $!";
   $pid = <$input>;
   close $input;
   unlink "$boot_output.pid";
   
   # kill off the QEMU instance
   kill 1, $pid;

   return "done";
}

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
