#!/usr/bin/perl
# 538W Superblock visualization tool
# This file is a part of a submission for a course project in
# CPSC 538W, Execution Mining, at UBC, Winter 2010.
#
# Copyright (c) 2010 Nathan Taylor <tnathan@cs.ubc.ca>
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
# 
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

use strict;
use warnings;

use GraphViz;
use Gtk2::Ex::GraphViz;
use Glib qw/TRUE FALSE/;
use Gtk2 '-init';

my $g = GraphViz->new(directed => 1, ratio => 'compress');
while (my $line = <>)
{
    next unless $line =~ /^EDGE/ or $line =~ /^NODE/ or $line =~ /^FNNAME/;

#    print $line;
    if (my ($address, $count) = ($line =~ /^NODE (0x[0-9a-f]+) \((\d+)\)/)) 
    {
        $g->add_node($address, label => $address);
    }
    elsif (my ($curr, $next, $count) = ($line =~ /^EDGE (0x[0-9a-f]+) =\> (0x[0-9a-f]+) \((\d+)\)/))
    {
        $g->add_edge($curr, $next, label => $count);
    }
    elsif (my ($address, $fnname) = ($line =~ /^FNNAME (0x[0-9a-f]+) (\w+)/))
    {
        print $fnname, "\n";
        $g->add_node($address, label => "$address\\n($fnname)");
    }

}


#------------------------- PerlTK widget init ---------------------------

#my $frame_source_browsers = 
#    $frame_main->Frame()->pack( -side=>'left',
#                                -fill=>'y',
#                                -pady=>9,
#                                -padx=>8);
#my $frame_graph =
#    $frame_main->Frame()->pack( -side=>'right',
#                                -fill=>'y',
#                                -pady=>9,
#                                -padx=>8);

my $graphviz = Gtk2::Ex::GraphViz->new($g);

$graphviz->signal_connect ('mouse-enter-node' => 
    sub {
            my ($self, $x, $y, $nodename) = @_;
            my $nodetitle = $graphviz->{svgdata}->{g}->{g}->{$nodename}->{title};
            print "Node : $nodetitle : $x, $y\n";
        }
    );


# create a new window
my $window = Gtk2::Window->new('toplevel');

# When the window is given the "delete_event" signal (this is given
# by the window manager, usually by the "close" option, or on the
# titlebar), we ask it to call the delete_event () functio
# as defined above. No data is passed to the callback function.
$window->signal_connect(delete_event => \&delete_event);

# Here we connect the "destroy" event to a signal handler.
# This event occurs when we call Gtk2::Widget::destroy on the window,
# or if we return FALSE in the "delete_event" callback. Perl supports
# anonymous subs, so we can use one of them for one line callbacks.
$window->signal_connect(destroy => sub { Gtk2->main_quit; });

# Sets the border width of the window.
$window->set_border_width(10);

$window->add($graphviz->get_widget());

$window->show();

Gtk2->main;

