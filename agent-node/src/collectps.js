
var process = require('process');
process.env.ALLOW_CONFIG_MUTATIONS = 1;

var os = require('os');
var cfg = require('config');
var cpsHTTPConfig = require('./httpconfig.js');
var cpsProcess = require('./collectps_process.js');
var cpsNetwork = require('./collectps_network.js');

var cpsVersion = '<%= pkg.version %>';
var globalInterval;
var destServers = [];

var path = require('path').dirname(require.main.filename);
var hostname;

// Initialize configuration directory in the same way that node-config does.
var configDir = cfg.util.initParam('NODE_CONFIG_DIR', process.cwd() + '/config');
if (configDir.indexOf('.') === 0) {
    configDir = process.cwd() + '/' + CONFIG_DIR;
}

function get_hostname_with_case() {
    var h = cfg.has('Hostname') ? cfg.get('Hostname') : os.hostname();
    var hcase = cfg.has('HostnameCase') ? cfg.get('HostnameCase') : 'default';
    switch(hcase) {
        case 'upper': h = h.toUpperCase(); break;
        case 'lower': h = h.toLowerCase(); break;
    }
    return(h);
}

function get_cps_servers_and_ports() {
    var servers = cfg.has('Network.servers') ? cfg.get('Network.servers') : {};
    var res = [];
    for (var i in servers) {
        res.push( { hostname:servers[i].hostname, port: (servers[i].port || 28230) } );
    }
    return(res);
}

function get_interval() {
    return(cfg.has('Interval') ? (cfg.get('Interval') * 1000) : 60000);
}

/* Load the httpconfig User Interface */
if(cfg.get('HttpConfig.enable')) {
    cpsHTTPConfig.init({
            cfg: cfg,
            path: path,
            configDir: configDir,
            cpsVersion: cpsVersion,
            });
    cpsHTTPConfig.start();
}

globalInterval = get_interval();
hostname = get_hostname_with_case();
destServers = get_cps_servers_and_ports();

processInfo = new cpsProcess(globalInterval, hostname);
processSender = new cpsNetwork(destServers);

processInfo.on('processInfoAreParsed', function(processInfoList) {
    processSender.send(processInfoList);
});


// vim: set filetype=javascript fdm=marker sw=4 ts=4 et:
