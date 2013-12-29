#!/usr/bin/env node

var app = require('http').createServer(handler)
  , io = require('socket.io').listen(app)
  , fs = require('fs')

app.listen(8081);

// variables
var child=require('child_process');
var prog=child.exec('pwd');
var freq="0.0";
var tone="0.0";
var power="-100.0";

// HTML handler
function handler (req, res)
{
	console.log('url is '+req.url.substr(1));
	reqfile=req.url.substr(1);
	reqfile="index.html"; // only allow this file for now
	fs.readFile(reqfile,
  function (err, data)
  {
    if (err)
    {
      res.writeHead(500);
      return res.end('Error loading index.html');
    }
    res.writeHead(200);
    res.end(data);
  });
}

function dds_rf(f, p)
{
	st='./dds --mode single-freq --freq '+f+' --rel-level '+p;
	console.log(st);
	prog=child.exec(st), function (error, stdout, stderr){};
}

function dds_fm(f, t, p)
{
	st='./dds --mode fmtone --freq '+f+' --tone '+t+' --rel-level '+p;
	console.log(st);
	prog=child.exec(st), function (error, stdout, stderr){};
}

// Socket.IO comms handling
// A bit over-the-top but we use some handshaking here
// We advertise message 'status stat:idle' to the browser once,
// and then wait for message such as 'freq command:12000000' or 'mode command:rf'
// We handle the message and then emit the message 'status stat:done'
io.sockets.on('connection', function (socket)
{
	socket.emit('status', {stat: 'idle'});
	
	socket.on('freq', function (data)
	{
		freq=data.command;
		console.log('freq='+freq);
	});
	
	socket.on('tone', function (data)
	{
		tone=data.command;
		console.log('tone='+tone);
	});
	
	socket.on('power', function (data)
	{
		power=data.command;
		console.log('power='+power);
	});
	
	socket.on('mode', function (data)
	{
    console.log('command='+data.command);
    // perform the desired action based on 'command':
    if (data.command=="rf")
    {
    	dds_rf(freq, power);
    	socket.emit('status', {stat: 'done'});
    	console.log('');
   	}
   	else if (data.command=="fm")
   	{
   		dds_fm(freq, tone, power);
   		socket.emit('status', {stat: 'done'});
   		console.log('');
  	}
  }); // end of socket.on('mode', function (data)

}); // end of io.sockets.on('connection', function (socket)


