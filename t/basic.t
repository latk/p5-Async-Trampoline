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

done_testing;
