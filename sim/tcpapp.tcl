set ns [new Simulator]

$ns color 0 red
$ns color 1 blue
$ns color 2 chocolate
$ns color 3 green
$ns color 4 brown
$ns color 5 tan
$ns color 6 gold
$ns color 7 black
$ns color 8 yellow
$ns color 9 purple


set nf [open out.nam w]
#$ns namtrace-all $nf

set tf [open out.tr w]
$ns trace-all $tf

#Define a 'finish' procedure
proc finish {} {
        global ns nf
        global ns tf

        $ns flush-trace

        close $nf
        close $tf

#        exec nam out.nam &
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

$ns duplex-link $na $nb 100Mb 5ms RED
$ns queue-limit $na $nb 1000
$ns duplex-link-op $na $nb orient right
$ns duplex-link-op $na $nb color "green"
$ns duplex-link-op $na $nb queuePos 0.5

$ns duplex-link $na0 $na 1000Mb 100ms DropTail
$ns queue-limit $na0 $na 1000
$ns duplex-link-op $na0 $na orient 60deg

$ns duplex-link $na1 $na 1000Mb 10ms DropTail
$ns queue-limit $na1 $na 1000
$ns duplex-link-op $na1 $na orient 300deg

$ns duplex-link $nb $nb0 1000Mb 10ms DropTail
$ns queue-limit $nb $nb0 1000
$ns duplex-link-op $nb $nb0 orient 300deg

$ns duplex-link $nb $nb1 1000Mb 10ms DropTail
$ns queue-limit $nb $nb1 1000
$ns duplex-link-op $nb $nb1 orient 60deg

source proc.tcl
#build-tcp Sack $na $nb 1460 1024000 0 0
#build-tcp Sack $na $nb 1460 1024000 2 0
build-sabul $na $nb 0.01 0.01 0 0
build-sabul $na $nb 0.01 0.0002 1 0

#build-sabul $na0 $nb0 0.22 0.00002 1 0
#build-sabul $na1 $nb0 0.04 0.00002 2 0
#build-sabul $na1 $nb0 0.04 0.004 3 0


#for {set i 0} {$i < 100} {incr i} {
#   build-on-off $na $nb 1500 1.0 2.0 10000k $i 0
#}

#proc build-on-off { src dest pktSize burstTime idleTime rate id startTime }


#for {set i 10} {$i < 20} {incr i} {
#   build-sabul $na $nb 0.2 0.002 $i 0
#}

#for {set i 0} {$i < 10} {incr i} {
#   build-tcp Sack $na $nb 1460 1024000 $i 0
#}

$ns at 50 finish

$ns run
