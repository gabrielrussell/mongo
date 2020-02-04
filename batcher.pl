#!/usr/bin/perl

use strict;
use Text::Glob qw(match_glob);

my $batch_reviewers = {};
my $found_batches = {};
my $tickets = {};

open(my $tickets_file, "tickets.txt") or die $!;
while (<$tickets_file>) {
    next if /^#/;
    s/\r?\n//;
    my @fields = split(/\t/);
    $tickets->{$fields[1]}=$fields[0];
}

while (<STDIN>) {
    next if /^#/;
    s/\r?\n//;
    my @fields = split(/\t/);
    $batch_reviewers->{$fields[0]}="$fields[2]/$fields[1]";
}

open(my $files_file, "logging_cpp_files.txt") or die $!;
while (<$files_file>) {
    next if /^#/;
    s/\r?\n//;
    my $match = "";
    #print("FILE $_\n");
    foreach my $key (sort { length($b) <=> length($a) } keys %$batch_reviewers) {
        my $regex;
        if ($key =~ /\*/) {
            $regex = $key;
            if ($key =~ /\//) {
                $regex =~ s[\*][\[^\/\]\*]g;
            } else {
                $regex =~ s[\*][\.\*]g;
            }
            $regex = qr($regex);
        } else {
            $regex = "$key/.*";
        }
        #print ("REGEX: $regex\n");
        if (m[$regex]) {
            #print("MATCH!!\n");
            $match = $key;
            last;
        }
    }
    if ($match) {
        push(@{$found_batches->{$match}},$_);
    } else {
        print("?\tunknown\t$_\n");
    }
}

sub run {
    my @cmd = @_;
    print( join(" ",map { / /?"\"$_\"":$_ } @cmd),"\n" );
    system(@cmd);
}

sub patch {
    my $filter = shift;
    foreach my $reviewer (sort keys %$tickets) {
        next unless $reviewer =~ m/$filter/;
        print("REVIEWER $reviewer\n");
        my @files;
        foreach my $batch (sort keys %$batch_reviewers) {
            if ($batch_reviewers->{$batch} ne $reviewer) {
                next;
            }
            if (! defined $found_batches->{$batch}) {
                next;
            }
            print("BATCH $batch $batch_reviewers->{$batch} $tickets->{$batch_reviewers->{$batch}} @{$found_batches->{$batch}}\n");
            push(@files, @{$found_batches->{$batch}});
        }
        next unless @files;
        print (join(", ",@files)."\n\n");
        run(qw(git add logging_cpp_files.txt batcher.pl logv1tologv2 run.sh));
        #    run(qw(git commit -m xxx));
        run(qw(git cifa));
        run("./logv1tologv2",@files); 
        run(qw(buildscripts/clang_format.py format));
        run(qw(evergreen patch -p mongodb-mongo-master  --yes -a required -f), "-d", "structured logging auto-conversion of $reviewer");
    }
}

sub upload {
    my $filter = shift;
    for my $batch (sort keys %$found_batches) {
        next unless $batch_reviewers->{$batch} =~ m/$filter/;
        my @files = @{$found_batches->{$batch}};
        print ("BATCH $batch $batch_reviewers->{$batch}\n");
        run(qw(git add logging_cpp_files.txt batcher.pl logv1tologv2 run.sh));
        run(qw(git commit -m xxx));
        run(qw(git cifa));
        run("./logv1tologv2",@files); 
        run(qw(buildscripts/clang_format.py format));
        run(qw(evergreen patch -p mongodb-mongo-master  --yes -a required -f), "-d", "structured logging auto-conversion of $batch");
        run(qw(python ~/git/kernel-tools/codereview/upload.py --git_no_find_copies -y),"-r", $batch_reviewers->{$batch}, "--send_mail", "-m", "structured logging auto-conversion of ".$batch, "HEAD");
    }
}

my $filter = shift;
upload($filter);
