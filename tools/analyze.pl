# Loopgrind graphviz utility
use Data::Dumper;
use Graph::Directed;
use GraphViz;
use List::Util qw(reduce max);
use strict;

my $g = Graph::Directed->new;

my ($executable) = shift;
my ($file, $line);


# Get the object dump and read the source file.

my @source_file;
my @objdump_lines = `objdump -dl $executable`;


my %insts_to_code = ();

for my $l (@objdump_lines) {
    if ($l =~ /(\S+\.c):(\d+)/) {

        if ((not defined $file) or ($1 ne $file)) {
            print "Opening $1...\n";
            open SOURCE_FILE, $1 or die $!;
            @source_file = <SOURCE_FILE>;
        }
        $file = $1;
        $line = $2;
    }
    elsif (my ($address, $insts) = ($l =~ /([0-9a-f]+):\s*(.*)/)) {
#            print "Main loop header $address found at $file:$line\n";

        if (defined $line) {
            $insts_to_code{hex $address} = $source_file[$line - 1];
        } else {
            $insts_to_code{hex $address} = $insts;
        }
    }
}



# MAIN LOOP (lolol)
while (my $line = <>)
{
    next unless $line =~ /^EDGE/ or $line =~ /^NODE/ or $line =~ /^FNNAME/;

#    print $line;
    if (my ($address, $count) = ($line =~ /^NODE (0x[0-9a-f]+) \((\d+)\)/)) 
    {
        $g->add_vertex($address);
    }
    elsif (my ($curr, $next, $count) = ($line =~ /^EDGE (0x[0-9a-f]+) =\> (0x[0-9a-f]+) \((\d+)\)/))
    {
        $g->add_edge($curr, $next);
        $g->set_edge_weight($curr, $next, ($count / 1000));
    }
#    elsif (my ($address, $fnname) = ($line =~ /^FNNAME (0x[0-9a-f]+) (\w+)/))
#    {
#        print $fnname, "\n";
#        $g_viz->add_node($address, label => "$address\\n($fnname)");
#    }

}





# Do some edge contraction on the graph: remove self loops and merge articulation points
for my $v ($g->vertices)
{
    if ($g->has_edge($v, $v))
    {
        print "Note: self-loop $v => $v (", $g->get_edge_weight($v, $v), ")\n";
#        $g->delete_edge($v, $v);
    }
}
for my $v ($g->vertices)
{
    my @edges_to = $g->edges_to($v);
    my @edges_from = $g->edges_from($v);


    if ((scalar @edges_to) == 1 and (scalar @edges_from) == 1)
    {
        my ($u, $w) = ($edges_to[0]->[0], $edges_from[0]->[1]);

        $g->add_edge($u, $w);
        $g->set_edge_weight($u, $w,  
                ($g->get_edge_weight($u, $w) or 0) + 
                $g->get_edge_weight($v, $w));

        $g->delete_edge($u, $v);
        $g->delete_edge($v, $w);
        $g->delete_vertex($v);

        print "Merging $u => $v => $w\n";
        print "\t(new weight: ", $g->get_edge_weight($u, $w), ")\n";
    }

}






# Build up the graphviz graph
my $g_viz = GraphViz->new(directed => 1, ratio => 'expand');
print "\nBuilding graph for displaying\n";
for my $v ($g->vertices)
{
    print ".";

    my $source_line = $insts_to_code{hex $v};
    $source_line =~ s/^\s+//;

    $g_viz->add_node($v, label => "$v\n" . $source_line);

    my @edges_to = $g->edges_to($v);
    my @edges_from = $g->edges_from($v);

    for my $e ($g->edges_from($v))
    {
        my ($u, $w) = @{$e};

        $g_viz->add_edge($u, $w, label => $g->get_edge_weight($u, $w));
    }
}
print "\n";


#print "$g\n";






# HERE BE DRAGONS


sub sum_in_weights {
    my ($vertex) = @_;

    my $total_weights = 0;

    my @edges = $g->edges_to($vertex);

    map { my ($u, $v) = @{$_}; $total_weights += $g->get_edge_weight($u, $v) } @edges;

    $total_weights;

}


# Get the first element of the largest scc
my $root = ($g->source_vertices())[0];


my @vertices = sort { sum_in_weights($b) <=> sum_in_weights($a) } $g->vertices;

my $max_in_weight = &sum_in_weights($vertices[0]);
print "Max in-weight:  $max_in_weight (addr: ", $vertices[0], ")\n";


#vert_pos_diff[i] = distance from vertex [i-1] to [i].  We want to pull out highly weighted
#but far apart instructions.
my @vert_pos_diffs = map { (hex $vertices[$_ - 1]) - (hex $vertices[$_]) } 1..$#vertices;

#From the top quartile of vertices, grab the ones that are sufficiently
#"different" (in a address locality sense)".
my @verts_of_max_degree = ($vertices[0]);
for my $i (1..$#vert_pos_diffs / 4)  {
    next if $vert_pos_diffs[$i] < 0x100;
    push @verts_of_max_degree, $vertices[$i];
}

my @path_lens = map { scalar($g->path_vertices($root, $_)) } @verts_of_max_degree;

print "-----------------------------------\n";
print "Top-ranked SBs: ";
print join(" ", @verts_of_max_degree), "\n";

print "Dist from main: ";
print join(" "x 10, @path_lens), "\n";

my @sorted_indexes = sort {$path_lens[$a] <=> $path_lens[$b]} 0..$#path_lens;
my $min_i = $sorted_indexes[0];


print "Outputting graph...\n";


$g_viz->add_node($verts_of_max_degree[$min_i], 
        label => $verts_of_max_degree[$min_i]."\n".$insts_to_code{hex $verts_of_max_degree[$min_i]}, 
        style => 'bold',
        shape => 'hexagon',
        );
$g_viz->as_png("graph.png");



