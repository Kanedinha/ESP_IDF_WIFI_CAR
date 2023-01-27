var speed = 0;
var dir = 0;
var up = 0;

$("#direita").mousedown(function () {
    up = setInterval(function () {
        if (dir < 70)
            dir += 1;
        direction_send();
    }, 50);
});

$("#direita").mouseup(function () {
    clearInterval(up);
});

$("#esquerda").mousedown(function () {
    up = setInterval(function () {
        if (dir > 0)
            dir -= 1;
        direction_send();
    }, 50);
});

$("#esquerda").mouseup(function () {
    clearInterval(up);
});

function direction_send() {
    direction = { X: dir, Y: speed };
    $.ajax({
        type: "POST",
        url: "/direction",
        data: JSON.stringify(direction),
        success: function (result) { },
        error: function (result) {
            alert('error');
        }
    });
}