const connectButton = document.getElementById("connectButton");
const disconnectButton = document.getElementById("disconnectButton");
const colourPicker = document.getElementById("colourPicker");
const colourButton = document.getElementById("colourButton");

const connect = document.getElementById("connect");
const deviceHeartbeat = document.getElementById("deviceHeartbeat");
const deviceButtonPressed = document.getElementById("deviceButtonPressed");

connectButton.onclick = async () => {
    // get a device object
    var device = new WebUSBSerialDevice({
        overridePortSettings: false,
        // these are the defaults, this config is only used if above is true
        baud: 115200,
        bits: 8,
        stop: 1,
        parity: false
    });

    // get available ports (ftdi devices)
    console.log(device.getAvailablePorts());

    // shows browser request for usb device
    var port = await device.requestNewPort();

    try {
        // try to connect, connect receives two parameters: data callback and error callback
        await port.connect((data) => {
            // this is data callback, print data to console
            console.log(data);
            // send/repeat received data back to port after 10ms
            setTimeout(()=>{
                port.send(data);
            },10);
        }, (error) => {
        // called if error receiving data
            console.warn("Error receiving data: " + error)
        });
    } catch (e) {
        // called if can't get a port
        console.warn("Error connecting to port: " + e.error);
    }
}

/*
connectButton.onclick = async () => {
  device = await navigator.usb.requestDevice({
    filters: [{ vendorId: 0x0403 }]
  });
  console.log(device.productName);
  console.log(device.manufacturerName);
  await device.open();
  await device.selectConfiguration(1);
  await device.claimInterface(0);

//   connected.style.display = "block";
  connected.style.display = "block";
  connectButton.style.display = "none";
  disconnectButton.style.display = "initial";
  listen();
};
*/
const listen = async () => {
  const result = await device.transferIn(1, 10);
  console.log(`result : ${result}`)
  const decoder = new TextDecoder;
  var message = decoder.decode(result.data);
  console.log(`message : ${message}`)

//   const messageParts = message.split(" = ");
//   if (messageParts[0] === "Count") {
//     deviceHeartbeat.innerText = messageParts[1];
//   } else if (messageParts[0] === "Button" && messageParts[1] === "1") {
//     deviceButtonPressed.innerText = new Date().toLocaleString("en-ZA", {
//       hour: "numeric",
//       minute: "numeric",
//       second: "numeric",
//     });
//   }
  listen();
};


colourButton.onclick = async () => {
  const data = new Uint8Array([0x06, 0x06, 0x06, 0x06]);
  
  await device.transferOut(2, data);
  console.log(`send : ${data}`);
};

disconnectButton.onclick = async () => {
  await device.close();
  deviceHeartbeat = 0;

  connected.style.display = "none";
  connectButton.style.display = "initial";
  disconnectButton.style.display = "none";
};

// await device.transferOut(1, data)
