#!/usr/bin/env perl

use strict;
use warnings;
use utf8;

use FindBin;
use lib "$FindBin::Bin/lib";
use Async::Trampoline ':all';

use Async::Trampoline::Describe qw(describe it);
use Test::More;
use Test::Exception;

BEGIN {
    plan skip_all => "Parser tests require Perl v5.20 or better"
        if $^V lt v5.20;
    plan skip_all => "Parser tests require namespace::autoclean"
        unless eval { require namespace::autoclean; 1 };
    plan skip_all =>
        "Parser tests require Async::Trampoline::Example::Interpreter, "
        . "which is only provided by the Dist::Zilla-based build process"
        unless eval { require Async::Trampoline::Example::Interpreter };
}

use Async::Trampoline::Example::Interpreter;

use experimental 'signatures';
## no critic (SubroutinePrototypes)

sub run_parser_test($name, $source, $result, %args) {
    my $env = delete $args{env} // {};
    my $ast = delete $args{ast};
    my $method = delete $args{method} // 'parse_toplevel';
    if (my @keys = sort keys %args) {
        die "Unknown args: @keys";
    }

    subtest $name => sub {
        my $async_ast = Parser->new(\$source)->$method->get_result;
        my $interpreter = Async::Trampoline::Example::Interpreter->new(%$env);
        my $async_result = await $async_ast => sub ($ast) {
            return $interpreter->eval($ast);
        };
        is_deeply scalar $async_result->run_until_completion, $result;

        is_deeply scalar $async_ast->run_until_completion, $ast
            if defined $ast;
    };
}

run_parser_test q(multiplication),
    'foo * 7 / baz' => 10.5,
    method => 'parse_expr',
    ast => [DIV => [MUL => [VAR => 'foo'], [LIT => 7]], [VAR => 'baz']],
    env => { foo => 3, baz => 2 };

run_parser_test q(addition),
    'a + b * (c - -3)' => 7,
    method => 'parse_expr',
    ast => [ADD =>
        [VAR => 'a'],
        [MUL =>
            [VAR => 'b'],
            [SUB => [VAR => 'c'], [LIT => -3]],
        ],
    ],
    env => { a => 1, b => 3, c => -1 };

run_parser_test q(calls),
    'x * f y * g (5 + b)' => 30,
    method => 'parse_expr',
    ast => [MUL =>
        [MUL => [VAR => 'x'], [CALL => [VAR => 'f'], [VAR => 'y']]],
        [CALL => [VAR => 'g'], [ADD => [LIT => 5], [VAR => 'b']]],
    ],
    env => {
        x => 2, y => 3, b => 2,
        f => sub ($x) { $x },
        g => sub ($x) { $x - 2 },
    };

run_parser_test q(statements),
    'x; y - z; f a' => 3,
    ast => [DO =>
        [VAR => 'x'],
        [SUB => [VAR => 'y'], [VAR => 'z']],
        [CALL => [VAR => 'f'], [VAR => 'a']],
    ],
    env => {
        x => 13, y => 8, z => 7, a => 9,
        f => sub ($x) { sqrt $x },
    };

run_parser_test q(let-statement),
    'let x = 4; x * 3' => 12,
    ast => [DO =>
        [LET => 'x', [LIT => 4]],
        [MUL => [VAR => 'x'], [LIT => 3]],
    ];

done_testing;

BEGIN {
    package BasicParser;

    use namespace::autoclean;
    use Async::Trampoline ':all';
    use Carp;

    sub _wrap($class, $async) { bless \$async => (ref $class or $class) }
    sub _unwrap($self) { $$self }

    sub new($class, $str_ref, $pos=0) {
        my $cache = [];
        return $class->_wrap(async_value $str_ref, $pos, $cache);
    }

    sub then($self, $callback) {
        return $self->_wrap($$self->await($callback));
    }

    sub then_cached($self, $name, $callback) {
        return $self->_wrap($$self->await(sub ($, $pos, $cache, @) {
            return $cache->[$pos]{$name} //= $callback->(@_);
        }));
    }

    sub do ($self, $callback, @args) :method {
        return $self->then(sub { $self->$callback(@args)->_unwrap });
    }

    sub do_cached($self, $name, $callback, @args) {
        return $self->then_cached($name => sub {
            return $self->$callback(@args)->_unwrap;
        });
    }

    sub make_value($self, $callback) {
        return $self->then(sub ($str_ref, $pos, $cache, @args) {
            return async_value $str_ref, $pos, $cache, $callback->(@args);
        });
    }

    sub make_error($self, $callback) {
        return $self->then(sub ($str_ref, $pos, $cache, @) {
            my @lines = split /(?<=$)/, $$str_ref;
            my $start = 0;
            $start += length shift @lines
                while @lines and $pos > $start + length $lines[0];

            my ($error, $details) = $callback->();

            my $message = sprintf qq(error: %s\n  | %s\n  | %*s),
                $error,
                $lines[0] // '(unknown line)',
                $pos - $start + 1, '^';
            $message .= "\n$details" if length $details;
            $message .= "\n ";
            return async_error $message;
        });
    }

    sub capture($self, @refs) {
        return $self->then(sub {
            my $str_ref = shift;
            my $pos = shift;
            my $cache = shift;

            for (@refs) {
                if (ref eq 'ARRAY') {
                    @$_ = splice @_;
                }
                elsif (not defined) {
                    shift;
                }
                elsif (ref eq 'SCALAR') {
                    $$_ = shift;
                }
                else {
                    croak q(Unknown assignment target: )
                        . q(want undef, scalar ref, or array ref);
                }
            }

            return $$self;
        });
    }

    sub match($self, $pattern, @captures) {
        return $self->then(sub ($str_ref, $pos, $cache, @) {
            pos($$str_ref) = $pos;
            # warn qq(Matching /$pattern/ at $pos\n);

            $$str_ref =~ /\G$pattern/gc
                or return $self->make_error(sub {
                    qq(pattern failed to match: $pattern),
                })->_unwrap;
            my $newpos = pos($$str_ref);

            # warn qq(  --> "$&"\n);

            return async_value $str_ref, $newpos, $cache, @+{@captures};
        });
    }

    sub try($self) {
        return $self->_wrap($$self->value_or(async_cancel));
    }

    sub get_result($self) {
        return $self->_unwrap->await(sub ($str_ref, $pos, $cache, $ast) {
            if ($pos < length $$str_ref) {
                return $self->make_error(sub { "expected EOF here" })->_unwrap;
            }
            return async_value $ast;
        });
    }

    sub production($self, @production) {
        my $action = pop @production;
        my @saves;

        for (@production) {
            if ($_ eq '::') {
                $self = $self->try;
                next;
            }

            if ($_ eq '$prev') {
                push @saves, undef;
                $self = $self->capture(\$saves[-1]);
                next;
            }

            my $save_this = s/^\$//;

            if (/^<(\w+)>$/) {
                $self = $self->do("parse_$1");
            }
            elsif (/^op:(.+)$/) {
                $self = $self->parse_operator($1);
            }
            elsif (/^kw:(\w+)$/) {
                $self = $self->parse_keyword($1);
            }
            else {
                croak qq(Unknown grammar spec: $_);
            }

            if ($save_this) {
                push @saves, undef;
                $self = $self->capture(\$saves[-1]);
            }
        }

        return $self->make_value(sub { $action->(@saves) });
    }
}

