#!/usr/bin/perl -w
#
# RETrieve_Open_Digital_Radio_Status, retodrs:
#  Retrieve the status and statistics of an Opendigitalradio service and report
#  the results to Xymon. The information is retrieved from the management server
#  within ODR-DabMux.
#
# NOTE: This script MUST be run on the same machine as DabMux is running! The
#       management server within DabMux is only accessible from localhost.
#       Moreover, the check on availability only works for localhost.
#
# Written by W.J.M. Nelis, wim.nelis@ziggo.nl, 2016.12
#
use strict ;
use Time::Piece ;			# Format time
use ZMQ::LibZMQ3 ;			# Message passing
use ZMQ::Constants qw(ZMQ_REQ) ;
use JSON::PP ;				# Decode server message

#
# Installation constants.
# -----------------------
#
my $XyDisp  = $ENV{XYMSRV} ;		# Name of monitor server
my $XySend  = $ENV{XYMON} ;		# Monitor interface program
my $FmtDate = "%Y.%m.%d %H:%M:%S" ;	# Default date format
   $FmtDate = $ENV{XYMONDATEFORMAT} if exists $ENV{XYMONDATEFORMAT} ;
my $HostName= 'OzoNop' ;		# 'Source' of this test
my $TestName= 'odr_mux' ;		# Test name
my $XyInfo  = "hostinfo host=$HostName" ;	# Extract host info from xymon

my @ColourOf= ( 'red', 'yellow', 'clear', 'green' ) ;

#
# Define the URL to access the management server in DabMux. The access is
# limited to access from localhost!
#
my $ODRMgmtSrvr= 'tcp://127.0.0.1:12720' ;	# URL of server

#
# Define the parameters to show in the table and how to enter them in an RRD.
# From this definition a list of counter-like variables is compiled in hash
# %Counters. The values of these variables need to be saved from one pass of
# this script to the next.
#
my @Params= (
#    OdrName          TableName          RrdDefinition
  [ 'state'        , 'State'          , ''                                ],
  [ 'peak_left'    , 'Peak left [dB]' , 'DS:PeakLeft:GAUGE:600:-100:100'  ],
  [ 'peak_right'   , 'Peak right [dB]', 'DS:PeakRight:GAUGE:600:-100:100' ],
  [ 'num_underruns', ''               , 'DS:Underrun:DERIVE:600:0:U'      ],
  [ 'num_overruns' , ''               , 'DS:Overrun:DERIVE:600:0:U'       ],
  [ 'rate_underruns', 'Underrun [/s]' , ''                                ],
  [ 'rate_overruns' , 'Overrun [/s]'  , ''                                ],
  [ 'min_fill'     , ''               , 'DS:BufferMin:GAUGE:600:-1:U'     ],
  [ 'max_fill'     , ''               , 'DS:BufferMax:GAUGE:600:0:U'      ]
) ;
my %Counters= () ;
foreach ( @Params ) {
  next				unless $$_[2] =~ m/DERIVE/ ;
  $Counters{$$_[0]}= $$_[0] ;		# Save name of counter-like variable
  $Counters{$$_[0]}=~ s/^num_/rate_/ ;	# Build name of derived variable
}  # of foreach

#
# Define the thresholds for the various DabMux statistics and any derived
# value.
#
my %Thresholds= (
  state          => { red => qr/^(?:NoData)$/ },
  rate_underruns => { red => '20.0'},
  rate_overruns  => { red => '20.0'},
# peak_left      => { yellow => ['< -80', '80'] },
# peak_right     => { yellow => ['< -80', '80'] }
) ;

#
# Define the name of the file to hold the values of the counter-type variables.
#
my $SaveFile= '/usr/lib/xymon/client/ext/retodrs.sav' ;

#
# Global variables.
# -----------------
#
my $Now= localtime ;			# Timestamp of tests
   $Now= $Now->strftime( $FmtDate ) ;
my $Colour= $#ColourOf ;		# Test status
my $Result= '' ;			# Message to sent to Xymon

my %HostInfo ;				# Host information from xymon
my %Table0= () ;			# Tables with results
my %Table1= () ;
my @SubChannel= () ;			# Subchannel assignment
my %SubChannel= () ;			#  in both directions
my %ErrMsg ;				# Error messages
   $ErrMsg{$_}= []		foreach ( @ColourOf ) ;
