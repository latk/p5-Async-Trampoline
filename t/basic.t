#!/usr/bin/env perl

use strict;
use warnings;
use utf8;

use FindBin;
use lib "$FindBin::Bin/lib";

use Async::Trampoline::Describe qw(describe it);
use Test::More;

use Async::Trampoline qw(
    await
    async
    async_value
);

describe q(Async::Trampoline) => sub {
    ok 1;  # TODO write actual tests
};

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

done_testing;
