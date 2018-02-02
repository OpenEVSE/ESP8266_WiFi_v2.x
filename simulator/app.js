/* jslint node: true, esversion: 6 */
"use strict";

const ws = require("ws");
const express = require("express");
const http = require("http");
const path = require("path");
const bodyParser = require("body-parser");

const app = express();

//
// Create HTTP server by ourselves.
//

const port = 80;

const server = http.createServer(app);

const wss = new ws.Server({
  server: server,
  path: "/ws"
});

wss.on("connection", function connection(ws) {
  ws.on("message", function incoming(message) {
    console.log("received: %s", message);
  });

  ws.send("something");
});

// Setup the static content
app.use(express.static(path.join(__dirname, "../src/data"), { index: "home.htm" }));

// Setup the API endpoints
app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ extended: true }));

app.get("/config", function (req, res) {
  res.json({ "firmware": "-", "protocol": "-", "espflash": 4194304, "version": "2.5.2.dev", "diodet": 0, "gfcit": 1, "groundt": 1, "relayt": 0, "ventt": 0, "tempt": 0, "service": 2, "scale": 220, "offset": 0, "ssid": "wibble_ext", "pass": "___DUMMY_PASSWORD___", "emoncms_enabled": false, "emoncms_server": "data.openevse.com/emoncms", "emoncms_node": "openevse", "emoncms_apikey": "", "emoncms_fingerprint": "7D:82:15:BE:D7:BC:72:58:87:7D:8E:40:D4:80:BA:1A:9F:8B:8D:DA", "mqtt_enabled": true, "mqtt_server": "home.lan", "mqtt_topic": "openevse", "mqtt_user": "emonpi", "mqtt_pass": "___DUMMY_PASSWORD___", "mqtt_solar": "", "mqtt_grid_ie": "emon/test/grid_ie", "www_username": "", "www_password": "", "ohm_enabled": false });
});
app.get("/status", function (req, res) {
  res.json({"mode":"STA","wifi_client_connected":1,"srssi":(-40 - Math.floor(Math.random() * 20)),"ipaddress":"172.16.0.191","emoncms_connected":0,"packets_sent":0,"packets_success":0,"mqtt_connected":1,"ohm_hour":"NotConnected","free_heap":20816,"comm_sent":1077,"comm_success":1075,"amp":0,"pilot":20,"temp1":247,"temp2":0,"temp3":230,"state":3,"elapsed":1079034,"wattsec":0,"watthour":54,"gfcicount":0,"nogndcount":12,"stuckcount":0,"divertmode":1});
});
app.get("/update", function (req, res) {
  res.send("<html><form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='firmware'> <input type='submit' value='Update'></form></html>");
});
app.get("/r", function (req, res) {
  var rapi = {
    "$GT": "$OK 18 0 25 23 54 27^1B",
    "$GE": "$OK 20 0229^2B",
    "$GC": "$OK 10 80^29",
    "$G3": "$OK 0^30",
    "$GH": "$OK 0^30",
    "$GO": "$OK 650 650^20",
    "$GD": "$OK 0 0 0 0^20"
  };

  var cmd = req.query.rapi;

  switch(cmd)
  {
  case "$GT":
    var date = new Date();
    var time = [
      date.getFullYear() % 100,
      date.getMonth(),
      date.getDate(),
      date.getHours(),
      date.getMinutes(),
      date.getSeconds()
    ];
    res.json({"cmd":"$GT","ret":"$OK "+time.join(" ")});
    return;
  default:
    if(rapi.hasOwnProperty(cmd)) {
      res.json({
        "cmd": cmd,
        "ret": rapi[cmd]
      });
      return;
    }
  }

  res.json({"cmd": cmd, "ret":"$NK"});
});

app.listen(port, () => console.log("Example app listening on port " + port + "!"));