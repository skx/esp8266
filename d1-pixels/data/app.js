// Image is a reserved word in JavaScript so I use Pixels instead
var Pixels = function( options )
{
    // Private Properties & Methods
    var private      = {};
    private.xPixels  = (options !== undefined && options.xPixels !== undefined) ? options.xPixels : 8;
    private.yPixels  = (options !== undefined && options.yPixels !== undefined) ? options.yPixels : 8;
    private.pixelH   = (options !== undefined && options.pixelH !== undefined) ? options.pixelH : 20;
    private.pixelW   = (options !== undefined && options.pixelW !== undefined) ? options.pixelW : 20;
    private.pixels   = [];

    // Public Properties & Methods
    var public      = {
        reset: function()
        {
            for (var y = 1; y <= private.yPixels; y += 1)
            {
                for (var x = 1; x <= private.xPixels; x += 1)
                {
                    this.setPixel(
                        x, y, {
                            mouseOver: false,
                            on: false,
                            x: ((x - 1) * private.pixelW),
                            y: ((y - 1) * private.pixelH),
                            h: (private.pixelH - 1),
                            w: (private.pixelW - 1)
                        }
                    );
                }
            }
        },
        setPixel: function( row, col, value )
        {
            private.pixels[ private.xPixels * row + col ] = value;
        },

        getPixel: function( row, col )
        {
            return private.pixels[ private.xPixels * row + col ];
        },

        getPixels: function()
        {
            return private.pixels;
        },

        setPixels: function( pixels )
        {
            private.pixels = pixels;
        }
    };

    public.reset();
    return public;
};


