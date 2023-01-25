var speed = 0;
var dir = 0;


$(document.getElementsById("esquerda")).mousedown(function(){
    dir = 0;
    while(!(this).mouseup()){
        direction_send();
    }
    dir = 30;
    direction_send();
});


$(document.getElementsById("direita")).mousedown(function(){
    dir = 50
    while(!(this).mouseup()){
        direction_send();
    }
    dir = 30;
    direction_send();
});

$(document.getElementsById("cima")).mousedown(function(){
    speed = 100;
    while(!(this).mouseup()){
        direction_send();
    }
    speed = 0;
    direction_send();
});

$(document.getElementsById("baixo")).mousedown(function(){
    speed = -100;
    while(!(this).mouseup()){
        direction_send();
    }
    speed = 0;
    direction_send();
});

$(document).ready(function(){
    setInterval( function(){
    
    }, 1000);
});

function direction_send(){

    direction = "X:" + dir + "Y:" + speed + "end";
    $.ajax({
        type: "POST",
        url: "/direction",
        data: JSON.stringify(directon),
        success: function(result){},
        error: function (result) {
            alert('error');
        }
    });
}