my ($CurTime,$PrvTime) ;		# Times of measurement
my %Prev= () ;				# Variables in previous pass

#
# Save an error message in intermediate list %ErrMsg. Function InformXymon will
# move these messages to the start of the xymon status message.
#
sub LogError($$) {
  my $clr= shift ;			# Status/colour of message
  my $msg= shift ;			# Error message
  return			unless defined $msg ;
  return			unless $msg ;

  chomp $msg ;				# Clean up message, just to be sure
  $msg=~ s/^\s+// ;  $msg=~ s/\s+$// ;

  if ( exists $ErrMsg{$clr} ) {
    push @{$ErrMsg{$clr}}, $msg ;
  } else {
    push @{$ErrMsg{clear}}, $msg ;
  }  # of else
}  # of LogError

#
# Issue a message the the logfile. As this script is run periodically by Xymon,
# StdOut will be redirected to the logfile.
#
sub LogMessage {
  my $Msg= shift ;
  my @Time= (localtime())[0..5] ;
  $Time[4]++ ;  $Time[5]+= 1900 ;
  chomp $Msg ;
  printf "%4d%02d%02d %02d%02d%02d %s\n", reverse(@Time), $Msg ;
}  # of LogMessage

sub max($$) { return $_[0] > $_[1] ? $_[0] : $_[1] ; }
sub min($$) { return $_[0] < $_[1] ? $_[0] : $_[1] ; }


#
# Function AnError is given a short description and a boolean value, whose value
# is false if the operation associated with the description failed. The result
# of this function is the opposite of the boolean value supplied. If failed, the
# description is entered in the error message list %ErrMsg, including the
# content of $!, if the latter is not empty.
#
sub AnError($$) {
  if ( $_[1] ) {
    return 0 ;				# Return a false value
  } else {
    my $msg= $! ;			# Retrieve any error message
    if ( $msg eq '' ) {
      LogError( 'clear', "$_[0] failed" ) ;
    } else {
      LogError( 'clear', "$_[0] failed:" ) ;
      LogError( 'clear', "  $msg" ) ;
    }  # of else
    return 1 ;				# Return a true value
  }  # of else
}  # of AnError

#
# Function ApplyThresholds determines for which channels threshold checks should
# be performed. Then it checks the collected statistics against their
# thresholds, and sets the status of those statistics accordingly. The status of
# the statistics which are not checked against a threshold are set to 'clear'.
#
sub ApplyThresholds() {
  my $hr ;				# Reference in a multi-level hash

 #
 # Set flag ThresholdCheck at each subchannel. It is set to true if threshold
 # checks should be performed.
 #
  if ( exists $HostInfo{select}{list} ) {
    $hr= $HostInfo{select}{list} ;
    $Table1{$_}{ThresholdCheck}= exists $$hr{$_} ? 1 : 0	foreach ( keys %Table1 ) ;
  } else {
    $Table1{$_}{ThresholdCheck}= 1		foreach ( keys %Table1 ) ;
  }  # of else

 #
 # Invoke function CheckValue for each pair {subchannel,statistic} for which a
 # threshold check should and can be performed.
 #
  foreach my $sub ( keys %Table1 ) {
    next			unless $Table1{$sub}{ThresholdCheck} ;
    $hr= $Table1{$sub} ;		# Ref to subchannel info
    foreach my $var ( keys %Thresholds ) {
      next			unless exists $$hr{$var} ;
      CheckValue( $$hr{$var}, $Thresholds{$var} ) ;
    }  # of foreach
  }  # of foreach
}  # of ApplyThresholds