var ImageCanvas = function( options )
{
    // Private Properties & Methods
    var private      = {};
    private.xPixels  = (options !== undefined && options.xPixels !== undefined) ? options.xPixels : 8;
    private.yPixels  = (options !== undefined && options.yPixels !== undefined) ? options.yPixels : 8;
    private.pixelH   = (options !== undefined && options.pixelH !== undefined) ? options.pixelH : 20;
    private.pixelW   = (options !== undefined && options.pixelW !== undefined) ? options.pixelW : 20;

    private.offset   = (options !== undefined && options.offset !== undefined) ? options.offset : { x: 10, y: 10 };

    private.pixels   = new Pixels( {
        xPixels: private.xPixels,
        yPixels: private.yPixels,
        pixelW: private.pixelW,
        pixelH: private.pixelH
    });

    private.cWidth   = (private.xPixels * private.pixelW);
    private.cHeight  = (private.yPixels * private.pixelH);
    private.hasFocus = false;

    private.cCanvas  = $('<canvas/>').attr({ width: private.cWidth, height: private.cHeight });
    private.cContext = private.cCanvas.get(0).getContext("2d");

    private.cContext.fillStyle = '#999999';
    private.cContext.fillRect(0,0, private.cWidth, private.cHeight);

    private.cContext.fillStyle = '#FFFFFF';
    private.cContext.fillRect(1,1, (private.cWidth - 2), (private.cHeight - 2));

    private.cContext.beginPath();
    private.cContext.strokeStyle = "#DDDDDD";
    private.cContext.lineWidth   = "1";

    for (var y = 20; y <= private.cHeight; y += private.pixelH) {
        private.cContext.moveTo(0.5 + y, 1);
        private.cContext.lineTo(0.5 + y, private.cHeight - 1);
    }

    for (var x = 20; x <= private.cWidth; x += private.pixelW) {
        private.cContext.moveTo(1, 0.5 + x);
        private.cContext.lineTo(private.cWidth - 1, 0.5 + x);
    }

    private.cContext.stroke();

    private.cGrid = private.cContext.getImageData(0,0, private.cWidth, private.cHeight);

    // Public Properties & Methods
    return {

        // Public getter
        get: function( prop ) {
            if ( private.hasOwnProperty( prop ) ) {
                return private[ prop ];
            }
        },

        heart: function (){
            private.pixels.reset();

            //
            // List of pixels
            //
            var arr = [      1,2, 1,3,           1,6, 1,7 ,
                             2,1, 2,2, 2,3, 2,4, 2,5, 2,6, 2,7, 2,8,
                             3,1, 3,2, 3,3, 3,4, 3,5, 3,6, 3,7, 3,8,
                             4,1, 4,2, 4,3, 4,4, 4,5, 4,6, 4,7, 4,8,
                                  5,2, 5,3, 5,4, 5,5, 5,6, 5,7,
                                       6,3, 6,4, 6,5, 6,6,
                                            7,4, 7,5,
                      ];
            for (var i = 0; i < arr.length; i += 2) {
                var y = arr[i];
                var x = arr[i+1];

                var currentPixel = private.pixels.getPixel( x,y );
                currentPixel.mouseOver = false;
                currentPixel.on = true;
                private.pixels.setPixel(x,y, currentPixel);
            }
        },


        cross: function (){
            private.pixels.reset();

            //
            // List of pixels
            //
            var arr = [  1,1, 2,2, 3,3, 4,4, 5,5, 6,6, 7,7, 8,8,
                         1,8,
                         2,7,
                         3,6,
                         4,5,
                         5,4,
                         6,3,
                         7,2,
                         8,1,
                      ];
            for (var i = 0; i < arr.length; i += 2) {
                var y = arr[i];
                var x = arr[i+1];

                var currentPixel = private.pixels.getPixel( x,y );
                currentPixel.mouseOver = false;
                currentPixel.on = true;
                private.pixels.setPixel(x,y, currentPixel);
            }
        },

        load: function ( pixels )
        {
            // ...
        },

        save: function ()
        {
            // ...
        },

        update: function ( step, canvas, context )
        {
            if(
                (Mouse.x > 0 && Mouse.y > 0) &&
                (Mouse.x >= 0 && Mouse.x <= private.cWidth)  &&
                (Mouse.y >= 0 && Mouse.y <= private.cHeight)
            ){
                console.log('ImageCanvas has focus!');
                private.hasFocus = true;
            }else{
                private.hasFocus = false;
            }

            if (private.hasFocus === true)
            {
                for (var y = 1; y <= private.yPixels; y+= 1)
                {
                    for (var x = 1; x <= private.xPixels; x+= 1)
                    {
                        var currentPixel = private.pixels.getPixel( x,y );

                        // Reset mouseover
                        currentPixel.mouseOver = false;

                        if ( Mouse.x >= (private.offset.x + currentPixel.x) && Mouse.x <= (private.offset.x + currentPixel.x + currentPixel.w)){
                            if ( Mouse.y >= (private.offset.y + currentPixel.y) && Mouse.y <= (private.offset.y + currentPixel.y + currentPixel.h) ){
                                currentPixel.mouseOver = true;
                                if (Mouse.events.mousedown === true)
                                {
                                    currentPixel.on = ( Mouse.events.mouseButton === 1);
                                }
                            }
                        }
                        private.pixels.setPixel( x, y, currentPixel );
                    }
                }
            }
        },

        render: function ( step, canvas, context )
        {
            context.putImageData( private.cGrid, private.offset.x, private.offset.y );

            for (var y = 1; y <= private.yPixels; y+= 1)
            {
                for (var x = 1; x <= private.xPixels; x+= 1)
                {
                    var currentPixel = private.pixels.getPixel( x, y );

                    if ( currentPixel.on === true)
                    {
                        context.fillStyle = 'rgba(0,0,0,1)';
                        context.fillRect( (private.offset.x + currentPixel.x + 1), (private.offset.y + currentPixel.y + 1), (private.pixelW - 1), (private.pixelH - 1) );
                    }

                    if ( currentPixel.mouseOver === true)
                    {
                        context.fillStyle = 'rgba(0,0,0,0.2)';
                        context.fillRect( (private.offset.x + currentPixel.x + 1), (private.offset.y + currentPixel.y + 1), (private.pixelW - 1), (private.pixelH - 1) );
                    }
                }
            }
        },

        dump: function ( step, canvas, context )
        {
            var orig = $('#data').text();

            $('#data').html("");

            for (var y = 1; y <= private.yPixels; y+= 1)
            {
                var row = "";

                for (var x = 1; x <= private.xPixels; x+= 1)
                {
                    var currentPixel = private.pixels.getPixel( x, y );

                    if ( currentPixel.on === true)
                    {
                        row += "1";
                    }
                    else
                    {
                        row += "0";
                    }
                }
                // Convert to decimal
                row = parseInt(row, 2);

                // Show what we'll send
                $("#data").append( row);
                if ( y < private.yPixels )
                    $("#data").append( "," );
            }

            if ( orig != $('#data').text() )
            {
                //
                // Here we do a JSON post - because the pixels
                // have changed since their last dump.
                //
                $.get( "/?data=" + $('#data').text(), function( data ) {
                    console_log( "Requested data" );
                });
            }
        }
    };
};

