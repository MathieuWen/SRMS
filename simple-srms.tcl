set ns [new Simulator]
set nf [open out.tr w]
set qf [open qm.out w]

$ns trace-all $nf

set testTime   5.0

set s [$ns node]
set d [$ns node]
set n1 [$ns node]
set n2 [$ns node]

$ns duplex-link $s $n1 5Mb 5.5ms DropTail
$ns duplex-link $n2 $d 5Mb 5.5ms DropTail
$ns duplex-link $n1 $n2 2Mb 5.5ms DropTail
$ns queue-limit $n1 $n2 5
$ns queue-limit $n2 $n1 5

#$ns trace-queue $n1 $n2 $nf
set qmonitor [$ns monitor-queue $n1 $n2 $qf 0.02]
[$ns link $n1 $n2] queue-sample-timeout

 
#$ns trace-queue $n1 $n2 $nf

#set qmonitor [$ns monitor-queue $n2 $d $qf 0.02];
#[$ns link $n2 $d] queue-sample-timeout;
#set qmonitor [$ns monitor-queue $s $n1 $qf 0.02];
#[$ns link $s $n1] queue-sample-timeout;
 
set udp [new Agent/UDP]
$ns attach-agent $s $udp
$udp set packetSize_ 1000

set cbr [new Application/Traffic/CBR]
#set cbr [new Application/Traffic/Exponential] 
$cbr attach-agent $udp
$cbr set packetSize_ 1000
#$cbr set rate_ 1000000
$cbr set rate_ 2Mb
#$cbr set interval_ 0.004
#$cbr set burst_time_ 50ms
#$cbr set idle_time_ 10ms



set srms [new Agent/srms_rcv]
#set srms [new Agent/SRMSink]
#set srms [new Agent/Null]
$srms set snd_interval_ 0.004
$srms set interval_ 0.02
$ns attach-agent $d $srms
$ns connect $udp $srms


$srms set_filename owd_flow0
 
proc finish {} {
    global ns
    exit 0
}


$ns at 0.0 "$cbr start"
$ns at $testTime "$cbr stop"
$ns at $testTime "$srms close_file"
$ns at [expr $testTime + 1.0] "finish"
 
$ns run