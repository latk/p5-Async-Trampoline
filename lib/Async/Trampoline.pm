use strict;
use warnings;
use utf8;

package Async::Trampoline;

=head1 NAME

Async::Trampoline - Trampolining functions with async/await syntax

=head1 SYNOPSIS

    TODO

=head1 DESCRIPTION

TODO

=cut

## no critic
our $VERSION = '0.000001';
$VERSION = eval $VERSION;
## use critic

use Exporter 'import';

our @EXPORT_OK = qw(
    await
    async
    async_value
);

## no critic (ProhibitSubroutinePrototypes)

=head2 await

    @result = await $async

TODO

=cut

sub await {
    my ($async) = @_;

    my @runnable;

    my $enqueue = sub {
        my ($async, $notify) = @_;
        push @runnable, { async => $async, notify => $notify };
        return;
    };

    my $async_is_ready = sub {
        my ($async) = @_;
        return $async->[0] eq 'value';
    };

    $enqueue->($async, undef);

    TASK:
    while (@runnable) {
        my $top = shift @runnable;
        my $top_async = $top->{async};
        my $top_notify = $top->{notify};

        my ($type, @args) = @$top_async;

        if ($type eq 'value') {
            push @runnable, $top_notify if $top_notify;
            next TASK;
        }

        if ($type eq 'thunk') {
            my ($dep, $body) = @args;

            if (not $async_is_ready->($dep)) {
                $enqueue->($dep, $top);
                next TASK;
            }

            @$top_async = @{ $body->(@$dep[1 .. $#$dep]) };
            push @runnable, $top;
            next TASK;
        }

        # uncoverable statement
        die "Unknown async type $type";
    }

    my ($type, @args) = @$async;

    return @args[0 .. $#args] if $type eq 'value';

    # uncoverable statement
    die "Async with type $type was not resolved in time";
}

=head2 async

    $async = async $dependency => sub { ... }

TODO

=cut

sub async($&) {
    my ($dep, $body) = @_;
    return bless [thunk => $dep, $body] => __PACKAGE__;
}

=head2 async_value

    $async = async_value @values

TODO

=cut

sub async_value(@) {
    return bless [value => @_] => __PACKAGE__;
}

1;

__END__

=head1 AUTHOR

amon - Lukas Atkinson (cpan: AMON) <amon@cpan.org>

=head1 COPYRIGHT

Copyright 2017 Lukas Atkinson

This library is free software and may be distributed under the same terms as perl itself. See L<http://dev.perl.org/licenses/>.

=cut
