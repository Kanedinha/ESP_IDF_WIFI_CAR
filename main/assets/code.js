var speed = 0;
var dir = 0;
var up = 0;
var MAX_STEP = 50;
var Sensor;
var readBattery = 0;
var timeout = 80;

// ---------------- 1 step --------------------//
$("#direita1").mousedown(function () {
    up = setInterval(function () {
        dir = 1;
        direction_send();
    }, timeout);
});
$("#direita1").mouseup(function () {
    clearInterval(up);
});
$("#direita1").mouseout(function () {
    clearInterval(up);
});

$("#esquerda1").mousedown(function () {
    up = setInterval(function () {
        dir = -1;
        direction_send();
    }, timeout);
});
$("#esquerda1").mouseup(function () {
    clearInterval(up);
});
$("#esquerda1").mouseout(function () {
    clearInterval(up);
});

// ---------------- 10 step --------------------//

$("#direita10").mousedown(function () {
    up = setInterval(function () {
        dir = 10;
        direction_send();
    }, timeout);
});
$("#direita10").mouseup(function () {
    clearInterval(up);
});
$("#direita10").mouseout(function () {
    clearInterval(up);
});

$("#esquerda10").mousedown(function () {
    up = setInterval(function () {
        dir = -10;
        direction_send();
    }, timeout);
});
$("#esquerda10").mouseup(function () {
    clearInterval(up);
});
$("#esquerda10").mouseout(function () {
    clearInterval(up);
});

// ---------------- 1000 step --------------------//

$("#direita100").mousedown(function () {
    up = setInterval(function () {
        dir = 100;
        direction_send();
    }, timeout);
});
$("#direita100").mouseup(function () {
    clearInterval(up);
});
$("#direita100").mouseout(function () {
    clearInterval(up);
});

$("#esquerda100").mousedown(function () {
    up = setInterval(function () {
        dir = -100;
        direction_send();
    }, timeout);
});
$("#esquerda100").mouseup(function () {
    clearInterval(up);
});
$("#esquerda100").mouseout(function () {
    clearInterval(up);
});

// ---------------- Full revolution --------------------//

$("#direitaFull").mousedown(function () {
    up = setInterval(function () {
        dir = 4096;
        direction_send();
    }, timeout);
});
$("#direitaFull").mouseup(function () {
    clearInterval(up);
});
$("#direitaFull").mouseout(function () {
    clearInterval(up);
});

$("#esquerdaFull").mousedown(function () {
    up = setInterval(function () {
        dir = -4096;
        direction_send();
    }, timeout);
});
$("#esquerdaFull").mouseup(function () {
    clearInterval(up);
});
$("#esquerdaFull").mouseout(function () {
    clearInterval(up);
});

// ---------------- jQuery AJAX --------------------//

function direction_send() {
    direction = { x: dir, y: speed };
    $.ajax({
        type: "POST",
        url: "/direction",
        data: JSON.stringify(direction),
        success: function (result) { 
            Sensor = jQuery.parseJSON(result);
            $("#Angle").html('<p class="text" id="ang">Angle: ' + Sensor.x + 'º</p>');
        },
        error: function (result) {
            alert('error');
        }
        
    });
}


// $(document).ready(function(){
//     readBattery = setInterval(function(){

//     });
// });