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

use DBI;
use POSIX qw(strftime);
use HTML::Template;

print "Content-Type: text/html\n\n";


#
#  Load + Create the template
#
my $template = "";
while( my $line = <DATA> )
{
   $template .= $line;
}

#
#  Read the temperature
#
my $stats;

#
#  Connect to the database.
#
my $filename = "/var/cache/temperature/db";
my $dbh =
  DBI->connect( "dbi:SQLite:dbname=$filename", "", "",
              { AutoCommit => 1, RaiseError => 0 } ) or
  die "Could not open SQLite database: $DBI::errstr";
$dbh->{ sqlite_unicode } = 1;

#
#  Select readings.
#
my $sql = $dbh->prepare( "SELECT temperature,humidity FROM readings ORDER BY recorded DESC LIMIT 50" );
$sql->execute();
my $t;
my $h;

$sql->bind_columns( undef, \$t, \$h );
my $count = 0;
while( $sql->fetch() )
{
    push( @$stats, { temperature => $t, humidity => $h, count => $count } );
    $count += 1;
}
$sql->finish();


#
# Populate the template, display it & terminate
#
my $tmplr = HTML::Template->new( scalarref => \$template,
				 die_on_bad_params => 0 );
$tmplr->param( readings => $stats );
print $tmplr->output();
exit(0);

__DATA__
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
				{ x: <!-- tmpl_var name='count' -->, y: <!-- tmpl_var name='temperature' --> },

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
				{ x: <!-- tmpl_var name='count' -->, y: <!-- tmpl_var name='humidity' --> },

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
                 <title>Balcony</title>
</head>
<body>
<center><h1>Balcony</h1></center>
<div id="temperature" style="height: 300px; width: 80%; margin-left:10%"></div>
<p>&nbsp;</p>
<div id="humidity" style="height: 300px; width: 80%; margin-left:10%"></div>
</body>
</html>
