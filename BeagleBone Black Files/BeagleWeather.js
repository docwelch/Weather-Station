var b=require('bonescript');
var xbee_api=require('bonescript/node_modules/xbee-api');
var C=xbee_api.constants;
var xbeeAPI=new xbee_api.XBeeAPI({
    api_mode: 2
});
var moment=require('bonescript/node_modules/moment');
var today=moment().date();
var util=require('util');
var txport = '/dev/ttyO2';
var rxport = '/dev/ttyO1';
var options = { baudrate: 9600, parser: xbeeAPI.rawParser() };
var options2= { baudrate: 9600};

b.serialOpen(rxport, options, onRxSerial);
b.serialOpen(txport, options2, onTxSerial);

function onTxSerial(x){
    console.log('tx.event= ' + x.event);
}

function onRxSerial(x){
    console.log('rx.event= '+ x.event);
}

var reportTime=20;
var changed=false;
var reset=false;

var http=require('http'),
	path=require('path'),
	fs=require('fs');
var extensions={
	'.html':'text/html',
	'.css':'text/css',
	'.js':'application/javascript',
	'.png':'image/png',
	'.gif':'image/gif',
	'.jpg':'image/jpg'
};
function requestHandler(req,res) {
	var fileName=path.basename(req.url) || 'index.html';
	var ext=path.extname(fileName);
	var localFolder=__dirname + '/public/';
	var page404=localFolder + '404.html';
		if(!extensions[ext]){
			res.writeHead(404,{'Content-Type':'text/html'});
			res.end("<html><head></head><body>The requested file type is not supported</body></html>");
		}
	getFile((localFolder + fileName),res,page404,extensions[ext]);
};

function getFile(filePath,res,page404,mimeType){
	fs.exists(filePath,function(exists){
		if(exists){
			fs.readFile(filePath,function(err,contents){
				if(!err){
					res.writeHead(200,{
						'Content-Type': mimeType,
						'Content-Length':contents.length
					});
					res.end(contents);
				}else{
					console.dir(err);
				}
			});
		}else{
			fs.readFile(page404,function(err,contents){
				if(!err){
					res.writeHead(404, {'Content-Type': 'text/html'});
					res.end(contents);
				}else{
					console.dir(err);
				}
			});
		}
	});
};
//Set the destination16 values below to match your XBee on the weather station
var midnightFrame={
	type: C.FRAME_TYPE.TX_REQUEST_16,
	destination16:'1002',
	options: 0x00,
	data: [0xFE]
}

var resetFrame={
	type: C.FRAME_TYPE.TX_REQUEST_16,
	destination16:'1002',
	options: 0x00,
	data: [0xFF]
}

var dataArray=[];
var weatherArray=[];

var server=http.createServer(requestHandler).listen(8090);
var io = require('bonescript/node_modules/socket.io').listen(server);


io.sockets.on('connection', function(socket){
    socket.emit('update',dataArray);
    socket.emit('updateFreq',reportTime);
    //Set an automatic update of the web page every 60 seconds
    setInterval(function(){
        socket.emit('update',dataArray);
    }, 60000);

    socket.on('resetStation',function(data){
        reset=true;
    });
    
    socket.on('reportTime',function(data){
        reportTime=data;
        console.log(reportTime);
        changed=true;
    });
});

