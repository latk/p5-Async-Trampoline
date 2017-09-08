#!/usr/bin/env perl

use strict;
use warnings;
use utf8;

use FindBin;
use lib "$FindBin::Bin/lib";

use Async::Trampoline::Describe qw(describe it);
use Test::More;
use Test::Exception;

use Async::Trampoline ':all';

describe q(monad laws) => sub {
    # async_value $x        =   return x
    # await $async => \&f   =   await >>= f

    my $f = sub {
        my ($x) = @_;
        return async_value "f($x)";
    };

    my $g = sub {
        my ($x) = @_;
        return async_value "g($x)";
    };

    it q(satisfies left identity: return a >>= f === f a) => sub {
        my $return_a_bind_f = await async_value("a") => \&$f;
        my $f_a = $f->("a");

        is run_until_completion($return_a_bind_f), run_until_completion($f_a);
        is run_until_completion($f_a), 'f(a)';
    };

    it q(satisfies right identity: m >>= return === m) => sub {
        my $id = [];
        my $m = async_value $id;
        my $m_bind_return = await $m => \&async_value;

        is run_until_completion($m), run_until_completion($m_bind_return);
        is run_until_completion($m), "$id";
    };

    it q(satisfies associativity: (m >>= f) >>= g === m >>= (x -> f x >>= g)) => sub {
        my $id = [];
        my $m = async_value $id;
        my $m_bind_f_bind_g = await(await($m => \&$f) => \&$g);
        my $m_bind_x_f_x_bind_g = await $m => sub {
            my ($x) = @_;
            return await $f->($x) => \&$g;
        };

        is run_until_completion($m_bind_f_bind_g),
            run_until_completion($m_bind_x_f_x_bind_g);
        is run_until_completion($m_bind_f_bind_g), "g(f($id))";
    };
};

describe q(resolved_or) => sub {
    it q(returns the first value) => sub {
        my $async = async_value(42)->resolved_or(async_cancel);
        is run_until_completion($async), 42;
    };

    it q(skips cancelled values) => sub {
        my $async = async_cancel->resolved_or(async_value "foo");
        is run_until_completion($async), "foo";
    };

    it q(can evaluate thunks) => sub {
        my $async =
            (async { async_cancel })
            ->resolved_or(async_value "bar");
        is run_until_completion($async), "bar";
    };

    it q(dies if no uncancelled values exist) => sub {
        my $async = async_cancel->resolved_or(async_cancel);

        throws_ok { run_until_completion($async) }
            qr/^run_until_completion\(\): Async was cancelled/;
    };
};

sub _loop {
    my ($items, $i) = @_;
    return async_value $items if not $i;
    push @$items, $i--;
    return async { _loop($items, $i) };
}

describe q(Scheduler) => sub {

    it q(reuses queue space successfully) => sub {
        my $scheduler = Async::Trampoline::Scheduler->new(5);
        my @values = (0 .. 40);
        my @results;

        $scheduler->enqueue(async_value 0);
        for my $x (@values[1 .. $#values]) {
            $scheduler->enqueue(async_value $x);
            push @results, run_until_completion $scheduler->dequeue;
        }
        while (my ($async) = $scheduler->dequeue) {
            push @results, run_until_completion $async;
        }
        is "@results", "@values";
    };

    it q(handles many Asyncs) => sub {
        my $scheduler = Async::Trampoline::Scheduler->new(2);
        my @values = (0 .. 40);
        $scheduler->enqueue(async_value $_) for @values;
        my @results;
        while (my ($async) = $scheduler->dequeue) {
            push @results, run_until_completion $async;
        }
        is "@results", "@values";
    };

    it q(handles complicated access pattern) => sub {
        my $scheduler = Async::Trampoline::Scheduler->new(2);

        for my $round (2**0 .. 2**8) {
            subtest qq(round $round) => sub {
                my @results;
                $scheduler->enqueue(async_value $_) for 0 .. $round - 1;
                for my $x ($round .. 2 * $round) {
                    $scheduler->enqueue(async_value $x);
                    push @results, run_until_completion $scheduler->dequeue;
                }
                while (my ($async) = $scheduler->dequeue) {
                    push @results, run_until_completion $async;
                }
                my @values = (0 .. 2 * $round);
                is "@results", "@values";
            };
        }
    };

    it q(discards dupes) => sub {
        my @values = (0 .. 4);
        my @async_values = map { async_value $_ } @values;
        my @asyncs;
        push @asyncs, @async_values[$_ .. 4] for @values;  # repeat objects
        is 0+@asyncs, (@values + 1 + 2 + 3 + 4), "precondition";

        my $scheduler = Async::Trampoline::Scheduler->new;
        $scheduler->enqueue($_) for @asyncs;

        my @result_asyncs;
        while (my ($async) = $scheduler->dequeue) {
            push @result_asyncs, $async;
        }

        is 0+@result_asyncs, 0+@values;

        my @results = map { run_until_completion $_ } @result_asyncs;

        is "@results", "@values";
    };

    it q(knows about task dependencies) => sub {
        my $scheduler = Async::Trampoline::Scheduler->new;

        my $starter = async_value 0;
        $scheduler->enqueue($starter => (async_value 1), (async_value 2));

        my $starter_again = $scheduler->dequeue;
        is $starter, $starter_again, q(got starter async back);
        is scalar $scheduler->dequeue, undef, q(queue has no further elems);
        run_until_completion $starter_again;
        $scheduler->complete($starter_again);

        my @results;
        while (my ($async) = $scheduler->dequeue) {
            push @results, run_until_completion $async;
        }
        @results = sort @results;

        is "@results", "1 2", q(got blocked tasks back);
    };

};

done_testing;
