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
        return async { loop($items, $i) };
    }

    run_until_completion loop_async([], 5);

# ASYNC STATES

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

In `async` and `await` callbacks,
the Async will be updated to the state of the return value of that callback.

Completed states are terminal.
The Asyncs are not subject to further processing.

A Cancelled Async represents an aborted computation.
They have no value.
Cancellation is not an error,
but `run_until_completion()` will die when the Async was cancelled.
You can cancel a computation via the `async_cancel` constructor.

Resolved Async are Completed Asyncs that finished their computation
and have a value, either an Error or a Value upon success.

An Error Async indicates that a runtime error occurred.
Error Asyncs can be created with the `async_error` constructor,
or when a callback throws.
The exception will be rethrown by `run_until_completion()`.

A Value Async contains a list of Perl values.
They can be created with the `async_value` constructor.
The values will be returned by `run_until_completion()`.
To access the values of an Async, you can `await` it.

# FUNCTIONS

## run\_until\_completion

    @result = $async->run_until_completion

TODO

## async

    $async = async { ... }

TODO

## await

    $async = await $dependency => sub {
        my (@result) = @;
        ...
    };

    $async = $dependency->await(sub {
        my (@result) = @_;
        ...
    });

TODO

## async\_value

    $async = async_value @values

TODO

## async\_error

    $async = async_error $error

TODO

## async\_cancel

    $async = async_cancel

TODO

## resolved\_or

    $async = $first_async->resolved_or($alternative_async)

TODO

## to\_string

    $str = $async->to_string
    $str = "$async"

Low-level debugging stringification that displays Async identity and type.

## is\_complete

## is\_cancelled

## is\_resolved

## is\_error

## is\_value

    $bool = $async->is_complete;
    $bool = $async->is_cancelled;
    $bool = $async->is_resolved;
    $bool = $async->is_error;
    $bool = $async->is_value;

Inspect the state of an Async (see ["Async States"](#async-states)).

# AUTHOR

amon - Lukas Atkinson (cpan: AMON) <amon@cpan.org>

# COPYRIGHT

Copyright 2017 Lukas Atkinson

This library is free software and may be distributed under the same terms as perl itself. See [http://dev.perl.org/licenses/](http://dev.perl.org/licenses/).
