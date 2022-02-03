#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>

#include <WebSocketsServer.h>


#ifndef APSSID
#define APSSID "thermal_print"
#define APPSK  ""
#endif

WebSocketsServer webSocket = WebSocketsServer(81);

const char *page_index = R"=====(
<!DOCTYPE html>
<html>
  <head>
    <title>QR701</title>
    <style>
      body {
        background: #9a9abf;
      }
    </style>
    <script src='zuri.js'></script>
  </head>
  <body onload="my_setup()">
    <input type="file" id="myFile">Load image</input>
    <br>
    <img id="myImage" style="border:black 1px" />
    <img id="myImage2" style="border:black 2px" />
    <dir id='container'></dir>
    <input id='prnt_btn' type='button' onclick='func_print()' value='print'></input>
  </body>
</html>
)=====";

const char *js_code = R"=====(
    var canvas = document.createElement("canvas");
    var ctx = canvas.getContext("2d");
    var imageData;

    var final_d = [];
   
    var condensed_data = [];

    var server_ready = false;
    var send_data = false;
    var data_index;

    var xxx;
 
    //window.onload = function() {
    //}

    function send_chunck() {
      var data_str = "DATA ";
      if(data_index >= final_d.length) {
        console.log("no more data");
        Socket.send("END");
        document.getElementById('prnt_btn').disabled = false;
      } else {
        for(xxx= 0; (xxx< 16) && (data_index < final_d.length); ++xxx) {
          data_str += final_d[data_index++] + " ";
        }
        Socket.send(data_str);
      }
    }

  function my_setup() {
    Socket = new WebSocket('ws://' + window.location.hostname + ':81/');
    Socket.onmessage = function(event) {
      console.log(event.data);
      server_ready = true;
      if(event.data=="YES!") {
        //console.log("start sending data");
        send_data = true;
        data_index = 0;
        send_chunck();      
      } else {
        if(event.data = "RTR!") {
          send_chunck();
        } else {
          console.log(event.data);
        }
      }
    }
    Socket.onopen = function() {
      Socket.send('.');
      console.log('here');
      server_ready = true;
    }
    Socket.onclose = function() {
      console.log('died');
      server_ready = false;
      send_data = false;
    }

    document.getElementById('myFile').onchange = function (evt) {
      var tgt = evt.target || window.event.srcElement,
      files = tgt.files;
     
      // FileReader support
      if (FileReader && files && files.length) {
        var fr = new FileReader();
        fr.readAsDataURL(files[0]);
        fr.onload = () => showImage(fr);
      }
    }
  }

  function showImage(fileReader) {
        var img = document.getElementById("myImage");
        img.src = fileReader.result;
        img.onload = () => getImageData(img);
      }

      function getImageData(img) {
        document.getElementById('container').appendChild(canvas);
       
        ratio = img.height / img.width;
        new_height = Math.round(384 * ratio)
       
       
        padded_height = Math.ceil(new_height / 24) * 24;
       
        canvas.width = 384;
        canvas.height = new_height;
       
        ctx.drawImage(img, 0, 0, 384, new_height);
        imageData = ctx.getImageData(0, 0, 384, new_height);
        var i;
        for (i = 0; i < imageData.data.length; i += 4) {
          imageData.data[i+0] ^= 0xff;
          imageData.data[i+1] ^= 0xff;
          imageData.data[i+2] ^= 0xff;
          imageData.data[i+3] = 0xff;
        }
       
        // now make padded image, pad with red
        canvas.height = padded_height;
        ctx.fillStyle = 'red';
        ctx.fillRect(0, 0, 384, padded_height);      
               
        ctx.putImageData(imageData, 0, 0);
       
        console.log("384 " + padded_height);
       
        // dump new padded data
        //imageData = ctx.getImageData(0, 0, 168, padded_height);
        //console.log(imageData.data);
      }
     
      function re_format() {
        final_d = [];
        var i = 0;
       
        for(x= 0; x< condensed_data.length; x += 8) {
          mask = 0;
          if(condensed_data[x + 0]) mask |= 0x80;
          if(condensed_data[x + 1]) mask |= 0x40;
          if(condensed_data[x + 2]) mask |= 0x20;
          if(condensed_data[x + 3]) mask |= 0x10;
          if(condensed_data[x + 4]) mask |= 0x08;
          if(condensed_data[x + 5]) mask |= 0x04;
          if(condensed_data[x + 6]) mask |= 0x02;
          if(condensed_data[x + 7]) mask |= 0x01;
         
          final_d[i] = mask;
          ++i;
        }
       
        //console.log(final_d);
        if(server_ready) {
          Socket.send('print_img ' + padded_height);
        }
      }
     
      function process_24x24(img_d) {
        var d = [];
        var x1, y1;
        var s1, d2;
        s1 = 0;
        d2 = 0;
        //condensed_data = [];
        //index = 0;
        for(x1 = 0; x1< 24; ++x1) {
          d2 = x1 * 4;
          for(y1 = 0; y1 < 24; ++y1) {
            d[d2 + 0] = img_d.data[s1 + 0];
            d[d2 + 1] = img_d.data[s1 + 1];
            d[d2 + 2] = img_d.data[s1 + 2];
            d[d2 + 3] = img_d.data[s1 + 3];
           
            // condensed data
            var val = 0x00;
            if(img_d.data[d2 + 1] > 127) val = 0xff;
            condensed_data.push(val);
           
            s1 += 4;
            d2 += (24 * 4);
           
          }
        }
        for(x1 = 0; x1 <= d.length; ++x1) img_d.data[x1] = d[x1];
        return img_d;
      }

      function func_print() {  
        // reset data
        //var x1;
        //var val;
        document.getElementById('prnt_btn').disabled = true;
        final_d = [];
        condensed_data = [];
        // break into blocks 24
        var x, y;
        for(y=0; y< canvas.height; y += 24) {
          for(x=0; x< 384; x += 24) {
            // get section
            segment = ctx.getImageData(x, y, 24, 24);
            segment = process_24x24(segment);
            //ctx.putImageData(segment, x, y);
            //ctx.fillStyle = 'blue';
            //ctx.fillRect(x, y, 8, 8);
          }
        }
        re_format();
      }
)=====";

