set ns [new Simulator]

$ns color 0 red
$ns color 1 blue
$ns color 2 chocolate
$ns color 3 red
$ns color 4 brown
$ns color 5 tan
$ns color 6 gold
$ns color 7 black

set nf [open out.nam w]
$ns namtrace-all $nf

#Define a 'finish' procedure
proc finish {} {
        global ns nf
        global app_sd_s

        $ns flush-trace

        #Close the NAM trace file
        close $nf

        #Execute NAM on the trace file
        exec nam out.nam &
        exit 0
}

set na [$ns node]
$na color "red"
set nb [$ns node]
$nb color "red"
set na0 [$ns node]
$na0 color "blue"
set na1 [$ns node]
$na1 color "blue"
set nb0 [$ns node]
$nb0 color "tan"
set nb1 [$ns node]
$nb1 color "tan"

$ns duplex-link $na $nb 1000Mb 100ms RED
$ns queue-limit $na $nb 50
$ns duplex-link-op $na $nb orient right
$ns duplex-link-op $na $nb color "green"

$ns duplex-link $na0 $na 1000Mb 10ms RED
$ns queue-limit $na0 $na 10
$ns duplex-link-op $na0 $na orient 60deg

$ns duplex-link $na1 $na 1000Mb 10ms RED
$ns queue-limit $na1 $na 10
$ns duplex-link-op $na1 $na orient 300deg

$ns duplex-link $nb $nb0 1000Mb 10ms RED
$ns queue-limit $nb $nb0 10
$ns duplex-link-op $nb $nb0 orient 300deg

$ns duplex-link $nb $nb1 1000Mb 10ms RED
$ns queue-limit $nb $nb1 10
$ns duplex-link-op $nb $nb1 orient 60deg


$ns run
