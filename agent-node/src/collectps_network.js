
Object.defineProperty(global, '__stack', {
  get: function(){
    var orig = Error.prepareStackTrace;
    Error.prepareStackTrace = function(_, stack){ return stack; };
    var err = new Error();
    Error.captureStackTrace(err, arguments.callee);
    var stack = err.stack;
    Error.prepareStackTrace = orig;
    return stack;
  }
});

Object.defineProperty(global, '__line', {
  get: function(){
    return __stack[1].getLineNumber();
  }
});
// ---------------------------------------------

var net = require('net');
var assert = require('assert');
var MAX_UINT32 = 0x00000000FFFFFFFF;
var MAX_INT53 = 0x001FFFFFFFFFFFFF;

var msgqueueMaxSize = 5;

function uintHighLow(number) {
	return [high, low];
}


function writeUInt64BE(buf, val, offset) {
	assert(val > -1 && val <= MAX_INT53, 'val out of range');
	assert(Math.floor(val) === val, 'val must be an integer');
	var high = 0;
	var signbit = val & 0xFFFFFFFF;
	var low = signbit < 0 ? (val & 0x7FFFFFFF) + 0x80000000 : signbit;
	if (val > MAX_UINT32) {
		high = (val - low) / (MAX_UINT32 + 1);
	}
	buf.writeUInt32BE(high, offset);
	buf.writeUInt32BE(low, offset+4);
}

function hashHeaderToBuffer(l) {
	var b = new Buffer(14);
	b.writeUInt16BE(0x3, 0);
	b.writeUInt32BE(0xE, 2);

	writeUInt64BE(b, l, 6);
	return(b);
}

function stringToBuffer(str) {
	var b;
	var l;
	if(Object.prototype.toString.apply(str) !== '[object String]') {
		str = str.toString();
	}
	l = str.length;
	b = new Buffer(l+7);

	b.writeUInt16BE(0x0, 0);
	b.writeUInt32BE(6+l+1, 2);

	b.write(str, 6, l);
	b.writeUInt8(0x0, 6+l, 1);
	return(b);
}

function encodeAndSendHash(clientInfo, hash) {
	var b;
	var l = Object.keys(hash).length;

	b = hashHeaderToBuffer(l);
	clientInfo.client.write(b);

	Object.keys(hash).forEach(function(k) {
		b = stringToBuffer(k);
		clientInfo.client.write(b);
		b = stringToBuffer(hash[k]);
		clientInfo.client.write(b);
	});
}

var encodeAndSend = function(clientInfo, p) {
	for(var i=0; i<p.length; i++) {
		encodeAndSendHash(clientInfo, p[i]);
	}
};

var send = function(p) {
	var item = { ps: p };
	var itemclients = [];
	for(var i = 0; i<this.destServers.length; i++) {
			itemclients.push(false);
	}
	item.isSent = itemclients;

	if(this.msgQueue.length > msgqueueMaxSize) {
		this.msgQueue.shift();
	}
	this.msgQueue.push(item);

	this.flush();
};

var flush = function() {
	var oneNotSentYet;
	var newQueue = [];
	var p;
	this.connect();
	while(undefined !== (p = this.msgQueue.shift())) {
		oneNotSentYet = false;
		for(var i = 0; i<this.destServers.length; i++) {
			if(! p.isSent[i]) {
				if(this.destServers[i].state !== false) {
					this.encodeAndSend(this.destServers[i], p.ps);
					p.isSent[i] = true;
				} else {
					oneNotSentYet = true;
				}
			}
		}
		if(oneNotSentYet) {
			newQueue.push(p);	
		}
	}
	this.msgQueue = newQueue;
};

var connectOne = function(clientInfo) {
	clientInfo.client = new net.Socket();
	try {
		clientInfo.client.on('error', function() {
			clientInfo.client.destroy();
			clientInfo.state = false;
			console.log('Failed to connect to, or disconnected from '+clientInfo.hostname+':'+clientInfo.port);
		});
		clientInfo.client.connect(clientInfo.port, clientInfo.hostname, function() {
			clientInfo.state = true;
			console.log('Connected to '+clientInfo.hostname+':'+clientInfo.port);
		});
		clientInfo.client.on('close', function() {
			clientInfo.client.destroy();
			clientInfo.state = false;
			console.log('Disconnected from '+clientInfo.hostname+':'+clientInfo.port);

		});
	} catch(e) {
		console.log('Could not connect to '+clientInfo.hostname+':'+clientInfo.port);
		clientInfo.state = false;
	}
};

var connect = function() {
	var me = this;
	for(var i = 0; i<this.destServers.length; i++) {
		if(this.destServers[i].state === false) {
			connectOne(this.destServers[i]);
		}
	}
};

function cpsNetwork(destServers) {
	this.msgQueue = [];
	this.destServers = destServers;
	for(var i = 0; i<this.destServers.length; i++) {
		this.destServers[i].state = false; // not connected
	}

	this.encodeAndSend = encodeAndSend.bind(this);
	this.connect = connect.bind(this);
	this.send = send.bind(this);
	this.flush = flush.bind(this);
}
module.exports = cpsNetwork;

cpsNetwork.prototype.send = send;
