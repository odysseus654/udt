#Create a simulator object
set ns [new Simulator]

Application/AppSd set pktsize_ 1500
Application/AppSd set interval_ 0.00001
Application/AppSd set maxlosslen_ 366
Application/AppSd set threshold_ 0.01
Application/AppSd set ackinterval_ 0.005
Application/AppSd set errinterval_ 0.001
Application/AppSd set coninterval_ 0.0075
Application/AppSd set syninterval_ 0.01
Application/AppSd set expinterval_ 1
Application/AppSd set sendflagsize_ 100000
Application/AppSd set recvflagsize_ 100000
Application/AppSd set rtt_ 0.2

#Define different colors for data flows (for NAM)
$ns color 1 Blue
$ns color 2 Red

#Open the NAM trace file
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

#Create four nodes
set n0 [$ns node]
set n1 [$ns node]

#Create links between the nodes
$ns duplex-link $n0 $n1 1000Mb 100ms RED
#$ns queue-limit $n0 $n1 1000


#Give node position (for NAM)
$ns duplex-link-op $n0 $n1 orient right

#Monitor the queue for link (n0-n1). (for NAM)
$ns duplex-link-op $n0 $n1 queuePos 0.5

#Setup a UDP SD(Sabul Data) connection
set udp_s [new Agent/UdpSd]
set udp_r [new Agent/UdpSd]
$ns attach-agent $n0 $udp_s
$ns attach-agent $n1 $udp_r
$ns connect $udp_s $udp_r
$udp_s set packetSize_ 1500
$udp_s set fid_ 1
$udp_r set packetSize_ 1500
$udp_r set fid_ 1

#Setup a Sabul Data application
set app_sd_s [new Application/AppSd]
set app_sd_r [new Application/AppSd]
$app_sd_s attach-agent $udp_s
$app_sd_r attach-agent $udp_r

#Setup a TCP connection
set tcp1 [new Agent/TCP/FullTcp]
set tcp2 [new Agent/TCP/FullTcp]
$tcp1 set nodelay_ true
$tcp2 set nodelay_ true
$ns attach-agent $n0 $tcp1
$ns attach-agent $n1 $tcp2
$ns connect $tcp1 $tcp2
$tcp1 set fid_ 2
$tcp2 set fid_ 2
$tcp1 listen

#Setup a Sabul Control Application
set app1 [new Application/AppSc $tcp1]
set app2 [new Application/AppSc $tcp2]
$app2 connect $app1

#Complete SD/SC connection
$udp_s set-status server
$app_sd_s set-status server
$app1 set-status server

$app_sd_s set-target $app1
$app1 set-target $app_sd_s
$app_sd_r set-target $app2
$app2 set-target $app_sd_r

#Setup SABUL transmission parameters
#$udp_s set udpsd_hdr_size_ 2
#$app_sd_s set pktsize_ 1500

#Do initialization
$ns at 0 "$app_sd_s initialize"
$ns at 0 "$app_sd_r initialize"

#Schedule events for the SD and SC applications
$ns at 0.05 "$app_sd_s start"
$ns at 0.05 "$app_sd_r start"
$ns at 50.05 "$app_sd_s close"
$ns at 50.05 "$app_sd_r close"

#Call the finish procedure after XX seconds of simulation time
$ns at 52.05  "finish"

#Run the simulation
$ns run
