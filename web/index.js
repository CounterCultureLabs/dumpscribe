#!/usr/bin/env nodejs

var fs = require('fs');
var util = require('util');
var sanitize = require("sanitize-filename");
var async = require('async');
var express     = require('express');
var path        = require('path');
var bodyParser  = require('body-parser');
var argv = require('minimist')(process.argv.slice(2));
var settings = require('./settings.js')

var port = argv.port || settings.port || 3000;

function usage() {
  console.error("Usage: index.js unmuddle_output_dir")
}

function error(res, msg) {
  res.status(500).send(JSON.stringify({
      status: 'error',
      msg: msg
  }));
}

if(argv._.length < 1) {
  usage();
  process.exit(1);
}

var dataDir = argv._[0];

var app = express();

app.use(express.static(path.join(__dirname, 'static')));

var pendataMount = 'pendata';
app.use('/' + pendataMount, express.static(dataDir));

app.use(bodyParser.urlencoded({
  extended: true
}));

function getNotebookName(id, callback) {
    var name = null;
    var nbDirname = 'notebook-'+id;
    var nbDir = path.join(dataDir, nbDirname);

    fs.readFile(path.join(nbDir, "notebook_name"), function(err, nbname) {
        if(!err) {
            name = nbname.toString('utf8').replace(/[\n\r\t]+/, '');
        }
        callback(null, name);
    });
}

function getNotebookPages(id, callback) {
    var pages = {};
     
    var m;
    var nbDirname = 'notebook-'+id;
    var nbDir = path.join(dataDir, nbDirname);

    fs.readdir(nbDir, function(err, nbfiles) {
        if(err) return callback(err);

        // for each file in the notebook directory
        async.eachSeries(nbfiles, function(nbfile, fcallback) {
            // for pdf files
            if(nbfile.match(/\.pdf$/)) {
                m = nbfile.match(/page-(\d+)/);
                if(!m) return fcallback();
                var number = parseInt(m[1]);
                fs.stat(path.join(nbDir, nbfile), function(err, stats) {
                    if(err) return fcallback(err);

                    if(!pages[number]) {
                        pages[number] = {
                            recordings: []
                        };
                    }
                    pages[number].pdf = pendataMount + '/' + nbDirname + '/' + nbfile;
                    pages[number].thumbnail = pendataMount + '/' + nbDirname + '/thumbnails/' + nbfile + '.png';
                    pages[number].number = number;
                    pages[number].date = stats.mtime;
                    pages[number].size = stats.size;
                    fcallback();
                });
                
            // for audio files
            } else if(nbfile.match(/\.ogg$/) || nbfile.match(/\.aac$/)) {
                m = nbfile.match(/page-(\d+)-.*(\d+)\./);
                if(!m) return fcallback();
                number = parseInt(m[1]);
                duration = parseInt(m[2]);
                
                fs.stat(path.join(nbDir, nbfile), function(err, stats) {
                    if(err) return fcallback(err);

                    var recording = {
                        path: pendataMount + '/' + nbDirname + '/' + nbfile,
                        date: stats.mtime,
                        size: stats.size,
                        duration: duration
                    };

                    if(!pages[number]) {
                        pages[number] = {
                            number: number,
                            recordings: [recording]
                        }
                    } else {
                        pages[number].recordings.push(recording);
                    }
                    
                    fcallback();
                });
            } else {
                fcallback();
            }
        }, function(err) {
            if(err) return callback(err);
            callback(null, pages);
        });
    });
}

