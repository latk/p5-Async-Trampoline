use strict;
use warnings;
use utf8;

package Async::Trampoline::Scheduler;

=head1 NAME

Async::Trampoline::Scheduler - decide which thunk should be evaluated next

=head1 DESCRIPTION

This is an B<internal module>.
The interface may change at any time.

=head2 new

    $scheduler = Async::Trampoline::Scheduler->new

Create a new scheduler.

=cut

sub new {
    my $runnable = [];
    my $blocked = {};
    my $self = bless [$runnable, $blocked], __PACKAGE__;
    $self;
}

=head2 enqueue

    $scheduler->enqueue($task)

    $scheduler->enqueue($task => @blocked)

Add a I<$task> to the runnable queue.

B<$task>:
A task to be scheduled at some point in the future.

B<@blocked>:
Any number of tasks that depend on the I<$task>.
The blocked tasks will be added to the runnable queue
once the I<$task> is completed.

=cut

sub enqueue {
    my ($self, $async, @notify_on_completion) = @_;
    my ($runnable, $blocked) = @$self;

    push @{ $blocked->{0+$async} }, @notify_on_completion
        if @notify_on_completion;

    push @$runnable, $async;

    return;
}

=head2 dequeue

    ($task) = $scheduler->dequeue
    ()      = $scheduler->dequeue

Get the next scheduled I<$task>.

B<returns>:
A I<$task> if there is a runnable task.
An empty list if no tasks are runnable.

=cut

sub dequeue {
    my ($self) = @_;
    my ($runnable, $blocked) = @$self;

    return if not @$runnable;
    return shift @$runnable;
}

=head2 complete

    $scheduler->complete($task)

Mark a I<$task> as completed.

This may move further tasks to the runnable state.

B<$task>:
A task that was completed.

=cut

sub complete {
    my ($self, $async) = @_;
    my ($runnable, $blocked) = @$self;

    if (my $unblock = delete $blocked->{0+$async}) {
        push @$runnable, @$unblock;
    }

    return;
}

1;

