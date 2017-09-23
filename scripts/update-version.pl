#!/usr/bin/env perl

use strict;
use warnings;
use utf8;
use feature 'say';

use File::Find;

my ($version, @dirs) = @ARGV;

if (not length $version or not @dirs) {
    die qq(Usage: $0 VERSION DIRS...\n);
}

find(\&process_file, @dirs);

sub process_file {
    my $file = $_;

    return if not $file =~ /\A \w+\.pm \z/x;
    return if not -f $file;

    my @contents = slurp_text_lines($file);

    my $was_changed = 0;

    for (@contents) {
        next if not
            s{^ (?<lead> \h*) our \h* \$VERSION \h*
                = \h* '(?<oldversion>[0-9._]+)'
                ; \h* \# \h* VERSION \h* $}
            {$+{lead}our \$VERSION = '$version';  # VERSION}x;

        next if $+{oldversion} eq $version;

        say qq($0: $File::Find::name: $+{oldversion} -> $version);
        $was_changed = 1;
    }

    if ($was_changed) {
        spew_text($file => @contents);
    }
    else {
        say qq($0: $File::Find::name: no changes);
    }
}

sub slurp_text_lines {
    my ($name) = @_;
    open my $fh, '<:encoding(UTF-8)', $name or die "Can't slurp $name: $!";
    return <$fh>;
}

sub spew_text {
    my ($name, @contents) = @_;
    open my $fh, '>:encoding(UTF-8)', $name or die "Can't open $name: $!";
    print { $fh } @contents;
    close $fh or die "Can't close $name: $!";
    return;
}
