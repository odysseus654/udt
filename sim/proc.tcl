proc build-ftp { type src dest pktSize window class numPackets startTime } {
    global ns

    # build tcp source
    if { $type == "TCP" } {
      set tcp [new Agent/TCP]
      set snk [new Agent/TCPSink]
    } elseif { $type == "Reno" } {
      set tcp [new Agent/TCP/Reno]
      set snk [new Agent/TCPSink]
    } elseif { $type == "Sack" } {
      set tcp [new Agent/TCP/Sack1]
      set snk [new Agent/TCPSink/Sack1]
    } elseif  { $type == "Newreno" } {
      set tcp [new Agent/TCP/Newreno]
      set snk [new Agent/TCPSink]
    } else {
      puts "ERROR: Inavlid tcp type"
    }
    $ns attach-agent $src $tcp

    #build tcp sink
    $ns attach-agent $dest $snk

    # connect source to sink
    $ns connect $tcp $snk

    # init. tcp parameters
    if { $pktSize > 0 } {
      $tcp set packetSize_ $pktSize
    }
    $tcp set class_ $class
    if { $window > 0 } {
      $tcp set window_ $window
    } else {
      # default in ns-2 version 2.0 
      $tcp set window_ 20
    }
    $tcp set max_pkt_ $numPackets
   
    set ftp [new Source/FTP]
    $ftp set agent_ $tcp

    $ns at $startTime "$ftp start"
}

proc build-tcp { type src dest pktSize window class startTime } {
    global ns

    # build tcp source
    if { $type == "TCP" } {
      set tcp [new Agent/TCP]
      set snk [new Agent/TCPSink]
    } elseif { $type == "Reno" } {
      set tcp [new Agent/TCP/Reno]
      set snk [new Agent/TCPSink]
    } elseif { $type == "Sack" } {
      set tcp [new Agent/TCP/Sack1]
      set snk [new Agent/TCPSink/Sack1]
    } elseif  { $type == "Newreno" } {
      set tcp [new Agent/TCP/Newreno]
      set snk [new Agent/TCPSink]
    } else {
      puts "ERROR: Inavlid tcp type"
    }
    $ns attach-agent $src $tcp

    #$tcp set tcpTick_ 0.01
    #build tcp sink
    $ns attach-agent $dest $snk

    # connect source to sink
    $ns connect $tcp $snk

    # init. tcp parameters
    if { $pktSize > 0 } {
      $tcp set packetSize_ $pktSize
    }
    $tcp set class_ $class
    if { $window > 0 } {
      $tcp set window_ $window
    } else {
      # default in ns-2 version 2.0
      $tcp set window_ 20
    }

    set ftp [new Source/FTP]
    $ftp set agent_ $tcp
    $ns at $startTime "$ftp start"

    return $tcp
}


proc build-udp { src dest pktSize interval random id startTime } {
    global ns

    # build udp source
    set udp [new Agent/CBR]
    $ns attach-agent $src $udp

    #build cbr sink
    set null [new Agent/Null]
    $ns attach-agent $dest $null

    #connect cbr sink to cbr null
    $ns connect $udp $null

    # init. cbr parameters
    if {$pktSize > 0} {  
        $udp set packetSize_ $pktSize
    }
    $udp set fid_      $id
    $udp set interval_ $interval
    $udp set random_   $random
    $ns at $startTime "$udp start"

    return $udp
}

proc build-cbr { src dest pktSize interval random id startTime } {
    global ns

    # build cbr source
    set cbr [new Agent/CBR]
    $ns attach-agent $src $cbr

    #build cbr sink
    set null [new Agent/Null]
    $ns attach-agent $dest $null

    #connect cbr sink to cbr null
    $ns connect $cbr $null

    # init. cbr parameters
    if {$pktSize > 0} {  
        $cbr set packetSize_ $pktSize
    }
    $cbr set fid_      $id
    $cbr set interval_ $interval
    $cbr set random_   $random
    $ns at $startTime "$cbr start"

    return $cbr
}

proc build-on-off { src dest pktSize burstTime idleTime rate id startTime } {
    global ns

    set cbr [new Agent/CBR/UDP]
    $ns attach-agent $src $cbr

    set null [new Agent/Null]
    $ns attach-agent $dest $null

    $ns connect $cbr $null

    set exp1 [new Traffic/Expoo]
    $exp1 set packet-size $pktSize
    $exp1 set burst-time  $burstTime
    $exp1 set idle-time   $idleTime
    $exp1 set rate        $rate
    $cbr  attach-traffic $exp1

    $ns at $startTime "$cbr start"
    $cbr set fid_      $id
    return $cbr
}

proc build-sabul { src dest rtt initrate id startTime } {
    global ns

    set udp_s [new Agent/UdpSd]
    set udp_r [new Agent/UdpSd]
    $ns attach-agent $src $udp_s
    $ns attach-agent $dest $udp_r
    $ns connect $udp_s $udp_r
    $udp_s set packetSize_ 1500
    $udp_s set fid_ $id
    $udp_r set packetSize_ 1500
    $udp_r set fid_ $id

    set app_sd_s [new Application/AppSd]
    set app_sd_r [new Application/AppSd]
    $app_sd_s attach-agent $udp_s
    $app_sd_r attach-agent $udp_r
    $app_sd_s set rtt_ $rtt
    $app_sd_r set rtt_ $rtt
    $app_sd_s set interval_ $initrate
    $app_sd_s set coninterval_ 0.0075
#    $app_sd_s set syninterval_ $rtt
   $app_sd_s set syninterval_ 0.01
   $app_sd_s set ackinterval_ 0.05
   $app_sd_s set errinterval_ 0.0001
   $app_sd_s set recvflagsize_ 2000
   $app_sd_s set sendflagsize_ 2000

    set tcp1 [new Agent/TCP/FullTcp]
    set tcp2 [new Agent/TCP/FullTcp]
    $tcp1 set nodelay_ true
    $tcp2 set nodelay_ true
    $ns attach-agent $src $tcp1
    $ns attach-agent $dest $tcp2
    $ns connect $tcp1 $tcp2
#    $tcp1 set fid_ 0
#    $tcp2 set fid_ 0
    $tcp1 listen

    set app1 [new Application/AppSc $tcp1]
    set app2 [new Application/AppSc $tcp2]
    $app2 connect $app1

    $udp_s set-status server
    $app_sd_s set-status server
    $app1 set-status server

    $app_sd_s set-target $app1
    $app1 set-target $app_sd_s
    $app_sd_r set-target $app2
    $app2 set-target $app_sd_r

    $ns at 0 "$app_sd_s initialize"
    $ns at 0 "$app_sd_r initialize"

    $ns at $startTime "$app_sd_s start"
    $ns at $startTime "$app_sd_r start"

    return $app_sd_s
}
