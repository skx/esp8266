#!/usr/bin/perl
#
#  Simple visualisation from an SQLite database containing temperature
# and humidity for a small number of devices.
#
#  Configuration:
#
#    Edit the `NAMES` hash to include your devices.
#
#    Check the path of the SQLite database.
#
# Steve
# --
#




use strict;
use warnings;

use CGI;
use DBI;
use POSIX qw(strftime);
use HTML::Template;

print "Content-Type: text/html\n\n";

#
#  Get access to the device we're viewing, if any.
#
my $cgi = new CGI;
my $dev = $cgi->param("device");

#
#  Mapping MAC addresses human names.
#
my %NAMES;
$NAMES{ 'A0:20:A6:1A:1F:DC' } = "Balcony";
$NAMES{ 'A0:20:A6:15:4F:A5' } = "Bathroom";

#
#  If we have a device-name then show it.
#
if ($dev)
{
    show_graph($dev);
}
else
{
    #
    #  Otherwise let the user choose from the available devices.
    #
    show_menu();
}

exit(0);




#
#  Connect to the database
#
sub db_connect
{
    my $filename = "/var/cache/temperature/db";
    my $dbh =
      DBI->connect( "dbi:SQLite:dbname=$filename", "", "",
                    { AutoCommit => 1, RaiseError => 0 } ) or
      die "Could not open SQLite database: $DBI::errstr";
    $dbh->{ sqlite_unicode } = 1;

    return ($dbh);
}


#
# Return $count readings from the device with the specified MAC-address.
#
sub readings
{
    my ( $mac, $count ) = (@_);

    #
    #  Get the database handle
    #
    my $dbh = db_connect();

    #
    #  The stats we'll populate.
    #
    my $stats;

    #
    #  Select readings.
    #
    my $sql = $dbh->prepare(
        "SELECT temperature,humidity,recorded FROM readings WHERE mac=? ORDER BY recorded DESC LIMIT $count"
    );
    $sql->execute($mac);

    my $t;
    my $h;
    my $a;
    $sql->bind_columns( undef, \$t, \$h, \$a );

    while ( $sql->fetch() )
    {
        next unless ( $t && $h );
        push( @$stats, { temperature => $t, humidity => $h, time => $a } );
    }
    $sql->finish();

    return ($stats);
}


#
# Load a (HTML::Template) template-file from our __DATA__ area.
#
sub load_template
{
    my ($file) = (@_);

    my $txt = "";

    my $in_file = 0;

    while ( my $line = <DATA> )
    {
        if ( $line =~ /^start: $file/ )
        {
            $in_file = 1;
            next;
        }
        else
        {
            if ( $line =~ /^end: $file/ )
            {
                $in_file = 0;
            }
        }
        $txt .= $line if ($in_file);
    }
    return ($txt);
}


#
# Show a graph of temp/humidity from the given MAC address.
#
sub show_graph
{
    my ($mac) = (@_);

    #
    #  The number of readings to show.
    #
    my $count = $cgi->param("count") || 50;


    my $txt = load_template("index.html");
    my $tmplr = HTML::Template->new( scalarref         => \$txt,
                                     die_on_bad_params => 0 );

    my $readings = readings( $mac, $count );
    $tmplr->param( readings => $readings ) if ($readings);
    $tmplr->param( name => $NAMES{ $mac } ? $NAMES{ $mac } : $mac );
    $tmplr->param( mac => $mac );
    print $tmplr->output();
}


#
# Show a menu of all listed devices.
#
sub show_menu
{

    #
    #  Get the database handle
    #
    my $dbh = db_connect();

    #
    #  The devices we know about.
    #
    my $devices;

    #
    #  Select the devices
    #
    my $sql = $dbh->prepare("SELECT DISTINCT(mac) FROM devices");
    $sql->execute();

    my $mac;

    $sql->bind_columns( undef, \$mac );
    while ( $sql->fetch() )
    {
        my $name = $NAMES{ $mac } || $mac;
        push( @$devices, { name => $name, mac => $mac } );
    }
    $sql->finish();
    $dbh->disconnect();

    my $txt = load_template("choose.html");
    my $tmplr = HTML::Template->new( scalarref         => \$txt,
                                     die_on_bad_params => 0 );

    $tmplr->param( devices => $devices ) if ($devices);
    print( $tmplr->output() );
}


