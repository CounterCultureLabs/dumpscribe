#!/usr/bin/env nodejs

var fs = require('fs');
var express     = require('express');
var path        = require('path');
var bodyParser  = require('body-parser');
var argv = require('minimist')(process.argv.slice(2));

function usage() {
  console.error("Usage: index.js output_from_convert_and_organize.py")
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

// Return list of notebooks in JSON
app.get('/notebooks', function(req, res){
    fs.readdir(dataDir, function(err, files) {
        if(err) {
            error(res, "failed to list files in data dir: " + err);
            return;
        }
        res.send(JSON.stringify({
            status: 'success',
            data: files.filter(function(el) {
                if(el.match(/^notebook-/)) {
                    return true;
                } else {
                    return false;
                }
            })
        }));
    });
});

// TODO request for a specific notebook and order by page or by datetime
app.get('/pdfs', function(req, res){
    res.send("not implemented");
});

// TODO request for a specific notebook and order by page or by datetime
app.get('/audio', function(req, res){
    res.send("not implemented");
});


app.post('/name_notebook', function(req, res){
    res.send("not implemented");
});


app.listen(3000);
