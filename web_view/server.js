var http = require("http"), express = require('express'), sqlite3 = require('sqlite3').verbose();

var app = express();

var port = process.env.PORT || 9250;
var host = process.env.HOST || "127.0.0.1";

var dbPath = "./824Sim.db";

http.createServer(app).listen(port, host, function() {
    console.log("Server listening to %s:%d within %s environment", host, port, app.get('env'));
});

app.use(express.static('public'));

// At root of website, show main.html
app.get('/', function(req, res) {
   res.sendfile('./main.html');
});

app.get('/getAll', function(req, res) {
    var db = new sqlite3.Database(dbPath);
    var query = "SELECT * FROM NodeModel WHERE ( SELECT COUNT(*) FROM NodeModel as n WHERE n.NodeId = NodeModel.NodeId AND n.CurrentTime >= NodeModel.CurrentTime) <= 10 ORDER BY NodeId, CurrentTime DESC;";
    /*"SELECT * FROM NodeModel ORDER BY NodeId ASC, CurrentTime DESC;"*/
    db.all(query, function(err, rows) {
        if(err) throw err;
        res.json(rows);
    });
    db.close();
});

app.get('/getHistory', function(req, res) {
    var db = new sqlite3.Database(dbPath);
    var query = "SELECT * FROM NodeModel WHERE NodeId = " + req.query.nodeId + " ORDER BY CurrentTime DESC, Id DESC;";
    db.all(query, function(err, rows) {
        if(err) throw err;
        res.json(rows);
    });
    
    db.close();
});

app.get('/update', function(req, res) {
    var db = new sqlite3.Database(dbPath);
    var query = "SELECT * FROM NodeModel WHERE CurrentTime > " + req.query.maxTime + " ORDER BY CurrentTime DESC, Id DESC;";
    db.all(query, function(err, rows) {
        if(err) throw err;
        res.json(rows);
    });
    
    db.close();
});