(function() {
    'use strict';
  
    document.addEventListener('DOMContentLoaded', event => {
      let connectButton = document.querySelector("#connect");
      let statusDisplay = document.querySelector('#status');
      let sendPricebtn = document.querySelector('#pricebtn');
      let port;
      var intprice = 0;
      var price_protocol;
  
      // function addLine(linesId, text) {
      //   var senderLine = document.createElement("div");
      //   senderLine.className = 'line';
      //   var textnode = document.createTextNode(text);
      //   senderLine.appendChild(textnode);
      //   document.getElementById(linesId).appendChild(senderLine);
      //   return senderLine;
      // }
  
      // let currentReceiverLine;
  
      // function appendLines(linesId, text) {
      //   const lines = text.split('\r');
      //   if (currentReceiverLine) {
      //     currentReceiverLine.innerHTML =  currentReceiverLine.innerHTML + lines[0];
      //     for (let i = 1; i < lines.length; i++) {
      //       currentReceiverLine = addLine(linesId, lines[i]);
      //     }
      //   } else {
      //     for (let i = 0; i < lines.length; i++) {
      //       currentReceiverLine = addLine(linesId, lines[i]);
      //     }
      //   }
      // }

      
      function connect() {
        port.connect().then(() => {
          statusDisplay.textContent = '';
          connectButton.textContent = 'Disconnect';
  
          port.onReceive = data => {
            let textDecoder = new TextDecoder();
            console.log(textDecoder.decode(data));
            // if (data.getInt8() === 13) {
            //   currentReceiverLine = null;
            // } else {
            //   appendLines('receiver_lines', textDecoder.decode(data));
            // }
          };
          port.onReceiveError = error => {
            console.error(error);
          };
        }, error => {
          statusDisplay.textContent = error;
        });
      }
  
      connectButton.addEventListener('click', function() {
        if (port) {
          port.disconnect();
          connectButton.textContent = 'Connect';
          statusDisplay.textContent = '';
          port = null;
        } else {
          serial.requestPort().then(selectedPort => {
            port = selectedPort;
            connect();
          }).catch(error => {
            statusDisplay.textContent = error;
          });
        }
      });
  
      serial.getPorts().then(ports => {
        if (ports.length === 0) {
          statusDisplay.textContent = 'No device found.';
        } else {
          statusDisplay.textContent = 'Connecting...';
          port = ports[0];
          connect();
        }
      });
  
  
      // let commandLine = document.getElementById("command_line");
      
      // // send the key encoded in utf-8 when key is pressed
      // commandLine.addEventListener("keypress", function(event) {
      //   if (event.keyCode === 13) {
      //     if (commandLine.value.length > 0) {
      //       addLine('sender_lines', commandLine.value);
      //       commandLine.value = '';
      //     }
      //   }
        
      //   port.send(new TextEncoder('utf-8').encode(String.fromCharCode(event.which || event.keyCode)));
      // });

      function crc16(data, offset = 0) {
        var crc, length;
        length = data.length;
    
        if (data === null || offset < 0 || offset > data.length - 1 && offset + length > data.length) {
          return 0;
        }
        crc = 0;
        for (var i = 0, _pj_a = length; i < _pj_a; i += 1) {
          crc ^= data[i];
      
          for (var j = 0, _pj_b = 8; j < _pj_b; j += 1) {
            if ((crc & 1) > 0) {
              crc = (crc >> 1) ^ 40961;
            } else {
              crc = crc >> 1;
            }
          }
        }
        return crc.toString(16);
      }
    
      function getPrice(){
        // read price vale in won from input at click
        var price = document.getElementById('price').value;
    
        var zero = "0";
        var plength = price.length;
        intprice = price;
    
        // set price value to 6 digit value (ex. 1000 won --> 001000 / 2500 won --> 002500 )
        if(plength<6){
            for(var i = 0; 6-plength-i>0;i++){
                price = zero.concat(price);
            }
        }
    
        // packet structure refer to datasheet
        var STX = [0x02];
        var retcrc =[0x00, 0x08, 0xf8, 0x20, 0x02, parseInt(price.substring(0,2),16), parseInt(price.substring(2,4),16), parseInt(price.substring(4,6),16), 0x03];
        var pricecrc = crc16(retcrc);
        var clength = pricecrc.length;
        
        // set crc value to hex 4 digits
        if(clength<4){
            for(var i = 0; 4-clength-i>0;i++){
                pricecrc = zero.concat(pricecrc);
            }
        }
    
        // divide CRC value to CRC_H, CRC_L
        var fincrc = [parseInt(pricecrc.substring(0,2),16), parseInt(pricecrc.substring(2,4),16)];
    
        // reconfigure packet to send in format
        var price_protocol = STX.concat(retcrc.concat(fincrc));
        price_protocol = new Uint8Array(price_protocol);
    
        return price_protocol;
      }
        
        // send price to SPD500
      function sendPrice(){
          price_protocol = getPrice();
          port.send(new TextEncoder('utf-8').encode(price_protocol));
          alert(price_protocol)
      }

      sendPricebtn.addEventListener('click', function() {
        sendPrice();
      });
    });
  })();
  
  