#
# Function BuildMessage takes the the status and statistics in hash %Table and
# builds a message for Xymon.
#
sub BuildMessage() {
  my $RrdMsg ;				# RRD message
  my $sub ;				# Name of a sub channel
  my ($Value,$Status) ;			# Value and status of one statistic
  my @Values ;				# Values of one subchannel
  my $hr ;				# Reference into a hash

 #
 # Check the subchannel assignment against the list of named subchannels. They
 # should match.
 #
  for ( my $i= 0 ; $i<= $#SubChannel ; $i++ ) {
    $hr= $SubChannel[$i] ;
    next			unless defined $hr ;
    next			if exists $Table1{$hr} ;
    $SubChannel[$i]= undef ;
    delete $SubChannel{$hr} ;
  }  # of for
  foreach my $sub ( sort keys %Table1 ) {
    next			if exists $SubChannel{$sub} ;
    $hr= $#SubChannel + 1 ;
    $SubChannel[$hr]= $sub ;
  }  # of foreach
  
 #
 # Build a table showing the services.
 #
  $Result = "<table border=0>\n" ;
  foreach ( sort keys %Table0 ) {
    $Result.= " <tr> <td>$_</td> <td>$Table0{$_}</td> </tr>\n" ;
  }  # of foreach
  $Result.= "</table>\n\n" ;

 #
 # Build the first part of the table to enter the statistics into RRD's and
 # ultimately into graphs.
 #
  $RrdMsg = "<!-- linecount=" . scalar(keys %Table1) . " -->\n" ;
  $RrdMsg.= "<!--DEVMON RRD: $TestName 0 0\n" ;
  $hr= join( ' ', map { $$_[2] } @Params ) ;	# Extract RRD definitions
  $hr=~ s/^\s+// ;  $hr=~ s/\s+$// ;
  $hr=~ s/\s{2,}/ /g ;			# Remove superfluous spaces
  $RrdMsg.= "$hr\n" ;

 #
 # Build the table showing the subchannel status and statistics.
 #
  $Result.= "<table border=1 cellpadding=5>\n" ;
  $Result.= " <tr> <th>Nr.</th> <th>Subchannel</th>" ;
  foreach ( @Params ) {
    next			unless $$_[1] ne '' ;
    $Result.= " <th>$$_[1]</th>" ;
  }  # of foreach
  $Result.= " </tr>\n" ;

# foreach my $sub ( sort keys %Table1 ) {
  for ( my $i= 0 ; $i<= $#SubChannel ; $i++ ) {
    $sub= $SubChannel[$i] ;		# Next subchannel, numeric
    next			unless defined $sub ;
    $hr= $Table1{$sub} ;		# Ref into subchannel statistics

    $Result.= " <tr> <td align='right'>$i</td> <td>$sub</td>" ;
    foreach my $fld ( @Params ) {
      next			unless $$fld[1] ne '' ;
      if ( exists $$hr{$$fld[0]} ) {
	$Value = $$hr{$$fld[0]}{Value}  ;
	$Status= $$hr{$$fld[0]}{Status} ;
	if ( defined $Status ) {
	  if ( $$fld[0] eq 'state'  and  $Status < 2 ) {
	    LogError( $ColourOf[$Status], "$sub is $Value" ) ;
	  } elsif ( $$fld[1] =~ m/^(\w+run) \[.+?\]$/  and  $Status < 2 ) {
	    LogError( $ColourOf[$Status], "$sub high " .lc($1) .": $Value $2" ) ;
	  }  # of elsif
	  $Colour= min( $Colour, $Status ) ;
	  $Status= " &$ColourOf[$Status] " ;
	} else {
	  $Status= '' ;
	}  # of else

	if ( $Value =~ m/^[-+]?\d+(:?\.\d+)?$/ ) {
	  $Result.= " <td align='right'>$Status$Value</td>" ;
	} else {
	  $Result.= " <td>$Value$Status</td>" ;
	}  # of else
      } else {
	$Result.= " <td>Unknown</td>" ;
      }  # of else
    }  # of foreach
    $Result.= " </tr>\n" ;

    @Values= () ;
    foreach my $fld ( @Params ) {
      next			unless $$fld[2] ne '' ;
      if ( exists $$hr{$$fld[0]} ) {
	$Value= $$hr{$$fld[0]}{Value} ;
	push @Values, $Value ;
      } else {
	push @Values, 'U' ;
      }  # of else
    }  # of foreach
    $RrdMsg.= "$sub " . join( ':', @Values ) . "\n" ;

  }  # of foreach
  $Result.= "</table>\n" ;
  $RrdMsg.= "-->" ;
  $Result.= $RrdMsg ;
}  # of BuildMessage

#
# Function CheckPortStatus checks if the TCP port to access the management
# server is available in listen mode. The function result is true if the TCP
# port is found in the listen state, false otherwise.
#
sub CheckPortStatus($) {
  my $url= shift ;			# The url to check
  my $Found= 0 ;			# Function result
  my @F ;				# Fields of a line image

  my @netstat= `netstat -ln4` ;		# Retrieve port status info
  foreach ( @netstat ) {
    chomp ;
    @F= split ;
#   next			unless @F == 6 ;
#   next			unless $F[5] eq 'LISTEN' ;
    next			unless "$F[0]://$F[3]" eq $url ;
    $Found= 1 ;				# Port in listen state found
    last ;				# Terminate search
  }  # of foreach
  return $Found ;
}  # of CheckPortStatus

