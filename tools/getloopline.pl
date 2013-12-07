#!/bin/perl
use strict;
use warnings;

$|++;

# We are looking for blocks in the objdumpdump of the form
#  /home/ntaylor/code/loopgrind/tests/function.c:20
#  8048447:       83 44 24 1c 01          addl   $0x1,0x1c(%esp)
#  804844c:       83 7c 24 1c 63          cmpl   $0x63,0x1c(%esp)
#  8048451:       7e e8                   jle    804843b <main+0x35>
#
# That is, file and line comes before the instructions.

my ($executable, $addr) = @ARGV;

my ($file, $line);

my @objdump_lines = `objdump -dsl $executable`;

for my $l (@objdump_lines) {
    if ($l =~ /(\S+\.c):(\d+)/) {
        $file = $1;
        $line = $2;
    }
    elsif (my ($address, $insts) = ($l =~ /([0-9a-f]+):\s*(.*)/)) {
        if (hex $address eq hex $addr) {
#            print "Main loop header $address found at $file:$line\n";
            last;
        }
    }
}

open SOURCE_FILE, $file or die $!;
my @source_file = <SOURCE_FILE>;
print $source_file[$line - 1];
