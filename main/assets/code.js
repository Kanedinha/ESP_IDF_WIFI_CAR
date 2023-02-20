var speed = 0;
var dir = 0;
var up = 0;

var MAX_STEP = 50;

var Sensor;

var readBattery = 0;
var readTemperature = 0;
var readSpeed = 0;

var timeout = 80;

var myWebSocket;
var image_data = "";
var start_time = 0;

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
            alert('Direction Send Error');
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
                $("#battery").html('<p class="text">Battery: ' + (Sensor.BatLvL / 5 * 100).toFixed(2) + '% </p>');
            },
            error: function (result) {
                alert('Battery Level Sensor Error');
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
                alert('Temperature Sensor Error');
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
                alert('Speed Sensor Error');
            }
        });
    }, 1000);

    

});

function connectToWS() {

    var endpoint = "ws://experimento_kanedistico.local/ws";
    if (myWebSocket !== undefined) {
        myWebSocket.close()
    }

    myWebSocket = new WebSocket(endpoint);
    myWebSocket.binaryType = "arraybuffer";

    myWebSocket.onmessage = function (event) {
        var leng;
        if (event.data instanceof ArrayBuffer) {
            
            // incoming data is binary
            console.log("Received binary data" + event.data.size + " bytes");
            event.data.type = 'image/bmp';
            //var blob = new Blob([event.data], { type: 'image/bmp' });
            var blob = new Blob([event.data], { type: 'image/png' });
            const imageUrl = URL.createObjectURL(blob);
            document.getElementById("wsimg").src = imageUrl;

            // do something with the ArrayBuffer
        } else {
            // incoming data is text
            console.log("Received text data");
            // do something with the text data
        }

        /*payload = JSON.parse(event.data);

        if (payload['type'] === 'image') {
            if (payload['last_chunk']) {
                document.getElementById("wsimg").src = "data:image/bmp;base64," + image_data;
                image_data = "";
                var delta = (new Date()) - start_time;
                var fps = 1000 / delta;
                document.getElementById("fps").innerHTML = "FPS: " + fps.toFixed(2);
                var Bps = (payload['offset'] + payload['data_chunk'].length) / delta;
                document.getElementById("Bps").innerHTML = "Bps: " + Bps.toFixed(2);
                
            } else {
                if (payload['offset'] === 0) {
                    start_time = new Date();
                    image_data = payload['data_chunk'];
                }
                else {
                    image_data += payload['data_chunk'];
                }
                delete payload['msg'];

            }

        }
        if (payload['msg']) {
            console.log("onmessage. size: " + leng + ", content: " + payload['msg']);
            let endpoint = document.getElementById("myMessage").value;
            var newLine = "\r\n";
            document.getElementById("myMessage").value = endpoint + payload['msg'];
            var textarea = document.getElementById('myMessage');
            textarea.scrollTop = textarea.scrollHeight;
        }
        */

    }

    myWebSocket.onopen = function (evt) {
        console.log("onopen.");
        document.getElementById("myMessage").value = "";
    };

    myWebSocket.onclose = function (evt) {
        console.log("onclose.");
    };

    myWebSocket.onerror = function (evt) {
        console.log("Error!");
    };
}