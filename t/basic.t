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
    # async $async => \&f   =   async >>= f

    my $f = sub {
        my ($x) = @_;
        return async_value "f($x)";
    };

    my $g = sub {
        my ($x) = @_;
        return async_value "g($x)";
    };

    it q(satisfies left identity: return a >>= f === f a) => sub {
        my $return_a_bind_f = async async_value("a") => \&$f;
        my $f_a = $f->("a");

        is await($return_a_bind_f), await($f_a);
        is await($f_a), 'f(a)';
    };

    it q(satisfies right identity: m >>= return === m) => sub {
        my $id = [];
        my $m = async_value $id;
        my $m_bind_return = async $m => \&async_value;

        is await($m), await($m_bind_return);
        is await($m), "$id";
    };

    it q(satisfies associativity: (m >>= f) >>= g === m >>= (x -> f x >>= g)) => sub {
        my $id = [];
        my $m = async_value $id;
        my $m_bind_f_bind_g = async(async($m => \&$f) => \&$g);
        my $m_bind_x_f_x_bind_g = async $m => sub {
            my ($x) = @_;
            return async $f->($x) => \&$g;
        };

        is await($m_bind_f_bind_g), await($m_bind_x_f_x_bind_g);
        is await($m_bind_f_bind_g), "g(f($id))";
    };
};

describe q(async_else) => sub {
    it q(returns the first value) => sub {
        my $async = async_else(
            async_value(42),
            async_cancel,
        );
        is await($async), 42;
    };

    it q(skips cancelled values) => sub {
        my $async = async_else(
            async_cancel,
            async_value("foo"),
        );
        is await($async), "foo";
    };

    it q(can evaluate thunks) => sub {
        my $async = async_else(
            deferred { async_cancel },
            async_value("bar"),
        );
        is await($async), "bar";
    };

    it q(dies if no uncancelled values exist) => sub {
        my $async = async_else
            async_cancel,
            async_cancel;

        throws_ok { await($async) }
            qr/^async_else:\ found\ no\ values/x;
    };
};

sub _loop {
    my ($items, $i) = @_;
    return async_value $items if not $i;
    push @$items, $i--;
    return deferred { _loop($items, $i) };
}

describe q(deferred) => sub {
    it q(can defer function calls) => sub {
        my $values = await _loop([], 5);
        is "@$values", "5 4 3 2 1";
    };
};

done_testing;
