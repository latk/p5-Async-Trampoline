package inc::ExtractCodeFromPod;
use strict;
use warnings;

use Moose;
with 'Dist::Zilla::Role::FileGatherer';

use namespace::autoclean;

use Dist::Zilla::File::InMemory;

has file => (
    is => 'ro',
    isa => 'ArrayRef',
    default => sub { [] },
);

sub mvp_multivalue_args { qw( file ) }

sub gather_files {
    my ($self) = @_;

    $self->add_file($_) for map { $self->process_file($_) } @{ $self->file };
}

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
    my ($self, $file) = @_;

    $self->log("extracting tests from $file");

    my $default_output = derive_output_filename($file);

    my $snippets = parse_snippets($file);
    my %outputs = extract_target_files($snippets, $default_output);

    my @in_memory_files;
    for my $output (sort keys %outputs) {
        $self->log("  generating file $output");

        my $code = $outputs{$output};
        $code = expand_line_directives($code);
        $code = fill_template(TEST_TEMPLATE, code => $code);

        push @in_memory_files, Dist::Zilla::File::InMemory->new({
            name => $output,
            content => $code,
        });
    }

    return @in_memory_files;
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

    my $parser = inc::ExtractCodeFromPod::Parser->new;

    $parser->parse_from_file($file => $collect_fh);

    return $snippets;
}

sub extract_target_files {
    my ($code, $default_output) = @_;

    my %files;
    my @current_file = ($default_output);

    for my $line (split /\R\K/, $code) {

        if (my ($directive) = $line =~ /^\#@@ output (.*)$/) {

            if ($directive =~ s/^begin \s+ (?=\S)//x) {
                push @current_file, undef;
            }
            elsif ($directive eq 'end') {
                pop @current_file;
                next;
            }

            if ($directive eq 'default') {
                $current_file[-1] = $default_output;
            }
            else {
                $current_file[-1] = $directive;
            }

            next;
        }

        $files{$current_file[-1]} .= $line;
    }

    return %files;
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

package inc::ExtractCodeFromPod::Parser;
# Adapted from Test::Pod::Snippets
# <https://metacpan.org/pod/Test::Pod::Snippets>
# by YANICK
# which uses the Perl5 license

use parent qw/ Pod::Parser /;

sub initialize {
    my ($self) = @_;
    $self->SUPER::initialize;
    $self->{ecfp_state} = ['test'];
}

sub command {
    my ($parser, $command, $paragraph, $line_no ) = @_;

    if ($command eq 'for') {

        if ($paragraph =~ s/^\h* test\b \h*//x) {

            return $parser->{ecfp_state}[-1] = 'ignore'
                if $paragraph =~ /^ ignore \s*$/x;

            $parser->{ecfp_state}[-1] = 'test';
            print_paragraph($parser, $paragraph, $line_no);
            return;
        }

        elsif ($paragraph =~ /^\h* output \h+ (begin \h+ \S+ | end | default | \S+) \s*$/x) {
            $parser->output_handle->print("#@@ output $1\n");
        }

    }
    elsif ($command eq 'begin') {
        return unless $paragraph =~ s/^\h* test \h*//x;

        push @{ $parser->{ecfp_state} },'insidebegin';
        print_paragraph($parser, $paragraph, $line_no);
        return;
    }
    elsif ($command eq 'end') {
        return unless $paragraph =~ s/^\h* test \h*//x;

        pop @{ $parser->{ecfp_state} };
        return;
    }
}

sub textblock {
    my ($self, $paragraph, $line_no) = @_;

    return $self->_ecfp_dispatch(textblock => $paragraph, $line_no);
}

sub interior_sequence {}

sub verbatim {
    my ($self, $paragraph, $line_no) = @_;

    return $self->_ecfp_dispatch(verbatim => $paragraph, $line_no);
}

sub _ecfp_dispatch {
    my ($self, $name, @args) = @_;
    my $state = $self->{ecfp_state}[-1];
    my $method = "_state_${state}_${name}";
    return $self->$method(@args);
}

sub _state_insidebegin_textblock {
    my ($self, $paragraph, $line_no) = @_;

    return print_paragraph($self, $paragraph, $line_no, keep_indent => 1);
}

sub _state_insidebegin_verbatim {
    my ($self, $paragraph, $line_no) = @_;

    return print_paragraph($self, $paragraph, $line_no, keep_indent => 1);
}

sub _state_test_textblock { }

sub _state_test_verbatim {
    my ($self, $paragraph, $line_no) = @_;

    return print_paragraph($self, $paragraph, $line_no);
}

sub _state_ignore_textblock { }

sub _state_ignore_verbatim { }

sub print_paragraph {
    my ($parser, $paragraph, $line_no, %options) = @_;
    my $keep_indent = delete $options{keep_indent} // 0;

    return if not length $paragraph;

    $DB::single = 1;
    my $filename = $parser->input_file || 'unknown';

    $line_no++ while $paragraph =~ s/^\R//;

    unless ($keep_indent) {
        $paragraph =~ /^(\h*)/;
        my $indent = $1;
        $paragraph =~ s/^$indent//mg;
    }

    $paragraph = "#line $line_no $filename\n".$paragraph;

    print {$parser->output_handle} $paragraph, "\n";
}


1;

__END__