/* Set these to your desired credentials. */
const char *ssid = APSSID;
const char *password = APPSK;

int val;

uint8_t client_num;

bool socket_is_open = false;

uint8_t print_fsm = 0;

const char* print_command = "print_img ";

String data_for_printer;
uint8_t tempx;
String tempval;

int bytes_so_far;

ESP8266WebServer server(80);

/* Just a little test message.  Go to http://192.168.4.1 in a web browser
   connected to this access point to see it.
*/
void handleRoot() {
  server.send(200, "text/html", page_index);
}

void handleJS() {
  server.send(200, "text/script", js_code);
}

void handleNotFound() {
  //digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  //digitalWrite(led, 0);
}

void handleFavicon() {
  static const uint8_t gif[] PROGMEM = {
      0x47, 0x49, 0x46, 0x38, 0x37, 0x61, 0x10, 0x00, 0x10, 0x00, 0x80, 0x01,
      0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x2c, 0x00, 0x00, 0x00, 0x00,
      0x10, 0x00, 0x10, 0x00, 0x00, 0x02, 0x19, 0x8c, 0x8f, 0xa9, 0xcb, 0x9d,
      0x00, 0x5f, 0x74, 0xb4, 0x56, 0xb0, 0xb0, 0xd2, 0xf2, 0x35, 0x1e, 0x4c,
      0x0c, 0x24, 0x5a, 0xe6, 0x89, 0xa6, 0x4d, 0x01, 0x00, 0x3b
    };
    char gif_colored[sizeof(gif)];
    memcpy_P(gif_colored, gif, sizeof(gif));
    // Set the background to a random set of colors
    gif_colored[16] = random(0, 255);//millis() % 256;
    gif_colored[17] = random(0, 255);//millis() % 256;
    gif_colored[18] = random(0, 255);//millis() % 256;
    server.send(200, "image/gif", gif_colored, sizeof(gif_colored));
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

    String str = (char *) payload;
    int value;

    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
            socket_is_open = false;
            break;
        case WStype_CONNECTED:
            {
                IPAddress ip = webSocket.remoteIP(num);
                client_num = num;
                socket_is_open = true;
            }
            break;
        case WStype_TEXT:
            if(str.length() > 11) {
              if(str.substring(0, 10) == print_command) {
                value = str.substring(10).toInt();
                print_fsm = 1;
              } else {
                if(str.substring(0, 5) == "DATA ") {
                  data_for_printer = str.substring(5);
                  print_fsm = 2;
                }
              }
            } else {
              if(str.substring(0, 3) == "END") {
                print_fsm = 3;
              }
            }
            break;
        case WStype_BIN:
            break;
    }

}

void setup() {
  delay(1000);
  Serial.begin(9600);
  Serial.println();
  Serial.print("Configuring access point...");
  WiFi.softAP(ssid);

  IPAddress myIP = WiFi.softAPIP();
  //Serial.print("AP IP address: ");
  //Serial.println(myIP);
 
  socket_is_open = false;

 
  Serial.println(myIP);
  Serial.println("");
  Serial.println("");
  Serial.println("");

  server.on("/", handleRoot);
  server.on("/zuri.js", handleJS);
  server.on("/favicon.ico", handleFavicon);

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
 

  server.onNotFound(handleNotFound);
  server.begin();
}

void loop() {
  webSocket.loop();
  server.handleClient();

  if(print_fsm == 1) {
    //Serial.println("init printer!");
    //Serial.write();
    webSocket.sendTXT(client_num, "YES!");
    print_fsm = 0;
    bytes_so_far = 0;
  }
  if(print_fsm == 2) {
    //Serial.println(data_for_printer);
    if(bytes_so_far == 0) {
      //Serial.println("open header");
      Serial.write(byte(27));
      Serial.write(byte(42));
      Serial.write(byte(33));
      Serial.write(byte(128));
      Serial.write(byte(1));
    }
   
    data_for_printer.trim();
    tempval = "";
    for(tempx=0; tempx < data_for_printer.length(); ++tempx) {
      if(data_for_printer[tempx] != ' ') tempval += data_for_printer[tempx];
      else break;      
    }
    data_for_printer = data_for_printer.substring(tempx);
    data_for_printer.trim();
    Serial.write(tempval.toInt());
    //Serial.printf("%02X ", tempval.toInt());
    //delay(5);
    ++bytes_so_far;
    if(bytes_so_far >= 1152) {
      bytes_so_far = 0;
      //Serial.println(" close header");
      Serial.write(byte(29));
      Serial.write(byte(47));
      Serial.write(byte(1));
    }
    //Serial.print(' ');

    if(data_for_printer.length() == 0) {
      //Serial.println("");
      webSocket.sendTXT(client_num, "RTR!");
      print_fsm = 0;
    }
  }
  if(print_fsm == 3) {
    //Serial.println("close printer");
    print_fsm = 0;
  }
}