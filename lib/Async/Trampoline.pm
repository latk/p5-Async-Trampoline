use strict;
use warnings;
use utf8;

package Async::Trampoline;

=encoding utf8

=head1 NAME

Async::Trampoline - Trampolining functions with async/await syntax

=head1 SYNOPSIS

    TODO

=head1 DESCRIPTION

Trampolines are a functional programming technique
to implement complex control flow:
Instead of returning a result from a function,
we can return another function that will at some point return a result.
The trampoline keeps invoking the returned function
until a result is returned.
Importantly, such trampolines eliminate tail calls.

This programming style is powerful but inconvenient because you tend to get callback hell.
This module implements simple Futures with an async/await syntax.
Instead of nesting the callbacks, we can now chain callbacks more easily.

=head2 Example: loop

Synchronous/imperative:

    my @items;

    my $i = 5;
    while ($i) {
        push @items, $i--;
    }

Synchronous/recursive:

    sub loop {
        my ($items, $i) = @_;
        return $items if not $i;
        push @$items, $i--;
        return loop($items, $i);  # may lead to deep recursion!
    }

    loop([], 5);

Async/recursive:

    sub loop_async {
        my ($items, $i) = @_;
        return async_value $items if not $i:
        push @$items, $i--;
        return async { loop($items, $i) };
    }

    run_until_completion loop_async([], 5);

=head1 ASYNC STATES

Each Async exists in one of these states:

    Async
    +-- Incomplete
        +-- ... (internal)
    +-- Complete
        +-- Cancelled
        +-- Resolved
            +-- Error
            +-- Value

In Incomplete states, the Async will be processed in the future.
At some point, the Async will transition to a completed state.

In C<async> and C<await> callbacks,
the Async will be updated to the state of the return value of that callback.

Completed states are terminal.
The Asyncs are not subject to further processing.

A Cancelled Async represents an aborted computation.
They have no value.
Cancellation is not an error,
but C<run_until_completion()> will die when the Async was cancelled.
You can cancel a computation via the C<async_cancel> constructor.

Resolved Async are Completed Asyncs that finished their computation
and have a value, either an Error or a Value upon success.

An Error Async indicates that a runtime error occurred.
Error Asyncs can be created with the C<async_error> constructor,
or when a callback throws.
The exception will be rethrown by C<run_until_completion()>.

A Value Async contains a list of Perl values.
They can be created with the C<async_value> constructor.
The values will be returned by C<run_until_completion()>.
To access the values of an Async, you can C<await> it.

=head1 FUNCTIONS

=cut

## no critic
our $VERSION = '0.000001';
$VERSION = eval $VERSION;
## use critic

use XSLoader;
XSLoader::load __PACKAGE__, $VERSION;

use Exporter 'import';

our %EXPORT_TAGS = (
    all => [qw/
        run_until_completion
        await
        async
        async_value
        async_error
        async_cancel
    /],
);

our @EXPORT_OK = @{ $EXPORT_TAGS{all} };

use constant DEBUG => $ENV{ASYNC_TRAMPOLINE_DEBUG};

use Async::Trampoline::Scheduler;

## no critic (ProhibitSubroutinePrototypes)

=head2 run_until_completion

    @result = run_until_completion $async

    @result = $async->run_until_completion

TODO

=head2 async

    $async = async { ... }

TODO

=head2 await

    $async = await $dependency => sub {
        my (@result) = @;
        ...
    };

    $async = $dependency->await(sub {
        my (@result) = @_;
        ...
    });

TODO

=head2 async_value

    $async = async_value @values

TODO

=head2 async_error

    $async = async_error $error

TODO

=cut

sub async_error($;) { ... }  # TODO

=head2 async_cancel

    $async = async_cancel

TODO

=head2 resolved_or

=head2 async_resolved_or

    $async = $first_async->resolved_or($alternative_async)
    $async = async_resolved_or $first_async, $alternative_async

TODO

=cut

sub resolved_or :method {
    goto &async_resolved_or;
}

=head2 to_string

    $str = $async->to_string
    $str = "$async"

Low-level debugging stringification that displays Async identity and type.

=cut

use overload
    fallback => 1,
    q("") => sub {
        my ($self) = @_;
        return $self->to_string;
    };

=head2 is_complete

=head2 is_cancelled

=head2 is_resolved

=head2 is_error

=head2 is_value

    $bool = $async->is_complete;
    $bool = $async->is_cancelled;
    $bool = $async->is_resolved;
    $bool = $async->is_error;
    $bool = $async->is_value;

Inspect the state of an Async (see L<"Async States"|/"ASYNC STATES">).

=cut

1;

__END__

=head1 AUTHOR

amon - Lukas Atkinson (cpan: AMON) <amon@cpan.org>

=head1 COPYRIGHT

Copyright 2017 Lukas Atkinson

This library is free software and may be distributed under the same terms as perl itself. See L<http://dev.perl.org/licenses/>.

=cut