#
# HTML::Template-templates follow now.
#
__DATA__
start: index.html
<!DOCTYPE HTML>
<html>
<head>
<script src="/canvasjs.min.js"></script>
<script type="text/javascript">
function temperature() {
		var chart = new CanvasJS.Chart("temperature",
		{

			title:{
				text: "Temperature",
				fontSize: 30
			},
                        animationEnabled: true,
			axisX:{

				gridColor: "Silver",
				tickColor: "silver",

			},
                        toolTip:{
                          shared:true
                        },
			theme: "theme2",
			axisY: {
				gridColor: "Silver",
				tickColor: "silver",
				maximum: 40,
  			      minimum: -40,

			},
			legend:{
				verticalAlign: "center",
				horizontalAlign: "right"
			},
			data: [
			{
				type: "line",
				showInLegend: true,
				lineThickness: 2,
				name: "Temperature",
				markerType: "square",
				color: "#F08080",
				dataPoints: [

<!-- tmpl_loop name='readings' -->
				{ x:  new Date( 1000 * <!-- tmpl_var name='time' -->), y: <!-- tmpl_var name='temperature' --> },

<!-- /tmpl_loop -->
				]
			},


			],
          legend:{
            cursor:"pointer",
            itemclick:function(e){
              if (typeof(e.dataSeries.visible) === "undefined" || e.dataSeries.visible) {
              	e.dataSeries.visible = false;
              }
              else{
                e.dataSeries.visible = true;
              }
              chart.render();
            }
          }
		});

chart.render();
}


function humidity() {
		var chart = new CanvasJS.Chart("humidity",
		{

			title:{
				text: "Humidity",
				fontSize: 30
			},
                        animationEnabled: true,
			axisX:{

				gridColor: "Silver",
				tickColor: "silver",

			},
                        toolTip:{
                          shared:true
                        },
			theme: "theme2",
			axisY: {
			   maximum: 100,
			      minimum: 0,
				gridColor: "Silver",
				tickColor: "silver"
			},
			legend:{
				verticalAlign: "center",
				horizontalAlign: "right"
			},
			data: [
			{
				type: "line",
				showInLegend: true,
				lineThickness: 2,
				name: "Humidity",
				markerType: "square",
				color: "#F08080",
				dataPoints: [

<!-- tmpl_loop name='readings' -->
				{ x:  new Date( 1000 * <!-- tmpl_var name='time' -->), y: <!-- tmpl_var name='humidity' --> },

<!-- /tmpl_loop -->
				]
			},


			],
          legend:{
            cursor:"pointer",
            itemclick:function(e){
              if (typeof(e.dataSeries.visible) === "undefined" || e.dataSeries.visible) {
              	e.dataSeries.visible = false;
              }
              else{
                e.dataSeries.visible = true;
              }
              chart.render();
            }
          }
		});

chart.render();
}


window.onload = function () {
temperature();
humidity();
}
  </script>
<title><!-- tmpl_if name='name' --><!-- tmpl_var name='name' --><!-- tmpl_else -->Unknown Location<!-- /tmpl_if --></title>
</head>
<body>
<center><h1><a href="view.cgi">Home</a> | <!-- tmpl_if name='name' --><!-- tmpl_var name='name' --><!-- tmpl_else -->Unknown Location<!-- /tmpl_if --></h1></center>
<blockquote>
<p>Show
<a href="view.cgi?device=<!-- tmpl_var name='mac' -->;count=50">50</a>,
<a href="view.cgi?device=<!-- tmpl_var name='mac' -->;count=100">100</a>,
<a href="view.cgi?device=<!-- tmpl_var name='mac' -->;count=150">150</a>, or
<a href="view.cgi?device=<!-- tmpl_var name='mac' -->;count=250">250</a>
readings.
</p>
<div id="temperature" style="height: 300px; width: 80%; margin-left:10%"></div>
<p>&nbsp;</p>
<div id="humidity" style="height: 300px; width: 80%; margin-left:10%"></div>
</body>
</html>
end: index.html
start: choose.html
<!DOCTYPE HTML>
<html>
<head>
<title>Choose Location</title>
</head>
<body>
<center><h1>Choose Location</h1></center>
<p>&nbsp;</p>
<table width="100%"><tr><td width="20%"></td><td width="60%">
<!-- tmpl_if name='devices' -->
<!-- tmpl_loop name='devices' -->
<p><a href="?device=<!-- tmpl_var name='mac' -->"><!-- tmpl_var name='name' --></a></p>
<!-- /tmpl_loop -->
<!-- tmpl_else -->
<p>No devices are known; edit this script to setup the MAC-table.</p>
<!-- /tmpl_if -->
</td><td width="20%"></td></tr></table>
</body>
</html>
end: choose.html
