// WEB based SPD-500 serial program

var port =0;
var intprice = 0;

// custom crc
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

async function connectDevice(){
    const filter = {
        usbVendorId: 0x067B
        // usbVendorId: 0x0403
      };
    
    const connectButton = document.getElementById("connect");
    try {
        port = await navigator.serial.requestPort({ filters: [filter] });
        // Continue connecting to the device attached to |port|.
        await port.open({
            baudRate: 115200
        })
    } catch (e) {
        // The prompt has been dismissed without selecting a device.
        document.write("access denided.");
    }
}

async function sendConfirm(){
  const confirm = new Uint8Array([0x06,0x06,0x06,0x06]); // encode this way
  const writer = port.writable.getWriter();
  await writer.write(confirm);
  writer.releaseLock();
}

async function purchaseLoop(){
    console.log("loop")
    while (port.readable) {
        const reader = port.readable.getReader();
        while (true) {
          let value, done;
          try {
            ({ value, done } = await reader.read());
          } catch (error) {
            // Handle |error|...
            break;
          }
          if (done) {
            // |reader| has been canceled.
            break;
          }
          if (value.length > 4){
            console.log(value)
            sendConfirm();
            if(value[6] == 0){
                alert(`credit card purchace approved : ${intprice} won`);
            }
            break
          }
        }
        console.log("read done")
        reader.releaseLock();
      }

}  

function getPrice(){
    var price = document.getElementById('price').value;

    var zero = "0";
    var plength = price.length;
    intprice = price;

    // set price value to 6 digit value
    if(plength<6){
        for(var i = 0; 6-plength-i>0;i++){
            price = zero.concat(price);
        }
    }
    var STX = [0x02];
    var retcrc =[0x00, 0x08, 0xf8, 0x20, 0x02, parseInt(price.substring(0,2),16), parseInt(price.substring(2,4),16), parseInt(price.substring(4,6),16), 0x03];
    var pricecrc = crc16(retcrc);
    var clength = pricecrc.length;

    if(clength<4){
        for(var i = 0; 4-clength-i>0;i++){
            pricecrc = zero.concat(pricecrc);
        }
    }

    var fincrc = [parseInt(pricecrc.substring(0,2),16), parseInt(pricecrc.substring(2,4),16)];
    var price_protocol = STX.concat(retcrc.concat(fincrc));
    price_protocol = new Uint8Array(price_protocol);

    return price_protocol;
}

async function sendPrice(){
    price_protocol = getPrice();
    const writer = port.writable.getWriter();
    await writer.write(price_protocol);
    writer.releaseLock();
}

async function portClose(){

    await port.close();
    document.write("close");

}
