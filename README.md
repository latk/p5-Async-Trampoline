# NAME

Async::Trampoline - Trampolining functions with async/await syntax

# SYNOPSIS

    TODO

# DESCRIPTION

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

## Example: loop

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
        return deferred { loop($items, $i) };
    }

    await loop_async([], 5);

# FUNCTIONS

## await

    @result = await $async

    @result = $async->await

TODO

## deferred

    $async = deferred { ... }

TODO

## async

    $async = async $dependency => sub {
        my (@result) = @;
        ...
    };

    $async = $dependency->async(sub {
        my (@result) = @_;
        ...
    });

TODO

## async\_value

    $async = async_value @values

TODO

## async\_cancel

    $async = async_cancel

TODO

## async\_else

    $async = async_else @choices

TODO

# AUTHOR

amon - Lukas Atkinson (cpan: AMON) <amon@cpan.org>

# COPYRIGHT

Copyright 2017 Lukas Atkinson

This library is free software and may be distributed under the same terms as perl itself. See [http://dev.perl.org/licenses/](http://dev.perl.org/licenses/).
