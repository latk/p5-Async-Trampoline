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

sub count_down_generator {
    my ($i) = @_;
    return async_cancel unless $i >= 0;
    return async_yield async_value($i) => sub {
        return count_down_generator($i - 1);
    };
}

subtest q(basic functionality) => sub {
    my $size = 35;
    my $gen =
        count_down_generator($size)
        ->gen_map(sub { async_value "<@_>" })
        ->gen_collect;
    my @items = $gen->run_until_completion;
    is 0+@items, 1, "got exactly one return value";
    my ($acc) = @items;
    ok $acc, "got a return value";
    is_deeply $acc, [map { "<$_>" } reverse 0..$size];
};

subtest q(launch sequence) => sub {
    my $async =
        count_down_generator(10)
        ->gen_map(sub {
            my ($i) = @_;
            return async_value "ignition" if $i == 3;
            return async_value "liftoff"  if $i == 0;
            return async_value $i;
        })
        ->gen_collect;
    is_deeply $async->run_until_completion,
        [10, 9, 8, 7, 6, 5, 4, 'ignition', 2, 1, 'liftoff'];
};

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

subtest q(repetition) => sub {
    my $async = repeat_gen(count_down_generator(3))->gen_collect;
    is_deeply $async->run_until_completion, [3, 3, 2, 2, 1, 1, 0, 0];
};

done_testing;
