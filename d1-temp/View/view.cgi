#!/usr/bin/perl
#
#  Simple visualisation from an SQLite database.
#
#  TODO:
#   drop-down with different devices.
#   mapping from MAC -> location.
#
#




use strict;
use warnings;

use CGI;
use DBI;
use POSIX qw(strftime);
use HTML::Template;

print "Content-Type: text/html\n\n";

#
#  Get access to the device we're viewing
#
my $cgi = new CGI;
my $dev = $cgi->param("device");

#
#  Mapping
#
my %NAMES;
$NAMES{ 'A0:20:A6:02:86:3F' } = "Balcony";

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
    #  Otherwise let the user choose the available devices.
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
#  Find all the readings
#
sub readings
{
    my ($mac) = (@_);

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
        "SELECT temperature,humidity,recorded FROM readings WHERE mac=? ORDER BY recorded DESC LIMIT 50"
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


sub show_graph
{
    my ($mac) = (@_);

    my $txt = load_template("index.html");
    my $tmplr = HTML::Template->new( scalarref         => \$txt,
                                     die_on_bad_params => 0 );

    my $readings = readings($mac);
    $tmplr->param( readings => $readings )      if ($readings);
    $tmplr->param( name     => $NAMES{ $mac } ? $NAMES{$mac} : $mac );
    print $tmplr->output();
}


sub show_menu
{
    my $txt = load_template("choose.html");
    my $tmplr = HTML::Template->new( scalarref         => \$txt,
                                     die_on_bad_params => 0 );
    print( $tmplr->output() );
}


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
<center><h1><!-- tmpl_if name='name' --><!-- tmpl_var name='name' --><!-- tmpl_else -->Unknown Location<!-- /tmpl_if --></h1></center>
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
<p><a href="?device=A0:20:A6:02:86:3F">Balcony</a></p>
</body>
</html>
end: choose.html
