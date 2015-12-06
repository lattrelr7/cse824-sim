var http = require("http"), express = require('express'), sqlite3 = require('sqlite3').verbose();

var app = express();

var port = process.env.PORT || 9250;
var host = process.env.HOST || "127.0.0.1";

http.createServer(app).listen(port, host, function() {
    console.log("Server listening to %s:%d within %s environment", host, port, app.get('env'));
});

app.use(express.static('public'));

// At root of website, show main.html
app.get('/', function(req, res) {
   res.sendfile('./main.html');
});

app.get('/update', function(req, res) {
    var db = new sqlite3.Database('./824Sim.db');
    db.all("SELECT * FROM NodeModel ORDER BY NodeId ASC, CurrentTime DESC;", function(err, rows) {
        if(err) throw err;
        res.json(rows);
    });
    db.close();
});