use strict;
use warnings;
use utf8;

package Async::Trampoline;

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

use overload
    fallback => 1,
    q("") => sub {
        my ($self) = @_;

        require Scalar::Util;

        return sprintf "<Async 0x%x to 0x%x>",
            Scalar::Util::refaddr($self), $$self;
    };

1;

__END__

=head1 AUTHOR

amon - Lukas Atkinson (cpan: AMON) <amon@cpan.org>

=head1 COPYRIGHT

Copyright 2017 Lukas Atkinson

This library is free software and may be distributed under the same terms as perl itself. See L<http://dev.perl.org/licenses/>.

=cut