#
# Function CheckValue checks the value of a statistic against its threshold(s).
# A reference to the value and a reference to the threshold definition are
# passed.
#
sub CheckValue($$) {
  my $vr= shift ;			# Reference to the variable
  my $tr= shift ;			# Reference to the threshold descriptor
  my $clr ;

  $$vr{Status}= $#ColourOf ;		# Default result
  return			if $$vr{Value} eq 'wait' ;

  for ( my $i= $#ColourOf ; $i >= 0 ; $i-- ) {
    $clr= $ColourOf[$i] ;
    next			unless exists $$tr{$clr} ;
    if ( ref($$tr{$clr}) eq 'ARRAY' ) {
      foreach ( @{$$tr{$clr}} ) {
	if ( ref($_) eq 'Regexp' ) {		# Text check
	  $$vr{Status}= $i	if $$vr{Value} =~ m/$_/ ;
	} elsif ( m/^[-+\d\.]+$/ ) {		# Numeric upperbound
	  $$vr{Status}= $i	if $$vr{Value} > $_ ;
	} elsif ( m/^<\s*([-+\d\.]+)$/ ) {	# Numeric lowerbound
	  $$vr{Status}= $i	if $$vr{Value} < $1 ;
	}  # of elsif
      }  # of foreach
    } else {
      if ( ref($$tr{$clr}) eq 'Regexp' ) {		# Text check
	$$vr{Status}= $i	if $$vr{Value} =~ m/$$tr{$clr}/ ;
      } elsif ( $$tr{$clr} =~ m/^[-+\d\.]+$/ ) {	# Numeric upperbound
	$$vr{Status}= $i	if $$vr{Value} > $$tr{$clr} ;
      } elsif ( $$tr{$clr} =~ m/^<\s*([-+\d\.]+)$/ ) {	# Numeric lowerbound
	$$vr{Status}= $i	if $$vr{Value} < $1 ;
      }  # of elsif
    }  # of else
  }  # of for

}  # of CheckValue

#
# Function ComputeRates computes the the rate of change of the counter-like
# variables.
#
sub ComputeRates() {
  my $hr ;
  my $val ;

  foreach my $sub ( keys %Table1 ) {
    $hr= $Table1{$sub} ;		# Ref into hash

    foreach my $var ( keys %Counters ) {
      $$hr{$Counters{$var}}{Value}= 'wait' ;
      $$hr{$Counters{$var}}{State}= undef ;
      if ( exists  $Prev{$sub}{$var}  and
	   defined $$hr{$var}{Value}  and
	   defined $PrvTime ) {
	if ( $$hr{$var}{Value} >= $Prev{$sub}{$var} ) {
	  $val= ( $$hr{$var}{Value} - $Prev{$sub}{$var} ) /
		( $CurTime - $PrvTime ) ;
	  $$hr{$Counters{$var}}{Value}= sprintf( '%.2f', $val ) ;
	}  # of if
      }  # of if
    }  # of foreach

  }  # of foreach
}  # of ComputeRates

#
# Function GetOneReply sends the supplied request to the management server and
# returns the result as a reference to a hash. If something went wrong, the
# result will be undef and an (appropiate?) error message is entered in %ErrMsg.
#
sub GetOneReply($$) {
  my $socket = shift ;			# Socket object
  my $request= shift ;			# Request string

  my $reqlng= length( $request ) ;	# Length of request string
  my $rc= zmq_send( $socket, $request, $reqlng ) ;
  return undef		if AnError( "Request \"$request\"", $rc == $reqlng ) ;

  my $reply= zmq_recvmsg( $socket ) ;
  return undef		if AnError( "Reply on \"$request\"", defined $reply ) ;

  $reply= decode_json( zmq_msg_data($reply) ) ;	# Convert to Perl structure
  return $reply ;
}  # of GetOneReply

