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

    my $items = loop([], 5);

Async/recursive:

    sub loop_async {
        my ($items, $i) = @_;
        return async_value $items if not $i:
        push @$items, $i--;
        return async { loop_async($items, $i) };
    }

    my $items = loop_async([], 5)->run_until_completion;

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

In **Incomplete** states, the Async will be processed in the future.
At some point, the Async will transition to a completed state.

In `async` and `await` callbacks,
the Async will be updated to the state of the return value of that callback.

**Completed** states are terminal.
The Asyncs are not subject to further processing.

A **Cancelled Async** represents an aborted computation.
They have no value.
Cancellation is not an error,
but `run_until_completion()` will die when the Async was cancelled.
You can cancel a computation via the `async_cancel` constructor.

**Resolved** Asyncs are Completed Asyncs that finished their computation
and have a value, either an Error or a Value upon success.

An **Error** Async indicates that a runtime error occurred.
Error Asyncs can be created with the `async_error` constructor,
or when a callback throws.
The exception will be rethrown by `run_until_completion()`.

A **Value** Async contains a list of Perl values.
They can be created with the `async_value` constructor.
The values will be returned by `run_until_completion()`.
To access the values of an Async, you can `await` it.

# CREATING ASYNCS

## async

    $async = async { ... }

Create an Incomplete Async with a code block.
The callback must return an Async.
When the Async is evaluated,
this Async is updated to the state of the returned Async.

## async\_value

    $async = async_value @values

Create a Value Async containing a list of values.
Use this to return values from an Async callback.

## async\_error

    $async = async_error $error

Create an Error Async with the specified error.
The error may be a string or error object.
Use this to fail an Async.
Alternatively, you can `die()` inside the Async callback.

## async\_cancel

    $async = async_cancel

Create a Cancelled Async.
Use this to abort an Async without using an error.

# COMBINING ASYNCS

## await

    $async = await $dependency => sub {
        my (@result) = @;
        ...
    };

    $async = $dependency->await(sub {
        my (@result) = @_;
        ...
    });

Wait until the `$dependency` Async has a value,
then call the callback with the values as arguments.
If the dependency was cancelled or has an error,
the async is updated to that state.
The callback must return an Async.
Use this to chain Asyncs.
It does not directly return the values.

## resolved\_or

## value\_or

    $async = $first_async->resolved_or($alternative_async)
    $async = $first_async->value_or($alternative_async)

Evaluate the `$first_async`.
Upon success, the Async is updated to the state of the `$first_async`.
On failure, the `$second_async` is evaluated instead.
This creates a new Async that will be updated
when the dependencies become available.

**resolved\_or** succeeds on Value or Error, and fails on Cancelled.
Use this as a fallback against cancellation.

**value\_or** only succeeds on Value, and fails on Cancelled or Error.
Use this as a try-catch to provide default values upon errors.

## complete\_then

## resolved\_then

## value\_then

    $async = $first_async->complete_then($second_async)
    $async = $first_async->resolved_then($second_async)
    $async = $first_async->value_then($second_async)

Evaluate the `$first_async`.
Upon success, the `$second_async` is evaluated.
On failure, the Async is updated to the state of the `$first_async`.
This creates a new Async that will be updated
when the dependencies become available.

**complete\_then** always succeeds (Cancelled, Error, Value).

**resolved\_then** succeeds on Error or Value, and fails on Cancelled.

**value\_then** succeeds on Value, and fails on Cancelled or Error.
With regards to error propagation,
`$x->value_then($y)`
behaves just like
`$x->await(sub { return $y }`.

Use these functions to sequence actions on success and discarding their value.
They are like a semicolon `;` in Perl,
but with different levels of error propagation.
You may want to sequence Asyncs if any Async causes side effects.

## concat

    $async = $first_async->concat($second_async)

If both asyncs evaluate to Values, concatenate the values.

**Example**:

    my $async = (async_value 1, 2, 3)->concat(async_value 4, 5);
    #=> async_value 1, 2, 3, 4, 5

# GENERATORS

A **Generator** describes an Async
that has a continuation Async as its first value.
This continuation can be awaited to get the next continuation + value.
If the generator is Cancelled, no further items are available.
Errors are propagated.

Generators are useful for yielding a stream of values.

You can use `async_yield()` to conveniently return a value with a continuation.
The `gen_*()` Async methods can process generator streams.
They will fail at runtime when the Async is not a valid generator.

The most flexible way to handle generators is to `await()` them.
However, many use cases are better served by more specialized functions.

**Example:** a count down generator:

    sub count_down_generator {
        my ($from) = @_;
        return async_cancel if $i < 0;
        return async_yield async_value($i) => sub {
            return count_down_generator($i - 1);
        };
    }

    my $countdown_gen = count_down_generator(10);

**Example:** transforming a stream:

    $countdown_gen = $countdown_gen->gen_map(sub {
        my ($i) = @_;
        return async_value "ignition" if $i == 3;
        return async_value "liftoff"  if $i == 0;
        return async_value $i;
    });

**Example:** consuming a stream:

    my $finished_async = $countdown_gen->gen_foreach(sub {
        my ($i) = @_;
        say $i;
    });

**Example**: repeating each element:

    sub repeat_gen {
        my ($gen) = @_;
        return $gen->await(sub {
            my ($continuation, $x) = @_;
            return async_yield async_value($x) => sub {
                return async_yield async_value($x) => sub {
                    repeat_gen($continuation);
                };
            };
        });
    }

## async\_yield

    $generator = async_yield $async => sub { ... }

Yield a value from a generator function.
The `$async` contains the value or state you want to yield.
The callback will be executed to yield the next value.
It receives no arguments.
It must return a valid generator.

## gen\_map

    $generator = $generator->gen_map(sub { ... })

Transform the values yielded by a generator.
The callback receives the values of the current item as parameters.
The callback must return an Async, usually a value.
It may also return `async_cancel` to terminate the Generator,
or `async_error`.

You cannot return multiple Asyncs (at most a multi-value Async).
Returning a Generator Async is not meaningful,
and it will be treated as an ordinary value.

## gen\_foreach

    $async = $generator->gen_foreach(sub { ... })

Consume a generator.
The callback will be invoked with each item's values.
The callback may return an Async Value to receive the next value,
or may return an Async Error or Async Cancel to abort the loop.

The returned Async is
an empty Value when the loop completes successfully or was aborted,
and an Error when there was an error in the loop body or in the generator.

## gen\_collect

    $async = $generator->gen_collect

Collects all items in an array ref.
This will consume the whole stream, so only works for finite streams.

# OTHER FUNCTIONS

## run\_until\_completion

    @result = $async->run_until_completion

Creates and event loop and blocks until the `$async` is completed.
If it was cancelled, throws an exception.
If it was an error, rethrows that error.
If it was a value, the values are returned as a list.

This call should be used sparingly, usually once per program.
Sharing Asyncs between multiple event loops may lead to unexpected results.

If you want to use the results of an Async to continue within an Async context,
you usually want to `await()` the Async instead.

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
