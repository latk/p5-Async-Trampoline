#!/usr/bin/env perl

use strict;
use warnings;
use utf8;

use FindBin;
use lib "$FindBin::Bin/lib";

use Async::Trampoline::Describe qw(describe it);
use Test::More;

use Async::Trampoline;

describe q(Async::Trampoline) => sub {
    ok 1;  # TODO write actual tests
};

done_testing;
