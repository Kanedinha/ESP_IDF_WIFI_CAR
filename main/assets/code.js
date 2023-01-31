var speed = 0;
var dir = 0;
var up = 0;
var MAX_STEP = 50;
var Sensor;
var readBattery = 0;

$("#direita").mousedown(function () {
    up = setInterval(function () {
        dir = 1;
        direction_send();
    }, 80);
});

$("#direita").mouseup(function () {
    clearInterval(up);
});

$("#esquerda").mousedown(function () {
    up = setInterval(function () {
        dir = -1;
        direction_send();
    }, 80);
});

$("#esquerda").mouseup(function () {
    clearInterval(up);
});

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