xbeeAPI.on('frame_object',function(frame){
    if (frame.type==C.FRAME_TYPE.RX_PACKET_16){
        dataArray.length=0;
        weatherArray.length=0;
        var winddir=((frame.data[0]<<8)+frame.data[1]);
        weatherArray.push(winddir); //0
        dataArray.push('Wind Direction: '+winddir);
        var windspeed=(((frame.data[2]<<8)+frame.data[3])/10);
        weatherArray.push(windspeed);//1
        dataArray.push('Wind Speed: '+windspeed);
        var windgust=(((frame.data[4]<<8)+frame.data[5])/10);
        weatherArray.push(windgust);//2
        dataArray.push('Wind Gust Speed: '+windgust);
        var windgustdir=((frame.data[6]<<8)+frame.data[7]);
        weatherArray.push(windgustdir);//3
        dataArray.push('Wind Gust Direction: '+windgustdir);
        var windavg=(((frame.data[8]<<8)+frame.data[9])/10);
        weatherArray.push(windavg);//4
        dataArray.push('2 Minute Wind Speed Average: '+windavg);
        var winddiravg=((frame.data[10]<<8)+frame.data[11]);
        weatherArray.push(winddiravg);//5
        dataArray.push('2 Minute Wind Direction Average: '+winddiravg);
        var windgustten=(((frame.data[12]<<8)+frame.data[13])/10);
        weatherArray.push(windgustten);//6
        dataArray.push('10 Minute Wind Gust Speed: '+windgustten);
        var windgusttendir=((frame.data[14]<<8)+frame.data[15]);
        weatherArray.push(windgusttendir);//7
        dataArray.push('10 Minute Wind Gust Direction: '+windgusttendir);
        var humidity=(((frame.data[16]<<8)+frame.data[17])/10);
        weatherArray.push(humidity);//8
        dataArray.push('Humidity: '+humidity);
        var temp=(((frame.data[18]<<8)+frame.data[19])/10);
        weatherArray.push(temp);//9
        dataArray.push('Temperature: '+temp);
        var rain=(((frame.data[20]<<8)+frame.data[21])/100);
        weatherArray.push(rain);//10
        dataArray.push('Rain: '+rain);
        var dailyrain=(((frame.data[22]<<8)+frame.data[23])/100);
        weatherArray.push(dailyrain);//11
        dataArray.push('Daily Rain: '+dailyrain);
        var pressure=(((frame.data[24]<<8)+frame.data[25])/100);
        weatherArray.push(pressure);//12
        dataArray.push('Pressure: '+pressure);
        var dewpt=(((frame.data[26]<<8)+frame.data[27])/10);
        weatherArray.push(dewpt);//13
        dataArray.push('Dew Point: '+dewpt);
        var batt='Battery level: '+(((frame.data[28]<<8)+frame.data[29])/100);
        dataArray.push(batt);
        var light='Light Level: '+(((frame.data[30]<<8)+frame.data[31])/100);
        dataArray.push(light);
        console.log(dataArray);
        if (moment().date()!=today){
            today=moment().date();
            b.serialWrite(txport,xbeeAPI.buildFrame(midnightFrame));
        }
        if (changed){
            changed=false;
            //Change destination16 below to match your XBee on the weather station
            b.serialWrite(txport,xbeeAPI.buildFrame({
            	type: C.FRAME_TYPE.TX_REQUEST_16,
	            destination16:'1002',
	            options: 0x00,
	            data: [reportTime]}));
            console.log('***New frequency = '+reportTime+'***');
        }
        if (reset){
            reset=false;
            b.serialWrite(txport,xbeeAPI.buildFrame(resetFrame));
            console.log('**Reset Sent**');
            changed=true; //so it will update frequency
        }
        sendDataToWunderground();
    }else{
        console.log("OBJ> "+util.inspect(frame));
    }
});

function sendDataToWunderground(){
    var recordTime=moment.utc().format().replace(/\+.+/,'').replace(/T/,'+').replace(/:/g,'%3A');
    var requiredInfo='ID=YOURWEATHERSTATIONID&PASSWORD=YoUrPAssWoRD&dateutc='+recordTime+'&'//realtime=1&rtfreq=60&';
    var weatherData='humidity='+weatherArray[8]+'&tempf='+weatherArray[9]+'&baromin='+weatherArray[12]+'&dewptf='+weatherArray[13];
    weatherData+='&windir='+weatherArray[0]+'&windspeedmph='+weatherArray[1]+'&windgustmph='+weatherArray[2]+'&windgustdir='+weatherArray[3];
    weatherData+='&windspdmph_avg2m='+weatherArray[4]+'&winddir_avg2m='+weatherArray[5]+'&windgustmph_10m'+weatherArray[6]+'&windgustdir_10m='+weatherArray[7];
    weatherData+='&rainin='+weatherArray[10]+'dailyrainin='+weatherArray[11];
    var pathString='/weatherstation/updateweatherstation.php?'+requiredInfo+weatherData+'&action=updateraw&realtime=1&rtfreq='+reportTime;
    //console.log(pathString);
    http.get('http://rtupdate.wunderground.com'+pathString, function(res){
        console.log('Got response: '+res.statusCode);
    }).on('error',function(e){
        console.log('Got Error: '+e.message)
    });
}