#
# Function GetStatistics retrieves both the status and the statistics from the
# server within the DabMux. The results are collected in hash %Table. Subhash
# %{$Table{0}} will contain the service information, %{$Table{1}} will contain
# the subchannel status and statistics.
#
sub GetStatistics() {
  my ($ctxt,$socket) ;			# Connection variables
  my ($reply,$rv) ;			# Request/reply variables
  my ($hr,$vr) ;			# Refs into multi-level hash

  $CurTime= undef ;			# No data collected yet
 #
 # Build a connection to the DabMux server.
 #
  $ctxt= zmq_ctx_new ;
  return undef		if AnError( 'Building context object', defined $ctxt ) ;
  $socket= zmq_socket( $ctxt, ZMQ_REQ ) ;
  return undef		if AnError( 'Creating socket', defined $socket ) ;
  $rv= zmq_connect( $socket, $ODRMgmtSrvr ) ;
  return undef		if AnError( 'Connecting to DabMux', $rv == 0 ) ;

  $reply= GetOneReply( $socket, 'info' ) ;
  return undef		unless defined $reply ;
  %Table0= %$reply ;			# Save overview of services

  my $Once= 1 ;				# Loop control variable
  while ( $Once ) {
    $Once= 0 ;				# Only one iteration.
 #
 # Retrieve the subchannel assignment.
 #
    $reply= GetOneReply( $socket, 'getptree' ) ;
    if ( defined $reply ) {
      foreach my $sub ( keys %{$$reply{subchannels}} ) {
	$hr= $$reply{subchannels}{$sub} ;
	next			unless exists $$hr{id} ;
	next			unless $$hr{id} =~ m/^\d+$/ ;
	$SubChannel[$$hr{id}]= $sub ;
	$SubChannel{$sub}= $$hr{id} ;
      }  # of foreach
    } else {
      next ;				# Skip rest of retrievals
    }  # of else

 #
 # Retrieve the status and the statistics.
 #
    $reply= GetOneReply( $socket, 'state' ) ;
    if ( defined $reply ) {
      foreach my $sub ( keys %$reply ) {
	$Table1{$sub}= {} ;		# Preset result area
	$hr= $Table1{$sub} ;
	foreach ( keys %{$$reply{$sub}} ) {
	  $$hr{$_}{Value} = $$reply{$sub}{$_} ;
	  $$hr{$_}{Status}= undef ;
	}  # of foreach
      }  # of foreach
    } else {
      next ;				# Skip retrieval of statistics
    }  # of else

    $reply= GetOneReply( $socket, 'values' ) ;
    if ( defined $reply  and  exists $$reply{values} ) {
      $CurTime= time ;			# Save time of retrieval
      foreach my $sub ( keys %{$$reply{values}} ) {
	next			unless exists $Table1{$sub} ;
	next			unless exists $$reply{values}{$sub}{inputstat} ;
	$hr= $Table1{$sub} ;			# Ref to destination
	$vr= $$reply{values}{$sub}{inputstat} ;	# Ref to source
	foreach ( keys %$vr ) {
	  $$hr{$_}{Value} = $$vr{$_} ;
	  $$hr{$_}{Status}= undef ;
	}  # of foreach
      }  # of foreach
#   } else {
#     next ;
    }  # of else

 #
 # Terminate the connection to the DabMux server.
 #
  } continue {
    $rv= zmq_close( $socket ) ;
    AnError( 'Closing socket', $rv == 0 ) ;
    return 0 ;				# Return a defined value
  }  # of continue / while
}  # of GetStatistics


#
# Function GetXymonHostInfo retrieves the configuration of host $HostName from
# the xymon configuration file hosts.cfg. If tag ODR is present, it is handled.
#
sub GetXymonHostInfo() {
  %HostInfo= ( select => { default => '^.+$' } ) ;	# Default result

  my @Lines= `$XySend $XyDisp \"$XyInfo\"` ;	# Retrieve host info
  if ( @Lines != 1 ) {				# Handle error condition
    LogError( 'clear', 'Retrieval of host information from Xymon failed' ) ;
    return ;
  }  # of if

  my ($Tag)= $Lines[0] =~ m/\b(ODR[^\s\|]+)/ ;	# Extract tag ODR
  return			unless defined $Tag ;
# return			if $Tag eq 'ODR' ;

  $Tag=~ s/^ODR\:// ;			# Remove tag header
  foreach my $sub ( split( /,/, $Tag ) ) {
    if ( $sub =~ m/select\((.+)\)$/ ) {
      $HostInfo{select}{list}{$_}= 0	foreach ( split(/;/,$1) ) ;
      delete $HostInfo{select}{default} ;
    }  # of if
  }  # of foreach

}  # of GetXymonHostInfo

