var speed = 0;
var dir = 0;
var btn = document.getElementById("btn");

btn.addEventListener("click", send, false)

function send(){
    var xhttp = new XMLHttpRequest();
    var upload_path = "/direction";
    xhttp.onreadystatechange = function() {
        if (xhttp.readyState == 4) {
            if (xhttp.status == 200) {
                
            } 
            else if (xhttp.status == 0) {
                alert("Server closed the connection abruptly!");
                location.reload()
            } 
            else {
                alert(xhttp.status + " Error!\n" + xhttp.responseText);
                location.reload()
            }
        }
    };
    xhttp.open("POST", upload_path, true);
    xhttp.send();
}
