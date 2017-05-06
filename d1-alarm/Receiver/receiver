#!/usr/bin/perl
#
# Listen for a message on the topic "alarm", when found
# execute the `alarm.sh` script - which will play an alarm.
#

use strict;
use warnings;

use Net::MQTT::Simple;

my $mqtt = Net::MQTT::Simple->new("localhost");

$mqtt->run(
    "#" => sub {
        my ( $topic, $message ) = @_;
        if ( $topic =~ /alarm/i )
        {
            system("run-parts /opt/d1-alarm/part.d/");
        }
    },
);