BEGIN {
    package Parser;
    use parent -norequire, 'BasicParser';

    use namespace::autoclean;
    use Async::Trampoline ':all';
    use Carp;

    use overload '|' => sub ($lhs, $rhs, $swap) {
        ($lhs, $rhs) = ($rhs, $lhs) if $swap;
        return $lhs->_wrap($$lhs->resolved_or($$rhs));
    };

    sub parse_toplevel($self) {
        return $self->do_cached("<toplevel>" => sub {
            my $cont = $self->production(
                qw[ $<statement> op:; :: $<toplevel> ],
                sub ($stmt, $rest) {
                    my (undef, @rest) = @$rest;
                    return [DO => $stmt, @rest];
                },
            );

            my $stmt = $self->production(
                qw[ $<statement> ],
                sub ($stmt) { [DO => $stmt] },
            );

            return $cont | $stmt;
        });
    }

    sub parse_statement($self) {
        return $self->do_cached("<statement>" => sub {
            my $let = $self->production(
                qw[ kw:let :: $<ident> op:= $<expr> ],
                sub ($var, $expr) { [LET => $var, $expr ] },
            );

            return $let | $self->parse_expr;
        });
    }

    sub parse_expr($self) {
        return $self->do_cached("<expr>", 'parse_term');
    }

    sub parse_operator($self, $op) {
        return $self->do_cached("op:$op" => match => qr/\s*\Q$op\E\s*/);
    }

    sub parse_keyword($self, $kw) {
        return $self->do_cached("kw:$kw" => match => qr/\s*\b\Q$kw\E\b\s*/);
    }

    sub parse_ident($self) {
        return $self->do_cached("<ident>" => sub {
            return $self
                ->match(qr/\s*\b(?<var>(?!\d)\w+)\b\s*/, qw(var));
        });
    }

    sub parse_variable($self) {
        return $self->do_cached("<variable>" => sub {
            return $self
                ->parse_ident
                ->make_value(sub ($name) { [VAR => $name] });
        });
    }

    sub parse_literal($self) {
        return $self->do_cached("<literal>" => sub {
            return $self
                ->match(qr/\s*(?<value>-?[0-9]+)\b\s*/, qw(value))
                ->make_value(sub ($value) { [LIT => $value] });
        });
    }

    sub parse_atom($self) {
        return $self->do_cached("<atom>" => sub {
            my $parens = $self->production(
                qw[ op:( :: $<expr> op:) ],
                sub ($expr) { $expr },
            );

            my $call = $self->production(
                qw[ $<variable> $<atom> :: ],
                sub ($func, $arg) { [CALL => $func, $arg] },
            );

            return( $parens
                |   $call
                |   $self->parse_literal->try
                |   $self->parse_variable
            );
        });
    }

    sub parse_factor($self) {
        return $self->do_cached("<factor>" => sub {
            return $self->parse_atom->parse_factor_rest;
        });
    }

    sub parse_factor_rest($self) {
        my $mult = $self->production(
            qw[ $prev op:* :: $<atom> ],
            sub ($lhs, $rhs) { [MUL => $lhs, $rhs] },
        );

        my $div = $self->production(
            qw[ $prev op:/ :: $<atom> ],
            sub ($lhs, $rhs) { [DIV => $lhs, $rhs] },
        );

        return ($mult | $div)->do(sub { shift->parse_factor_rest }) | $self;
    }

    sub parse_term($self) {
        return $self->do_cached("<term>" => sub {
            return $self->parse_factor->parse_term_rest;
        });
    }

    sub parse_term_rest($self) {
        my $add = $self->production(
            qw[ $prev op:+ :: $<factor> ],
            sub ($lhs, $rhs) { [ADD => $lhs, $rhs] },
        );

        my $sub = $self->production(
            qw[ $prev op:- :: $<factor> ],
            sub ($lhs, $rhs) { [SUB => $lhs, $rhs] },
        );

        return ($add | $sub)->do(sub { shift->parse_term_rest }) | $self;
    }
}
