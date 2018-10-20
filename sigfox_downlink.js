var express = require('express');
var Nexmo = require('nexmo');

var mongo = require('mongodb');
var assert = require('assert');

var util = require('util');
var fs = require('fs');

var app = express();

var MongoClient = require('mongodb').MongoClient;

var nexmo = new Nexmo({
  apiKey: "",
  apiSecret: ""
});

var uri = "";

app.use(express.json());

app.post('/downlink', function(request, response){

  fs.writeFileSync('request', util.inspect(request) , 'utf-8');

  console.log(request.body);
  response.send("200");

  var device = request.body.device;
  var time = request.body.time;
  var station = request.body.station;
  var rssi = request.body.rssi;
  var duplicate = request.body.duplicate;
  var data = request.body.data;

  var flag = request.body.flag;
  var lat = request.body.lat;
  var long = request.body.long;

  MongoClient.connect(uri, { useNewUrlParser: true }, function(err, client) {
    assert.equal(null, err);
    var collection = client.db('connectedBottle').collection('messages');
    collection.insertOne(
      {
        'device' : device,
        'time' : time,
        'data' : data,
        'flag' : flag,
        'lat' : lat,
        'long' : long,
        'station' : station,
        'rssi' : rssi,
        'duplicate' : duplicate
      }
    , function(err, result) {
      assert.equal(err, null);

     });
  });

  if(flag == "F") {

    MongoClient.connect(uri, { useNewUrlParser: true }, function(err, client) {
      assert.equal(null, err);
      var collection = client.db('connectedBottle').collection('devices');
      collection
      .find({'id' : device})
      .toArray(function(err, device) {
        assert.equal(err, null);
        // Sending a SMS with the fallen location
        var message = "Chute détectée (Dispositif \"" + device[0].alias + "\"). Position :" + lat + "," + long + " (https://www.google.com/maps/?q=" + lat + "," + long + ")"
        sendSMS(device[0], message);
      });
      client.close();
    });
  }

});

app.listen(3000);

function sendSMS(device, message) {
  for(var i= 0; i < device.contacts.length; i++) {
    console.log(message);
    nexmo.message.sendSms("CONNECTEDBOTTLE", device.contacts[0], message);
    console.log("sending to " + device.contacts[0]);
   }
}
