var speed = 0;
var dir = 0;
var up = 0;
var MAX_STEP = 50;
var Sensor;
var readBattery = 0;
var readTemperature = 0;
var readSpeed = 0;
var timeout = 80;

// ---------------- ~1 degree --------------------//
$("#direita1").mousedown(function () {
    up = setInterval(function () {
        dir = 12;
        direction_send();
    }, timeout);
});
$("#direita1").mouseup(function () {
    clearInterval(up);
    dir = 0;
});
$("#direita1").mouseout(function () {
    clearInterval(up);
    dir = 0;
});

$("#esquerda1").mousedown(function () {
    up = setInterval(function () {
        dir = -12;
        direction_send();
    }, timeout);
});
$("#esquerda1").mouseup(function () {
    clearInterval(up);
    dir = 0;
});
$("#esquerda1").mouseout(function () {
    clearInterval(up);
    dir = 0;
});

// ---------------- 11,25 degrees ------------------//

$("#direita10").mousedown(function () {
    up = setInterval(function () {
        dir = 128;
        direction_send();
    }, timeout);
});
$("#direita10").mouseup(function () {
    clearInterval(up);
    dir = 0;
});
$("#direita10").mouseout(function () {
    clearInterval(up);
    dir = 0;
});

$("#esquerda10").mousedown(function () {
    up = setInterval(function () {
        dir = -128;
        direction_send();
    }, timeout);
});
$("#esquerda10").mouseup(function () {
    clearInterval(up);
    dir = 0;
});
$("#esquerda10").mouseout(function () {
    clearInterval(up);
    dir = 0;
});

// ---------------- 90 degrees --------------------//

$("#direita100").mousedown(function () {
    up = setInterval(function () {
        dir = 1024;
        direction_send();
    }, timeout);
});
$("#direita100").mouseup(function () {
    clearInterval(up);
    dir = 0;
});
$("#direita100").mouseout(function () {
    clearInterval(up);
    dir = 0;
});

$("#esquerda100").mousedown(function () {
    up = setInterval(function () {
        dir = -1024;
        direction_send();
    }, timeout);
});
$("#esquerda100").mouseup(function () {
    clearInterval(up);
    dir = 0;
});
$("#esquerda100").mouseout(function () {
    clearInterval(up);
    dir = 0;
});

// ---------------- 360 degrees --------------------//

$("#direitaFull").mousedown(function () {
    up = setInterval(function () {
        dir = 4096;
        direction_send();
    }, timeout);
});
$("#direitaFull").mouseup(function () {
    clearInterval(up);
    dir = 0;
});
$("#direitaFull").mouseout(function () {
    clearInterval(up);
    dir = 0;
});

$("#esquerdaFull").mousedown(function () {
    up = setInterval(function () {
        dir = -4096;
        direction_send();
    }, timeout);
});
$("#esquerdaFull").mouseup(function () {
    clearInterval(up);
    dir = 0;
});
$("#esquerdaFull").mouseout(function () {
    clearInterval(up);
    dir = 0;
});

// ---------------- Forward speed --------------------//

$("#cima").mousedown(function () {
    up = setInterval(function () {
        speed = 100;
        direction_send();
    }, timeout);
});
$("#cima").mouseup(function () {
    clearInterval(up);
    speed = 0;
});
$("#cima").mouseout(function () {
    clearInterval(up);
    speed = 0;
});

// ---------------- Backward speed --------------------//

$("#baixo").mousedown(function () {
    up = setInterval(function () {
        speed = -100;
        direction_send();
    }, timeout);
});
$("#baixo").mouseup(function () {
    clearInterval(up);
    speed = 0;
});
$("#baixo").mouseout(function () {
    clearInterval(up);
    speed = 0;
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
            $("#Angle").html('<p class="text">Angle: ' + Sensor.x.toFixed(2) + 'º</p>');
        },
        error: function (result) {
            alert('error');
        }
    });
}

$("body").ready(function () {

    // Funciona mas trocar por requisição GET
    // 
    readBattery = setInterval(function () {
        $.ajax({
            type: "GET",
            url: "/sensors/BatteryLevel",
            success: function (result) {
                Sensor = jQuery.parseJSON(result);
                $("#battery").html('<p class="text">Battery: ' + Sensor.BatLvL + '% </p>');
            },
            error: function (result) {
                alert('error');
            }
        });
    }, 10000);

    readTemperature = setInterval(function () {
        $.ajax({
            type: "GET",
            url: "/sensors/Temperature",
            success: function (result) {
                Sensor = jQuery.parseJSON(result);
                $("#temperature").html('<p class="text">Temperature: ' + Sensor.Temp.toFixed(2) + ' ºC</p>');
            },
            error: function (result) {
                alert('error');
            }
        });
    }, 5000);

    readSpeed = setInterval(function () {
        $.ajax({
            type: "GET",
            url: "/sensors/Speed",
            success: function (result) {
                Sensor = jQuery.parseJSON(result);
                $("#speed").html('<p class="text">Speed: ' + Sensor.Speed + ' Km/h</p>');
            },
            error: function (result) {
                alert('error');
            }
        });
    }, 1000);


});