var Mouse = {
    x: 0,
    y: 0,
    events: {
        mouseover: false,
        mouseout: false,
        mousedown: false,
        mousemove: false,
        mouseButton: 0
    },
    previousEvents: {
        mouseover: false,
        mouseout: false,
        mousedown: false,
        mousemove: false,
        mouseButton: 0
    }
};

$('#paintMe').on('mouseover', function(e){
    Mouse.events.mouseover   = true;
    Mouse.x                  = Math.floor(e.clientX - $(this).offset().left);
    Mouse.y                  = Math.floor(e.clientY - $(this).offset().top);
});

$('#paintMe').on('mouseout', function(e)
{
    Mouse.events.mousemove   = false;
    Mouse.events.mouseover   = false;
    Mouse.events.mousedown   = false;
    Mouse.events.mouseout    = true;
    Mouse.events.mouseButton = 0;

    Mouse.x                  = 0;
    Mouse.y                  = 0;
});

$('#paintMe').on('mousemove', function(e)
{
    Mouse.events.mousemove   = true;
    Mouse.x                  = Math.floor(e.clientX - $(this).offset().left);
    Mouse.y                  = Math.floor(e.clientY - $(this).offset().top);
    return false;
});

$('#paintMe').on('mousedown', function(e)
{
    Mouse.events.mousedown   = true;
    Mouse.events.mouseup     = false;
    Mouse.events.mouseButton = e.which;
    return false;
});

$('#paintMe').on('mouseup', function(e)
{
    Mouse.events.mousedown   = false;
    Mouse.events.mouseup     = true;
    Mouse.events.mouseButton = 0;
    return false;
});

// This returns false to disable the operating systems context menu on right click
$('#paintMe').contextmenu(function() {
    return false;
});

var App = {
    timestamp: function() {
        return window.performance && window.performance.now ? window.performance.now() : new Date().getTime();
    },

    run: function(options)
    {
        var now,
            dt       = 0,
            last     = App.timestamp(),
            slow     = options.slow || 1, // slow motion scaling factor
            step     = 1/options.fps,
            slowStep = slow * step,
            update   = options.update,
            render   = options.render,
            canvas   = options.canvas,
            context  = options.canvas.get(0).getContext("2d");

        function frame() {
            now = App.timestamp();
            dt = dt + Math.min(1, (now - last) / 1000);
            while(dt > slowStep) {
                dt = dt - slowStep;
                update(step, canvas, context);
            }
            render(dt/slow, canvas, context);
            last = now;
            requestAnimationFrame(frame, canvas);
        }
        requestAnimationFrame(frame);
    }
};

var iCanvas = new ImageCanvas();

App.run({
    canvas: $('#paintMe'),
    fps: 60,
    update: function(step, canvas, context){

        ////////////////////////////////////////////////////////////////////////////////////////////////////
        // Mouse Events
        ////////////////////////////////////////////////////////////////////////////////////////////////////

        // Has the mouse event changed since it was last logged?
        if ( Mouse.previousEvents.mouseover !== Mouse.events.mouseover )
        {
            Mouse.previousEvents.mouseover = Mouse.events.mouseover;
            console.log('Mouse Over Event Changed');
        }

        if ( Mouse.previousEvents.mousemove !== Mouse.events.mousemove )
        {
            Mouse.previousEvents.mousemove = Mouse.events.mousemove;
            console.log('Mouse Move Event Changed');
        }

        if ( Mouse.previousEvents.mouseup !== Mouse.events.mouseup )
        {
            Mouse.previousEvents.mouseup = Mouse.events.mouseup;
            console.log('Mouse Up Event Changed');
        }

        if ( Mouse.previousEvents.mousedown !== Mouse.events.mousedown )
        {
            Mouse.previousEvents.mousedown = Mouse.events.mousedown;
            console.log('Mouse Down Event Changed');
        }

        if ( Mouse.previousEvents.mouseButton !== Mouse.events.mouseButton )
        {
            Mouse.previousEvents.mouseButton = Mouse.events.mouseButton;
            console.log('Mouse Button Event Changed');
        }

        iCanvas.update( step, canvas, context );
    },
    render: function(step, canvas, context){
        iCanvas.render( step, canvas, context );
        iCanvas.dump();
    }
});
