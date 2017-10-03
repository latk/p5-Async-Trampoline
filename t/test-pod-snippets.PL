#!/usr/bin/env perl

use strict;
use warnings;
use utf8;
use feature qw( say state );

use inc::ExtractCodeFromPod;

process_file('lib/Async/Trampoline.pm');

exit;

sub fix_line_directives {
    my ($template, $file, $line) = @_;
    unless (defined $file and defined $line) {
        my (undef, $caller_file, $caller_line) = caller;
        $file //= $caller_file;
        $line //= $caller_line + 1;
    }

    my @lines = split /\n/, $template;

    for my $i (0 .. $#lines) {
        next unless $lines[$i] eq '#line fixup';
        my $next_line_no = $line + $i + 1;
        $lines[$i] = "#line $next_line_no $file";
    }

    return join "\n", @lines;
}

use constant TEST_TEMPLATE => fix_line_directives(<<'TEST_TEMPLATE');
#line fixup
use strict;
use warnings;
use Test::More;

{{code}}

#line fixup
done_testing;
TEST_TEMPLATE

sub process_file {
    my ($file) = @_;

    say "extracting tests from $file";

    my $output = derive_output_filename($file);

    my $code = parse_snippets($file);
    $code = expand_line_directives($code);
    $code = fill_template(TEST_TEMPLATE, code => $code);

    open my $fh, '>:encoding(UTF-8)', $output
        or die "Cannot open $output: $!";

    $fh->print($code);

    close $fh;

    return;
}

sub fill_template {
    my ($template, %args) = @_;
    $template =~ s/\{\{\s*(\w+)\s*\}\}/$args{$1}/g;
    return $template;
}

sub derive_output_filename {
    my ($file) = @_;

    $file =~ s|^lib/||;

    my $slug = $file;
    for ($slug) {
        s/\.(?:pm|pod)$//;
        s/\W+/-/g;
        s/^-|-$//g;
    }

    return "t/podsnippets-$slug.t";
}

sub parse_snippets {
    my ($file) = @_;

    my $snippets;
    open my $collect_fh, '>', \$snippets
        or die qq(Can't open string as file handle);

    state $parser = inc::ExtractCodeFromPod->new;

    $parser->parse_from_file($file => $collect_fh);

    return $snippets;
}

sub expand_line_directives {
    my ($code) = @_;
    my @lines_in = split /\R/, $code;
    my @lines_out;

    my $cur_file;
    my $cur_line = 0;
    while (defined(my $line = shift @lines_in)) {

        if (my ($no, $file) = $line =~ /^\#line \s+ ([0-9]+) \s+ (.+)$/x) {
            my $reverted = 0;
            while (@lines_out and $lines_out[-1] eq q()) {
                pop @lines_out;
                ++$reverted;
            }

            my $skipped_lines = $no - $cur_line - 1 + $reverted;

            if (defined $cur_file
                    and $cur_file eq $file
                    and 0 <= $skipped_lines and $skipped_lines < 10) {
                push @lines_out, ( q() ) x $skipped_lines;
                $cur_line += $skipped_lines - $reverted;
                next;
            }

            ($cur_file, $cur_line) = ($file, $no - 1);
            push @lines_out, q(), $line;
            next;
        }

        push @lines_out, $line;
        ++$cur_line;
    }

    return join "\n", @lines_out;
}
