var socket=io.connect();
socket.on('update',function(data){
	var i=0;
	$("li").remove();
	for (;data[i];){
		$("ol").append("<li>"+data[i]+"</li>");
		i++;
	}
});

socket.on('updateFreq',function(data){
    $("#reportTime").val(data).slider('refresh');
});

function restartStation(data){
    socket.emit('resetStation','on');
}
function reportTime(reportFreq){
    socket.emit('reportTime',reportFreq);
}