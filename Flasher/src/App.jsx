import "./App.css";
import { useState, useEffect } from "react";
import { open } from "@tauri-apps/api/dialog"
import { readBinaryFile } from "@tauri-apps/api/fs"
import { invoke } from "@tauri-apps/api";

function longToByteArray(/*long*/long) {
    // we want to represent the input as a 8-bytes array
    var byteArray = [0, 0, 0, 0, 0, 0, 0, 0];

    for ( var index = 0; index < byteArray.length; index ++ ) {
        var byte = long & 0xff;
        byteArray [ index ] = byte;
        long = (long - byte) / 256 ;
    }

    return byteArray;
};

const FLASH_STATUS = {
  NOT_STARTED: 0,
  IN_PROGRESS: 1,
  SUCCESS: 2,
  FAILED: 3,
};

function App() {
  const [file, setFile] = useState(null);
  const [fileName, setFileName] = useState(null);
  const [availablePorts, setAvailablePorts] = useState(null);
  const [selectedPort, setSelectedPort] = useState("/dev/ttyACM0");
  const [baudRate, setBaudRate] = useState(115200);
  const [flashStatus, setFlashStatus] = useState(FLASH_STATUS.NOT_STARTED);

  async function searchPorts() {
    const ports = await invoke("search_serial_ports");
    console.log(ports);
    setAvailablePorts(ports);
  }

  useEffect(() => {
    searchPorts();
  }, []);

  async function openDialog() {
    const result = await open({
      multiple: false,
      filters: [
        { 
          name: 'Binary',
          extensions: ['bin'] 
        },
      ],
    });

    if (result === null) {
      return;
    } 

    setFile(result);
    setFileName(result.split("/").pop());
  }

  async function sendOverSerial() {
    if (file === null || selectedPort === null) {
      return;
    }

    console.log("Sending file over serial", file, selectedPort);

    const contents = await readBinaryFile(file);
    
    console.log("Writing to port", selectedPort, contents.length, Array.from(contents));
    setFlashStatus(FLASH_STATUS.IN_PROGRESS);

    const res = await invoke("write_to_port", {
      portName: selectedPort,
      data: Array.from(contents),
    }).then(() => {
      console.log("Write success");
      setFlashStatus(FLASH_STATUS.SUCCESS);
    }).catch((err) => {
      console.log("Write failed", err);
      setFlashStatus(FLASH_STATUS.FAILED);
    });
  }

  

  return (
    <div className="container">
      <h1>Flash STM32F40x</h1>

      <div className="serial-container">
        <select name="serial port" onInput={(e) => setSelectedPort(e.target.value)}>
          <option value="">--Choose Serial Port--</option>
          {availablePorts?.map((port) => (
            <option value={port}>{port}</option>
          ))}
        </select>

        <button className="refresh-button" onClick={searchPorts}>
          Refresh
        </button>
      </div>

      <div className="baud-container">
        <select name="serial port" onInput={(e) => setBaudRate(e.target.value)}>
          <option value="">--Choose Baud Rate--</option>
          <option value="9600">9600</option>
          <option value="115200">115200</option>
        </select>
      </div>

      <div className="button-container">
        <button className="button" onClick={openDialog}>
          {fileName ?? "Choose File"}
        </button>

        <button className="button flash-button" 
                onClick={sendOverSerial} 
                disabled= {flashStatus === FLASH_STATUS.IN_PROGRESS}>
          Flash
        </button>
      </div>

      <div className="status-container">
        {flashStatus === FLASH_STATUS.NOT_STARTED && (
          <p className="status">Not started</p>
        )}

        {flashStatus === FLASH_STATUS.IN_PROGRESS && (
          <p className="status">Flashing...</p>
        )}

        {flashStatus === FLASH_STATUS.SUCCESS && (
          <p className="status success">Flash success</p>
        )}

        {flashStatus === FLASH_STATUS.FAILED && (
          <p className="status failed">Flash failed</p>
        )}
      </div> 


    </div>
  );
}

export default App;
