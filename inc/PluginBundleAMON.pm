package inc::PluginBundleAMON;
use strict;
use warnings;
use utf8;

use Moose;

with 'Dist::Zilla::Role::PluginBundle::Easy';

sub mvp_multivalue_args {
    return qw/
        exclude_author_deps
        autogenerate_file
        gather_exclude_file
        gather_exclude_match
    /;
}

has github => (
    is => 'ro',
    isa => 'Str',
    lazy => 1,
    default => sub { delete shift->payload->{github} },
);

has github_issues => (
    is => 'ro',
    isa => 'Bool',
    lazy => 1,
    default => sub { delete shift->payload->{github_issues} // 1 },
);

has exclude_author_deps => (
    is => 'ro',
    isa => 'ArrayRef[Str]',
    lazy => 1,
    default => sub { delete shift->payload->{exclude_author_deps} // [] },
);

my @always_autogenerate = qw(
    LICENSE cpanfile Build.PL README.md
);

has autogenerate_file => (
    is => 'ro',
    isa => 'ArrayRef[Str]',
    lazy => 1,
    default => sub { delete shift->payload->{autogenerate_file} // [] },
);

has gather_exclude_file => (
    is => 'ro',
    isa => 'ArrayRef[Str]',
    lazy => 1,
    default => sub { delete shift->payload->{gather_exclude_file} // [] },
);

has gather_exclude_match => (
    is => 'ro',
    isa => 'ArrayRef[Str]',
    lazy => 1,
    default => sub { delete shift->payload->{gather_exclude_match} // [] },
);

has ppport => (
    is => 'ro',
    isa => 'Maybe[Str]',
    lazy => 1,
    default => sub { delete shift->payload->{ppport} },
);

sub configure {
    my ($self) = @_;

    ### Meta ###

    my ($gh_user, $gh_repo) = split m(/), $self->github;

    $self->add_plugins(
        [GithubMeta => {
            repo => $gh_repo,
            user => $gh_user,
            issues => $self->github_issues }],
    );

    ### Prereqs ###

    my @exclude_author_deps;
    push @exclude_author_deps, @{ $self->exclude_author_deps };

    my %plugin_prereqs = qw(
        GithubMeta          0
        Prereqs             0
        Prereqs::AuthorDeps 0
        Git::GatherDir      0
        PruneCruft          0
        CPANFile            0
        MetaYAML            0
        MetaJSON            0
        MetaProvides::Package 0
        Manifest            0
        License             0
        Readme              0
        PPPort              0
        Test::Perl::Critic  0
        PodSyntaxTests      0
        PodCoverageTests    0
        Test::Kwalitee::Extra 0
        RunExtraTests       0
        CopyFilesFromBuild  0
        ReadmeAnyFromPod    0
        Git::CheckFor::CorrectBranch 0
        Git::Check          0
        CheckChangesHasContent 0
        CheckVersionIncrement 0
        TestRelease         0
        ConfirmRelease      0
        UploadToCPAN        0
        NextRelease         0
        Git::Commit         0
        Git::Tag            0
        Git::Push           0
    );

    my %plugin_prereqs_resolved_name =
        map {; "Dist::Zilla::Plugin::$_" => $plugin_prereqs{$_} }
        keys %plugin_prereqs;

    $self->add_plugins(
        ['Prereqs' => 'Plugin Author Deps' => {
            -phase => 'develop',
            -relationship => 'requires',
            %plugin_prereqs_resolved_name }],
        ['Prereqs::AuthorDeps' => {
            (exclude => [ @exclude_author_deps ]) x!! @exclude_author_deps }],
    );

    ### Update Version ###

    $self->add_plugins(
        ['=inc::UpdateVersion' => { dir => ['lib'] }],
    );

    ### Gather Files ###

    my @exclude_file;
    push @exclude_file, @always_autogenerate;
    push @exclude_file, @{ $self->autogenerate_file },
    push @exclude_file, @{ $self->gather_exclude_file };
    push @exclude_file, $self->ppport if $self->ppport;

    my @exclude_match;
    push @exclude_match, @{ $self->gather_exclude_match };

    $self->add_plugins(
        ['Git::GatherDir' => {
            (exclude_filename   => [ @exclude_file  ]) x!! @exclude_file,
            (exclude_match      => [ @exclude_match ]) x!! @exclude_match }],
        'PruneCruft',
    );

    ### Generate Extra Files ###

    # also NextRelease, but that must be specified later
    $self->add_plugins(qw(
        CPANFile
        MetaYAML
        MetaJSON
        MetaProvides::Package
        Manifest
        License
        Readme
    ));

    if (my $ppport = $self->ppport) {
        $self->add_plugins(['PPPort' => { filename => $ppport }]);
    }

    ### Extra Tests ###

    $self->add_plugins(qw(
        Test::Perl::Critic
        PodSyntaxTests
        PodCoverageTests
        Test::Kwalitee::Extra
        RunExtraTests
    ));

    ### Copy Generated Files ###

    my @copy_from_build;
    push @copy_from_build, grep { $_ ne 'README.md' } @always_autogenerate;
    push @copy_from_build, @{ $self->autogenerate_file };
    push @copy_from_build, $self->ppport if $self->ppport;

    $self->add_plugins(
        ['CopyFilesFromBuild' => {
            copy => [ @copy_from_build ] }],
        ['ReadmeAnyFromPod' => {
            type => 'markdown',
            filename => 'README.md',
            location => 'root',
            phase => 'build' }],
    );

    ### Before Release ###

    $self->add_plugins(qw(
        Git::CheckFor::CorrectBranch
        Git::Check
        CheckChangesHasContent
        CheckVersionIncrement
        TestRelease
        ConfirmRelease
    ));

    ### Release ###

    $self->add_plugins(qw(
        UploadToCPAN
    ));

    ### After Release ###

    my $tag_format = 'release-%v';

    $self->add_plugins(
        ['NextRelease' => {
            time_zone => 'UTC' }],
        ['Git::Commit' => {
            commit_msg => $tag_format }],
        ['Git::Tag' => {
            tag_format => $tag_format,
            tag_message => $tag_format }],
        'Git::Push',
    );

}

__PACKAGE__->meta->make_immutable;

1;
