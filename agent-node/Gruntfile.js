
module.exports = function(grunt) {
  var appendGenericBanner = function (content, srcpath) {
    content = '/*! <%= pkg.name %> - v<%= pkg.version %> - '
            + 'Built <%= grunt.template.today("yyyy-mm-dd") %> */'
            + '\n\n'
            + content;
    return(grunt.template.process(content));
  };

  var pkg = grunt.file.readJSON('package.json');
  // Project configuration.
  grunt.initConfig({
    pkg: pkg,
  clean: ['build/frontend', 'build', pkg.name+'-'+pkg.version+'.exe'],
  jshint: {
      options: {
        'node': true,
        'esnext': true,
        'curly': false,
        'smarttabs': true,
        'indent': 2,
        'quotmark': 'single',
        'laxbreak': true,
        'globals': {
          'jQuery': true
        }
      },
      all: ['Gruntfile.js', 'src/*.js']
    },
  shell: {
    makensis: {
      command: 'makensis /NOCD build\\collectps.nsi'
    }
  },
  copy: {
    node: {
      files: [
          { src: 'bin/node-0.10.32-x64.exe', dest: 'build/node.exe', },
          { src: 'bin/node-0.10.32-x86.exe', dest: 'build/node32.exe', },
          { src: 'bin/node-0.10.32-x64.exe', dest: 'build/node64.exe', },
        ]
    },
    sources: {
      options: {
        process: appendGenericBanner,
      },
      files: [
          { src: 'src/collectps.js', dest: 'build/collectps.js', },
          { src: 'src/collectps_process.js', dest: 'build/collectps_process.js', },
          { src: 'src/collectps_network.js', dest: 'build/collectps_network.js', },
          { src: 'src/httpconfig.js', dest: 'build/httpconfig.js', },
          { src: 'src/service.js', dest: 'build/service.js', },
        ]
    },
    collectps_nsi: {
      src: 'src/collectps.nsi',
      dest: 'build/collectps.nsi',
      options: {
        process: function (content, srcpath) {
		  content = content.replace(/ *\!define PROJECTNAME +".*" */g, '!define PROJECTNAME "'+pkg.name+'"');
		  content = content.replace(/ *\!define VERSION +".*" */g, '!define VERSION "'+pkg.version+'"');
          return(content);
        }
      }
    },
    frontend: {
      src: 'frontend/*',
      dest: 'build/',
    },
  },
  });

  grunt.loadNpmTasks('grunt-contrib-clean');
  grunt.loadNpmTasks('grunt-contrib-jshint');
  grunt.loadNpmTasks('grunt-contrib-copy');
  grunt.loadNpmTasks('grunt-shell');

  // Default task(s).
  grunt.registerTask('distexe', ['copy:collectps_nsi', 'copy:sources', 'copy:node', 'shell:makensis']);
  grunt.registerTask('test', ['jshint', 'copy:sources', 'copy:frontend']);
  grunt.registerTask('default', ['jshint']);

};

