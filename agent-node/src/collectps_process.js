
var spawn = require('child_process').spawn;
var EventEmitter = require('events').EventEmitter;
var util = require('util');

var currentProcess = {};


var pscmd = 
    'Get-WmiObject CIM_Process | foreach {'
        +'try { $owner=$_.getowner(); } catch { $owner.domain = ""; $owner.user = ""; }'
        +'"==",'
        +'"Name=$($_.name)=",'
        +'"PID=$($_.processID)=",'
        +'"VirtualSize=$($_.VirtualSize)=",'
        +'"User=$($owner.user)\\$($owner.domain)=",'
        +'"ThreadCount=$($_.threadcount)=",'
        +'"stime=$($_.kernelmodetime)=",'
        +'"utime=$($_.usermodetime)=",'
        +'"cmd=$($_.commandline)=" '
        +'| out-string -stream}';

var sendProcessInfo = function () {
    if(this.processInfoQueue.length === 0) {
        return;
    }
    this.emit('processInfoAreParsed', this.processInfoQueue);
    this.processInfoQueue = [];
};

var appendCurrentProcess = function (p, now) {
    p.cdtime = now;
    p.hostname = this.hostname;
    this.processInfoQueue.push(p);
};

var parseData = function (str, flush, now) {
    var line;
    var n;
    var pos;
    
    line = str.split('=\r\n');
    n = line.length;

    if(n === 0) {
        return(str);
    }
    if(!flush) {
        n -= 1;
    }

    if(n > 0) {
        for(i=0; i<n; i++) {
            line[i] = line[i].replace(/\r?\n|\r/g, '');
            if(line[i] == '=') {
                this.appendCurrentProcess(currentProcess, now);
                currentProcess = {};
            } else {
                pos = line[i].indexOf('=');
                if(pos > 0) {
                    currentProcess[line[i].substring(0,pos)] = line[i].substring(pos+1);
                }
            }
        }
    }
    if(flush) {
        if(currentProcess.length > 0) {
            this.appendCurrentProcess(currentProcess, now);
            currentProcess = {};
        }
        return('');
    }
    return(line[n]);
};

var getInfo = function () {
    var me = this;
    var prevLine = '';
    var now = new Date().getTime();

    if(this.powershell) {
        return;
    }
    this.powershell = spawn('Powershell.exe', [ '-Command', '& {'+pscmd+'}' ]);
    
    this.powershell.stdin.end();
    this.powershell.stdout.on('data', function (data) {
        prevLine = me.parseData(prevLine + data.toString(), 0, now);
    });

    this.powershell.stdout.on('end', function () { prevLine = me.parseData(prevLine, 1, now); me.sendProcessInfo(); });
    this.powershell.stdout.on('close', function () { prevLine = me.parseData(prevLine, 1, now); me.sendProcessInfo(); });
    this.powershell.stdout.on('error', function () { prevLine = me.parseData(prevLine, 1, now); me.sendProcessInfo(); });

    this.powershell.stderr.on('data', function (data) {
            console.error(data.toString());
            });

    this.powershell.on('close', function (code) { prevLine = me.parseData(prevLine, 1, now); me.sendProcessInfo(); me.powershell = undefined; });
    this.powershell.on('exit', function (code) { prevLine = me.parseData(prevLine, 1, now); me.sendProcessInfo(); me.powershell = undefined;  });
    this.powershell.on('error', function (code) { prevLine = me.parseData(prevLine, 1, now); me.sendProcessInfo(); me.powershell.kill(); me.powershell = undefined; });

};

function cpsProcess(interval, hostname) {
    EventEmitter.call(this);

    this.interval = interval || 10000;
    this.hostname = hostname;
    this.processInfoQueue = [];
    this.powershell = undefined;

    this.getInfo = getInfo.bind(this);
    this.parseData = parseData.bind(this);
    this.appendCurrentProcess = appendCurrentProcess.bind(this);
    this.sendProcessInfo = sendProcessInfo.bind(this);
    this.getInfo();
    setInterval(this.getInfo, this.interval);
}
util.inherits(cpsProcess, EventEmitter);
module.exports = cpsProcess;


// vim: set filetype=javascript fdm=marker sw=4 ts=4 et:
