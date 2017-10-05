package inc::UpdateVersion;
use strict;
use warnings;
use utf8;

use Moose;
use namespace::autoclean;

with qw(
    Dist::Zilla::Role::BeforeBuild
);

use File::Find ();

has dir => (
    is => 'ro',
    isa => 'ArrayRef[Str]',
    required => 1,
);

sub mvp_multivalue_args { qw( dir ) }

sub before_build {
    my ($self) = @_;

    my $version = $self->zilla->version;

    File::Find::find(sub {
        my $file = $_;

        return if not $file =~ /\A \w+\.pm \z/x;
        return if not -f $file;

        $self->_process_file($File::Find::name, $file, $version);

    }, @{ $self->dir });
}

sub _process_file {
    my ($self, $fullpath, $file, $version) = @_;

    my @contents = _slurp_text_lines($file);

    my $was_changed = 0;

    for (@contents) {
        next if not
            s{^ (?<lead> \h*) our \h* \$VERSION \h*
                = \h* '(?<oldversion>[0-9._]+)'
                ; \h* \# \h* VERSION \h* $}
            {$+{lead}our \$VERSION = '$version';  # VERSION}x;

        next if $+{oldversion} eq $version;

        $self->log("$fullpath: $+{oldversion} -> $version");
        $was_changed = 1;
    }

    if ($was_changed) {
        _spew_text($file => @contents);
    }
    else {
        $self->log_debug("$fullpath: no changes");
    }
}

sub _slurp_text_lines {
    my ($name) = @_;
    open my $fh, '<:encoding(UTF-8)', $name or die "Can't slurp $name: $!";
    return <$fh>;
}

sub _spew_text {
    my ($name, @contents) = @_;
    open my $fh, '>:encoding(UTF-8)', $name or die "Can't open $name: $!";
    print { $fh } @contents;
    close $fh or die "Can't close $name: $!";
    return;
}

__PACKAGE__->meta->make_immutable;

1;
