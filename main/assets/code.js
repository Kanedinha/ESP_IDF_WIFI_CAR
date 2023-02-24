var speed = 0;
var dir = 0;
var up = 0;

var MAX_STEP = 50;

var Sensor;

var readBattery = 0;
var readTemperature = 0;
var readSpeed = 0;
var videoReq;

var timeout = 80;

var myWebSocket = undefined;
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

// ---------------- WEB Socket ---------------------//

$("#wsConBtn").on('click',function(){
    connectToWS();
});

$("#wsDiscBtn").on('click',function(){
    myWebSocket.close();
});

$("#Photo").on('click', function(){
    $.ajax({
        type: "GET",
        url: "/camera",
        success: function (result) {
            $("#wsVideo").attr('src', result);
        },
        error: function (result) {
            console.log('Speed Sensor Error');
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
    // 
    // readBattery = setInterval(function () {
    //     $.ajax({
    //         type: "GET",
    //         url: "/sensors/BatteryLevel",
    //         success: function (result) {
    //             Sensor = jQuery.parseJSON(result);
    //             $("#battery").html('<p class="text">Battery: ' + (Sensor.BatLvL / 5 * 100).toFixed(2) + '% </p>');
    //         },
    //         error: function (result) {
    //             console.log('Battery Level Sensor Error');
    //         }
    //     });
    // }, 10000);

    // readTemperature = setInterval(function () {
    //     $.ajax({
    //         type: "GET",
    //         url: "/sensors/Temperature",
    //         success: function (result) {
    //             Sensor = jQuery.parseJSON(result);
    //             $("#temperature").html('<p class="text">Temperature: ' + Sensor.Temp.toFixed(2) + ' ºC</p>');
    //         },
    //         error: function (result) {
    //             console.log('Temperature Sensor Error');
    //         }
    //     });
    // }, 5000);

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

function getCoordinatesFromArrayBuffer(buffer) {
    // Create a new DataView object with the given buffer
    const dataView = new DataView(buffer);

    // Get the x and y positions from the DataView using the correct byte offsets
    const seq = dataView.getUint32(0, /* littleEndian = */ true);
    const x = dataView.getUint32(4, /* littleEndian = */ true);
    const y = dataView.getUint32(8, /* littleEndian = */ true);
    const width = dataView.getUint32(12, /* littleEndian = */ true);
    const height = dataView.getUint32(16, /* littleEndian = */ true);
    const pairs = dataView.getUint32(20, /* littleEndian = */ true);

    // Return an object with the extracted coordinates
    return { seq, x, y, width, height, pairs };
}

function connectToWS() {

    var ws_protocol = window.location.protocol === 'https:' ? 'wss' : 'ws';
    var ws_address = ws_protocol + '://' + window.location.hostname + "/ws";
    var endpoint = ws_address;

    if (myWebSocket !== undefined) {
        myWebSocket.close()
    }

    myWebSocket = new WebSocket(endpoint);
    // myWebSocket.binaryType = "arraybuffer";

    // myWebSocket.onmessage = function (event) {
    //     if (event.data instanceof ArrayBuffer) {
    //         coords = getCoordinatesFromArrayBuffer(event.data);
    //         // incoming data is binary
    //         const rle_data = new Uint8Array(event.data.slice(24));
    //         const offscreenCanvas = new OffscreenCanvas(coords.width, coords.height);
    //         const offscreenCtx = offscreenCanvas.getContext('2d');
    //         const image_data = offscreenCtx.createImageData(coords.width, coords.height);
    //         if (coords.seq !== prev_seq + 1) {

    //             msg = `Missing seq. prev_sec:${prev_seq} -> seq: ${coords.seq}, x: ${coords.x}, y: ${coords.y}, width: ${coords.width}, height: ${coords.height}, pairs: ${coords.pairs}`;
    //             console.log(msg);

    //             if (myWebSocket && prev_seq>-1) {
    //                 // Package the coordinates into a binary message
    //                 const message = new ArrayBuffer(8);
    //                 const view = new DataView(message);
    //                 view.setUint16(0, 0, true);
    //                 view.setUint16(2, 0, true); // not used here
    //                 view.setUint16(4, 0, true); // not used here

    //                 // Send the message over the WebSocket
    //                 myWebSocket.send(message);
    //             }
    //         }
    //         prev_seq = coords.seq;
    //         {
    //             let offset = 0;
    //             for (let i = 0; i < coords.pairs; i++) {
    //                 const pair_offset = i * 4;
    //                 const r = rle_data[pair_offset];
    //                 const g = rle_data[pair_offset + 1];
    //                 const b = rle_data[pair_offset + 2];
    //                 const count = rle_data[pair_offset + 3];
    //                 //console.log(`[${i}] r: ${r}, g: ${g}, b: ${b}, count: ${count} offset: ${offset}`);
    //                 for (let j = 0; j < count; j++) {
    //                     image_data.data[offset++] = r;
    //                     image_data.data[offset++] = g;
    //                     image_data.data[offset++] = b;
    //                     image_data.data[offset++] = 255;
    //                 }
    //             }
    //             offscreenCtx.putImageData(image_data, 0, 0);
    //             const canvas = document.getElementById("wsVideo");
    //             const ctx = canvas.getContext("2d");
    //             //draw offscreen canvas to main canvas
    //             ctx.drawImage(offscreenCanvas, coords.x, coords.y);
    //         }
    //     } else {
    //         // incoming data is text
    //         console.log("Received text data");
    //         // do something with the text data
    //     }
    // }

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

socket.addEventListener('message', (event) => {
    console.log('Message from server ', event.data);
});