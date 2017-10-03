package inc::ExtractCodeFromPod;
# Adapted from Test::Pod::Snippets
# <https://metacpan.org/pod/Test::Pod::Snippets>
# by YANICK
# which uses the Perl5 license

use strict;
use warnings;

use parent qw/ Pod::Parser /;

sub initialize {
    my ($self) = @_;
    $self->SUPER::initialize;
    $self->{ecfp_state} = ['test'];
}

sub command {
    my ($parser, $command, $paragraph, $line_no ) = @_;

    if ($command eq 'for') {
        return unless $paragraph =~ s/^\h* test \h*//x;

        return $parser->{ecfp_state}[-1] = 'ignore'
            if $paragraph =~ /^ ignore \s*$/x;

        $parser->{ecfp_state}[-1] = 'test';
        print_paragraph($parser, $paragraph, $line_no);
        return;
    }
    elsif($command eq 'begin') {
        return unless $paragraph =~ s/^\h* test \h*//x;

        push @{ $parser->{ecfp_state} },'insidebegin';
        print_paragraph($parser, $paragraph, $line_no);
        return;
    }
    elsif($command eq 'end') {
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
