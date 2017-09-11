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

describe q(resolved_or()) => sub {
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

describe q(is_complete()) => sub {
    my $async = async { return async_value; };

    ok !$async->is_complete, "async thunk is not complete";

    $async->run_until_completion;

    ok $async->is_complete, "completed async is complete";

    ok +(async_cancel)->is_complete, "cancel is complete";
    ok +(async_error "error message")->is_complete, "error is complete";
    ok +(async_value)->is_complete, "value is complete";
};

describe q(is_resolved()) => sub {
    my $async = async { return async_value; };

    ok !$async->is_resolved, "async thunk is not resolved";

    $async->run_until_completion;

    ok $async->is_resolved, "completed async is resolved";

    ok !(async_cancel)->is_resolved, "cancel is not resolved";
    ok +(async_error "error message")->is_resolved, "error is resolved";
    ok +(async_value)->is_resolved, "value is resolved";
};

describe q(is_cancelled()) => sub {
    my $async = async { return async_cancel; };

    ok !$async->is_cancelled, "async thunk is not cancelled";

    eval { $async->run_until_completion };  # throws due to cancellation

    ok $async->is_cancelled, "completed async is cancelled";

    ok +(async_cancel)->is_cancelled, "cancel is cancelled";
    ok !(async_error "error message")->is_cancelled, "error is not cancelled";
    ok !(async_value)->is_cancelled, "value is not cancelled";
};

describe q(is_error()) => sub {
    my $async = async { return async_error "error message" };

    ok !$async->is_error, "async thunk is not error";

    eval { $async->run_until_completion };  # throws errors

    ok $async->is_error, "completed async is error";

    ok !(async_cancel)->is_error, "cancel is not error";
    ok +(async_error "error message")->is_error, "error is error";
    ok !(async_value)->is_error, "value is not error";
};

describe q(is_value()) => sub {
    my $async = async { return async_value };

    ok !$async->is_value, "async thunk is not value";

    $async->run_until_completion;

    ok $async->is_value, "completed async is value";

    ok !(async_cancel)->is_value, "cancel is not value";
    ok !(async_error "error message")->is_value, "error is not value";
    ok +(async_value)->is_value, "value is value";
};

describe q(errors) => sub {
    # TODO perhaps retain async_error call location,
    # instead of throwing from run_until_completion()?

    it q(can be thrown from callbacks) => sub {
        my $file = __FILE__;

        my ($l, $async) = (__LINE__, async { die "my little error" });

        throws_ok { $async->run_until_completion }
            qr/\Amy little error at \Q$file\E line $l\.$/;

        ok $async->is_error;
    };

    it q(can be returned explicitly) => sub {
        my $file = __FILE__;

        my $async = async { async_error "explicit error message" };

        my $l = __LINE__; throws_ok { $async->run_until_completion }
            qr/\Aexplicit error message at \Q$file\E line $l\.$/;

        ok $async->is_error;
    };
};

done_testing;
