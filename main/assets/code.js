var speed = 0;
var dir = 0; 

// $(document.getElementsById("direita")).mousedown(function(){
//     dir = 50;
//     while(!(this).mouseup()){
//         direction_send();
//     }
//     dir = 30;
//     direction_send();
// });
 
$(".btn").click(function(){
    alert('click');
    if(dir == 0)
        dir = 50;
    else
        dir = 0;
    direction_send();
});


function direction_send(){
alert('send');
    direction = "X:" + dir + "Y:" + speed + "end";
    $.ajax({
        type: "POST",
        url: "/direction",
        data: JSON.stringify(directon),
        success: function(result){
            alert('success');
        },
        error: function (result) {
            alert('error');
        }
    });
}