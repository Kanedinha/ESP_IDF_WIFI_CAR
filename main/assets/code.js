var speed = 0;
var dir = 0;
var up = 0;

var MAX_STEP = 50;

var Sensor;

var readBattery = 0;
var readTemperature = 0;
var readSpeed = 0;
var videoReq = 0;

var timeout = 80;

var myWebSocket = undefined;
var image_data = "";
var start_time = 0;

var width = 320;
var height = 240;

var last_time = 0;
var now_time = 0;

// ---------------- ~1 degree --------------------//
$("#direita1").mousedown(function () {
    up = setInterval(function () {
        dir = 1;
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
        dir = -1;
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

// ---------------- WEB Socket ---------------------//

$("#wsConBtn").on('click', function () {
    connectToWS();
});

$("#wsDiscBtn").on('click', function () {
    myWebSocket.close();
});

//------------

$("#Photo").on('click', function () {
    $.ajax({
        type: "GET",
        xhrFields: {
            responseType: 'blob'
        },
        url: "/camera",
        success: function (data) {
            const url = window.URL || window.webkitURL;
            const src = url.createObjectURL(data);
            $("#wsVideo").attr('src', src);
        },
        error: function (result) {
            console.log('failed to get image');
        }
    });
})

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
            console.log('Direction Send Error');
        }
    });
}


$("body").ready(function () {

    // Funciona mas trocar por requisição GET

    readBattery = setInterval(function () {
        $.ajax({
            type: "GET",
            url: "/sensors/BatteryLevel",
            success: function (result) {
                Sensor = jQuery.parseJSON(result);
                $("#battery").html('<p class="text">Battery: ' + (Sensor.BatLvL / 5 * 100).toFixed(2) + '% </p>');
            },
            error: function (result) {
                console.log('Battery Level Sensor Error');
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
                console.log('Temperature Sensor Error');
            }
        });
    }, 1000);

    // readSpeed = setInterval(function () {
    //     $.ajax({
    //         type: "GET",
    //         url: "/sensors/Speed",
    //         success: function (result) {
    //             console.log(result);
    //             Sensor = jQuery.parseJSON(result);
    //             $("#speed").html();
    //         },
    //         error: function (result) {
    //             console.log('Speed Sensor Error');
    //         }
    //     });
    // }, 1000);

});

function rgb565ToCanvas(rgb565Data, width, height) {
    
    const canvas = document.getElementById('wsVideo');
    canvas.width = width;
    canvas.height = height;
    
    // Get the 2D context of the canvas
    const ctx = canvas.getContext('2d');
    
    const imageData = context.createImageData(width, height);
    for (let i = 0; i < width * height; i++) {
      const rgb565 = (rgb565Data[i * 2] << 8) | rgb565Data[i * 2 + 1];
      const r = (rgb565 >> 11) & 0x1F;
      const g = (rgb565 >> 5) & 0x3F;
      const b = rgb565 & 0x1F;
      imageData.data[i * 4] = Math.round(r * 255 / 31);
      imageData.data[i * 4 + 1] = Math.round(g * 255 / 63);
      imageData.data[i * 4 + 2] = Math.round(b * 255 / 31);
      imageData.data[i * 4 + 3] = 255;
    }
    
    // Draw the ImageData onto the canvas
    ctx.putImageData(imgData, 0, 0);
}


function connectToWS() {

    var ws_protocol = window.location.protocol === 'https:' ? 'wss' : 'ws';
    var ws_address = ws_protocol + '://' + window.location.hostname + "/ws";
    var endpoint = ws_address;

    if (myWebSocket !== undefined) {
        myWebSocket.close()
    }

    myWebSocket = new WebSocket(endpoint);
    myWebSocket.binaryType = "arraybuffer";

    myWebSocket.onmessage = function (event) {


        var urlObj = window.URL || window.webkitURL;
        var blob = new Blob([event.data], { type: 'image/jpeg' });
        var imgUrl = urlObj.createObjectURL(blob);

        document.getElementById('wsVideo').src = imgUrl;


        // // Verificar se os dados recebidos são da imagem RGB565
        // if (event.data.byteLength % 2 !== 0) {
        //     // Os dados não são da imagem RGB565
        //     return;
        // }

        // // Converter os dados para um array de bytes
        // const byteArray = new Uint8Array(event.data);

        // // Processar a imagem RGB565
        // rgb565ToCanvas(byteArray, width, height);

        const now_time = performance.now();
        const elapsed = now_time - last_time;
        console.log("Fps: " + 1000 / elapsed);
        last_time = now_time;

    };

    myWebSocket.onopen = function (evt) {
        console.log("onopen.");
    };

    myWebSocket.onclose = function (evt) {
        console.log("onclose.");
    };

    myWebSocket.onerror = function (evt) {
        console.log("Error!");
    };
}