function getNotebookSummary(basePath, nbdirs, callback) {

    data = {};

    async.eachSeries(nbdirs, function(nbdir, nbcallback) {
        nbpath = path.join(basePath, nbdir);
        var id = nbdir.replace(/^notebook-/, '');
        data[nbdir] = {
            id: id,
            dirname: nbdir,
            name: id,
            date: null,
            pages: [],
            audio: []
        };

        fs.readdir(nbpath, function(err, nbfiles) {
            if(err) return nbcallback(err);
            
            fs.readFile(path.join(nbpath, "notebook_name"), function(err, nbname) {
                if(!err) {
                    data[nbdir].name = nbname.toString('utf8').replace(/[\n\r\t]+/, '');
                }

                async.eachSeries(nbfiles, function(nbfile, fcallback) {
                    if(nbfile.match(/\.pdf$/)) {
                        if(nbfile == 'all_pages.pdf') {
                            return fcallback();
                        }
                        data[nbdir].pages.push(nbfile);

                        fs.stat(path.join(nbpath,  nbfile), function(err, stats) {
                            if(err) return fcallback(err);

                            if(!data[nbdir].date || (stats.mtime > data[nbdir].date)) {
                                data[nbdir].date = stats.mtime;
                            }
                            fcallback();
                        });
                    } else if(nbfile.match(/\.ogg$/) || nbfile.match(/\.aac$/)) {
                        data[nbdir].audio.push(nbfile);
                        fcallback();
                    } else {
                        fcallback();
                    }
                }, function(err) {
                    if(err) return nbcallback(err);
                    nbcallback();
                });
            });
        });

    }, function(err) {
        if(err) return callback(err);
        callback(null, data);
    });
    
}

function change_notebook_name(id, name, callback) {
    
    var dirname = sanitize('notebook-'+id);
    var nbpath = path.join(dataDir, dirname);

    if((name.length < 1) || (name.length > 256)) {
        return callback("Name is too long. Must be between 1 and 256 characters.")
    }

    fs.writeFile(path.join(nbpath, "notebook_name"), name, function(err, nbname) {
        if(!err) {
            return callback(err);
        }
        callback(name);
    });
}

// Return list of notebooks in JSON
app.get('/notebooks', function(req, res){
    fs.readdir(dataDir, function(err, files) {
        if(err) return error(res, "failed to list files in data dir: " + err);
        files = files.filter(function(el) {
            if(el.match(/^notebook-/)) {
                return true;
            } else {
                return false;
            }
        });
        
        getNotebookSummary(dataDir, files, function(err, data) {
            if(err) return error(res, "Failed to get notebook list: " + err);

            res.send(JSON.stringify({
                status: 'success',
                data: data
            }));
        });
    });
});


function orderPages(pages, order_by) {
    var arr = [];
    var key;
    for(key in pages) {
        arr.push(pages[key]);
    }
    arr.sort(function(a, b) {
        return b[order_by] - a[order_by];
    });
    return arr;
}

app.use('/notebook/:id', function(req, res, next){
    var id = req.params.id;
    var order = req.query.order || 'pagenumber'; // can also be 'date'

    getNotebookPages(id, function(err, pages) {
        if(err) return error(res, "Failed to get notebook page info: " + err);

        if(order == 'date') {
            pages = orderPages(pages, 'date');
        } else {
            pages = orderPages(pages, 'pagenumber');
        }

        getNotebookName(id, function(err, name) {

            res.send(JSON.stringify({status: 'success', data: {
                name: name,
                id: id,
                pages: pages,
                pdf: pendataMount + '/notebook-' + id + '/all_pages.pdf'
            }}));
            next();
        });
    });
});

app.use('/notebook-change-name/:id', function(req, res, next){
    var id = req.params.id;
    console.log(req.body);
    if(!id || !req.body.name || !req.body.password) {
        return error(res, "Missing id, password or new notebook name");
    }

    if(req.body.password != settings.admin_password) {
        return error(res, "Wrong password. Hint: The password is in the file web/server.js on the server");
    }

    change_notebook_name(id, req.body.name, function(err, new_name) {
        if(err) {
            return error(res, "Error changing notebook name: " + err);
        }

        res.send(JSON.stringify({status: 'success', data: {name: new_name}}));
        next();
    });
});


// TODO request for a specific notebook and order by page or by datetime
app.get('/audio', function(req, res){
    res.send("not implemented");
});


app.post('/name_notebook', function(req, res){
    res.send("not implemented");
});

console.log("Listening on http://localhost:" + port + "/")

app.listen(port);
