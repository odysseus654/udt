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


#
# Trace and NAM files
#
set nf [open out.nam w]
#$ns namtrace-all $nf

set tf [open out.tr w]
$ns trace-all $tf

set qf [open out.q w]

proc finish {} {
        global ns nf
        global ns tf

        $ns flush-trace

        close $nf
        close $tf

#        exec nam out.nam &
        exit 0
}


#
# Nodes
#
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

#
# Major Link, queue, and queue monitor
#
$ns duplex-link $na $nb 100Mb 5ms DropTail
$ns queue-limit $na $nb 10
$ns duplex-link-op $na $nb orient right
$ns duplex-link-op $na $nb color "green"
$ns duplex-link-op $na $nb queuePos 0.5
#$ns trace-queue $na $nb $qf
#$ns monitor-queue $na $nb $qf 0.1
#[$ns link $na $nb] start-tracing

#
# Error model
#
set em [new ErrorModel]
$em unit pkt
$em set rate_ 0.0000001
$em ranvar [new RandomVariable/Uniform]
$em drop-target [new Agent/Null]
#$ns link-lossmodel $em $na $nb

#
# Supportive link
#
$ns duplex-link $na0 $na1 100Mb 5ms RED
$ns queue-limit $na0 $na1 1000
$ns duplex-link-op $na0 $na1 orient 60deg

$ns duplex-link $na1 $na 200Mb 5ms DropTail
$ns queue-limit $na1 $na 1000
$ns duplex-link-op $na1 $na orient 360deg

$ns duplex-link $na $nb0 180Mb 5ms DropTail
$ns queue-limit $na $nb0 1000
$ns duplex-link-op $na $nb0 orient 0deg

$ns duplex-link $nb0 $nb1 100Mb 5ms RED
$ns queue-limit $nb0 $nb1 1000
$ns duplex-link-op $nb0 $nb1 orient 60deg


#
# functions for set up TCP, CBR, and UDT flows
#
source proc.tcl


#
# Testing cases
#
#build-tcp Sack $na $nb 1460 1000000000 0 0
build-udt $na $nb 1500 100000 1 0


#for {set i 100} {$i < 101} {incr i} {
#   build-on-off $na $nb 1500 1.0 2.0 100000k $i 0
#}

#for {set i 0} {$i < 10} {incr i} {
#   build-udt $na $nb 1500 10000 $i 0
#}

#for {set i 0} {$i < 10} {incr i} {
#   build-tcp Sack $na $nb 1460 1024000 $i 0
#}

#build-cbr $na $nb 25000k 0 20 30


#
# End here
#
$ns at 50 finish

$ns run
