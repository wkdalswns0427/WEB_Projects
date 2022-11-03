const connectButton = document.getElementById("connectButton");
const disconnectButton = document.getElementById("disconnectButton");
const colourPicker = document.getElementById("colourPicker");
const colourButton = document.getElementById("colourButton");

const connect = document.getElementById("connect");
const deviceHeartbeat = document.getElementById("deviceHeartbeat");
const deviceButtonPressed = document.getElementById("deviceButtonPressed");

let device;
connectButton.onclick = async () => {
    // try{
    //     device = await navigator.usb.requestDevice({
    //         filters: [{ vendorId: 0x0403 }]
    //       });
        
    //       await device.open();
    //       await device.selectConfiguration(1);
    //       await device.claimInterface(0);
    // }catch(e){
    //     console.error(e)
    // }
  device = await navigator.usb.requestDevice({
    filters: [{ vendorId: 0x0403 }]
  });

  await device.open();
  await device.selectConfiguration(1);
  await device.claimInterface(0);

//   connected.style.display = "block";
  connected.style.display = "block";
  connectButton.style.display = "none";
  disconnectButton.style.display = "initial";
  listen();
};

const listen = async () => {
  const result = await device.transferIn(1, 4);
  console.log(`result : ${result}`)

  const decoder = new TextDecoder();
  const message = decoder.decode(result.data);
  console.log(`message : ${message}`)

  const messageParts = message.split(" = ");
  if (messageParts[0] === "Count") {
    deviceHeartbeat.innerText = messageParts[1];
  } else if (messageParts[0] === "Button" && messageParts[1] === "1") {
    deviceButtonPressed.innerText = new Date().toLocaleString("en-ZA", {
      hour: "numeric",
      minute: "numeric",
      second: "numeric",
    });
  }
  listen();
};

// const hexToRgb = hex => {
//   const r = parseInt(hex.substring(1, 3), 16); //start at 1 to avoid #
//   const g = parseInt(hex.substring(3, 5), 16);
//   const b = parseInt(hex.substring(5, 7), 16);

//   return [r, g, b];
// };

colourButton.onclick = async () => {
  const data = new Uint8Array([0x06, 0x06, 0x06, 0x06]);
  for(var i = 1; i<17; i++){
    await device.transferOut(i, data);
  }
};

disconnectButton.onclick = async () => {
  await device.close();

  connected.style.display = "none";
  connectButton.style.display = "initial";
  disconnectButton.style.display = "none";
};

// await device.transferOut(1, data)