#
# Function InformXymon sends the message, in global variable $Result, to the
# Xymon server. Any error messages in %ErrMsg are prepended to the message and
# the status (colour) of the message is adapted accordingly.
#
sub InformXymon() {
  my $ErrMsg= '' ;
  my $Clr ;				# Colour of one sub-test

  for ( my $i= 0 ; $i < @ColourOf ; $i++ ) {
    $Clr= $ColourOf[$i] ;
    next			unless @{$ErrMsg{$Clr}} ;
    $Colour= min( $Colour, $i ) ;
    $ErrMsg.= "&$Clr $_\n"	foreach ( @{$ErrMsg{$Clr}} ) ;
  }  # of foreach
  $ErrMsg.= "\n"		if $ErrMsg ;

  $Colour= $ColourOf[$Colour] ;
  $Result= "\"status $HostName.$TestName $Colour $Now\n" .
	   "<b>Open Digital Radio DabMux status</b>\n\n" .
	   "$ErrMsg$Result\"\n" ;
  `$XySend $XyDisp $Result` ;		# Inform Xymon

  $Result= '' ;				# Reset message parameters
  $Colour= $#ColourOf ;
  $ErrMsg{$_}= []		foreach ( @ColourOf ) ;
}  # of InformXymon


#
# Function RestoreCounters restores the values of counter-like variables,
# collected in the previous pass of this script, in hash %Prev. However, if the
# information is too old, nothing is restored.
#
sub RestoreCounters() {
  my @F ;				# Fields in a line image

  %Prev= () ;				# Clear save area
  $PrvTime= undef ;
  unless ( open( FH, '<', $SaveFile ) ) {
    LogError( 'yellow', "Can't read file $SaveFile : $!" ) ;
    LogMessage( "Can't read file $SaveFile : $!" ) ;
    return ;
  }  # of unless

  while ( <FH> ) {
    chomp ;
    @F= split ;
    if ( $F[0] eq 'Time' ) {
      last			unless ( time - $F[1] < 1000 ) ;
      $PrvTime= $F[1] ;
    } elsif ( $F[0] eq 'Counter' ) {
      $Prev{$F[1]}{$F[2]}= $F[3] ;
    }  # of elsif
  }  # of while
  close( FH ) ;
}  # of RestoreCounters

# Function SaveCounters saves the counter-type variables in a file. They are
# retrieved in the next pass of this script, and will be used to calculate the
# rate in which these variables increase.
#
sub SaveCounters() {
 #
 # If the retrieval of the statistics failed, nothing should be saved. Perhaps
 # the information saved at the (a?) previous pass is usable in the next pass.
 #
  return			unless defined $CurTime ;

  unless ( open( FH, '>', $SaveFile ) ) {
    LogError( 'yellow', "Can't write file $SaveFile : $!" ) ;
    LogMessage( "Can't write file $SaveFile : $!" ) ;
    return ;
  }  # of unless

  print FH "Time $CurTime\n" ;
  foreach my $sub ( sort keys %Table1 ) {
    foreach my $var ( sort keys %{$Table1{$sub}} ) {
      next			unless exists $Counters{$var} ;
      next			unless defined $Table1{$sub}{$var}{Value} ;
      print FH "Counter $sub $var $Table1{$sub}{$var}{Value}\n" ;
    }  # of foreach
  }  # of foreach
  close( FH ) ;
}  # of SaveCounters


#
# MAIN PROGRAM.
# =============
#
unless ( CheckPortStatus($ODRMgmtSrvr) ) {	# If server down,
  LogMessage( "URL \"$ODRMgmtSrvr\" is not available" ) ;
  LogError( 'red', 'Server is not available' ) ;
  InformXymon ;				#  report error via xymon
  exit ;				#  and stop
}  # of unless

unless ( defined GetStatistics ) {	# If retrieval fails,
  InformXymon ;				#  report error via xymon
  exit ;				#  and stop
}  # of unless

GetXymonHostInfo ;			# Retrieve additional host info
if ( keys %Counters ) {
  RestoreCounters ;			# Get counters from previous pass
  ComputeRates ;			# Compute their rate of change
  SaveCounters ;			# Save counters for next pass
}  # of if

ApplyThresholds ;			# Check for out-of-range values
BuildMessage ;				# Build xymon message
InformXymon ;				# Send message to xymon

