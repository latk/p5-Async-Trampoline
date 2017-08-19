package inc::ModuleBuildXS;
use strict;
use warnings;

use Moose;
extends 'Dist::Zilla::Plugin::ModuleBuild';

around module_build_args => sub {
    my $orig = shift;
    my ($self, @args) = @_;
    my $mb_args = $orig->($self, @args);
    $mb_args->{c_source} = 'src';
    return $mb_args;
};

__PACKAGE__->meta->make_immutable;
1;

__END__
