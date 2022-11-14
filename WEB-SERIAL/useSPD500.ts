import { useEffect, useRef, useState } from 'react';
import { isSPD } from '../../app-config';

// CRC16 generator
function crc16(data: number[], offset = 0) {
  let crc;
  const length = data.length;

  if (data === null || offset < 0 || (offset > data.length - 1 && offset + length > data.length)) {
    return '0';
  }
  crc = 0;
  for (let i = 0, _pj_a = length; i < _pj_a; i += 1) {
    crc ^= data[i];

    for (let j = 0, _pj_b = 8; j < _pj_b; j += 1) {
      if ((crc & 1) > 0) {
        crc = (crc >> 1) ^ 40961;
      } else {
        crc = crc >> 1;
      }
    }
  }
  return crc.toString(16);
}

// convert input price into protocol
function parsePrice(price: number) {
  const zero = '0';
  let strPrice = `${price}`;
  const plength = strPrice.length;

  // set price value to 6 digit value (ex. 1000 won --> 001000 / 2500 won --> 002500 )
  if (plength < 6) {
    for (let i = 0; 6 - plength - i > 0; i++) {
      strPrice = zero.concat(strPrice);
    }
  }

  // packet structure refer to datasheet
  const STX = [0x02];
  const retcrc = [
    0x00,
    0x08,
    0xf8,
    0x20,
    0x02,
    parseInt(strPrice.substring(0, 2), 16),
    parseInt(strPrice.substring(2, 4), 16),
    parseInt(strPrice.substring(4, 6), 16),
    0x03,
  ];
  let pricecrc = crc16(retcrc);
  const clength = pricecrc.length;

  // set crc value to hex 4 digits
  if (clength < 4) {
    for (let i = 0; 4 - clength - i > 0; i++) {
      pricecrc = zero.concat(pricecrc);
    }
  }

  // divide CRC value to CRC_H, CRC_L
  const fincrc = [parseInt(pricecrc.substring(0, 2), 16), parseInt(pricecrc.substring(2, 4), 16)];

  // reconfigure packet to send in format
  const price_protocol = STX.concat(retcrc.concat(fincrc));
  return new Uint8Array(price_protocol);
}

function useSPD500() {
  const filter = { usbVendorId: 0x0403 };
  const portRef = useRef<SerialPort>();
  const [info, setInfo] = useState<Partial<SerialPortInfo>>();

  useEffect(() => {
    if (!isSPD()) return;
    navigator.serial.getPorts().then(ports => {
      const availablePort = ports.find((port: SerialPort) => port.getInfo().usbVendorId == filter.usbVendorId);
      if (!!availablePort) {
        portRef.current = availablePort;
        portRef.current?.open({
          baudRate: 115200,
        });
        setInfo(portRef.current?.getInfo());
        console.log('Success open serial port', portRef.current?.getInfo());
      }
    });
    return () => {
      setInfo(undefined);
      portRef.current?.close();
    };
  }, []);

  const connect = async () => {
    try {
      portRef.current = await navigator.serial.requestPort({ filters: [filter] });
      await portRef.current?.open({
        baudRate: 115200,
      });
      setInfo(portRef.current?.getInfo());
      console.log('Success open serial port', portRef.current?.getInfo());
    } catch (error) {
      console.log('Failed to open serial port', filter);
      console.log(error);
    }
  };

  const disconnect = async () => {
    console.log('Close port');
    setInfo(undefined);
    await portRef.current?.close();
  };

  // receive confirm protocol
  const sendConfirm = async () => {
    console.log(`started sendConfirm`);
    const confirm = new Uint8Array([0x06, 0x06, 0x06, 0x06]);
    const writer = portRef.current?.writable.getWriter();
    await writer?.write(confirm);
    writer?.releaseLock();
    console.log('writer.releaseLock');
  };

  // send price to SPD500
  const sendPrice = async (price: number) => {
    const price_protocol = parsePrice(price);
    console.log('sendPrice', price_protocol);
    const writer = portRef.current?.writable.getWriter();
    await writer?.write(price_protocol);
    writer?.releaseLock();
    console.log('writer.releaseLock');
  };

  // this loop must run before credit card is inserted
  const purchaseLoop = async ({
    price,
    onCompleted,
    onFailed,
  }: {
    price: number;
    onCompleted: (value: any) => void;
    onFailed: (error: any) => void;
  }) => {
    console.log(`started purchaseLoop`);
    const reader = portRef.current?.readable.getReader();
    if (!reader) {
      console.log(`reader unavailable`);
      return onFailed(`reader unavailable`);
    }
    while (true) {
      try {
        const { value, done } = await reader.read();
        if (done) {
          console.log('done break');
          break;
        }
        if (value.length > 0) {
          console.log('got value', value);
          // when 'any' message runs in the loop immediatly returns [06, 06, 06, 06]
          await sendConfirm();
          if (value[5] == 1) {
            // automatically send price when card inserted and ready for payment
            // need some delay for HW to proceed
            await new Promise(resolve => setTimeout(resolve, 1000));
            sendPrice(price);
            await new Promise(resolve => setTimeout(resolve, 500));
          }
          // you'll get sth like [02 00 06 07 20 02 00 03 c c]
          // the FE should check "idx[6] == 0" to see purchase result
          // if idx[6]==0xFF the purchase fails
          if (value[6] == 0) {
            onCompleted(value);
            await sendConfirm();
            break;
          }
          if (value[6] == 0xff) {
            onFailed(value);
            await sendConfirm();
            break;
          }
        }
      } catch (error) {
        console.log(error);
        onFailed(error);
        break;
      }
    }
    console.log('reader.releaseLock');
    reader.releaseLock();
  };

  const startPayment = (price: number) =>
    new Promise(async (resolve, reject) => {
      if (!portRef.current) {
        await connect();
      }
      purchaseLoop({
        price,
        onCompleted: value => resolve(value),
        onFailed: error => reject(error),
      });
    });

  return {
    startPayment,
    connect,
    disconnect,
    info,
  };
}

export default useSPD500;
