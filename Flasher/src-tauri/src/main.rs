
// Prevents additional console window on Windows in release, DO NOT REMOVE!!
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use serialport;
use std::thread::sleep;

#[tauri::command]
fn search_serial_ports() -> Vec<String> {
  let ports = serialport::available_ports()
                                      .expect("No ports found!");

  println!("Found {} ports", ports.len());

  ports
    .iter()
    .map(|port| port.port_name.clone())
    .collect()
}

#[tauri::command]
async fn write_to_port(port_name: String, data: Vec<u8>) -> Result<(), ()> {
  let length = data.len();
  println!("Writing {} bytes to port: {}", length, port_name);

  let mut port = serialport::new(port_name, 115_200)
    .timeout(std::time::Duration::from_millis(10))
    .open().expect("Failed to open port");

  // first 8 bytes are the length of the data
  port.write(&length.to_be_bytes())
    .expect("Failed to write length to port");

  sleep(std::time::Duration::from_millis(1000));

  // // wait for the arduino to respond with the same length
  // let mut buf: Vec<u8> = vec![0; 8];

  // while buf != length.to_be_bytes() {
  //   port.read_exact(&mut buf)
  //   .expect("Failed to read length from port");
  // }

  // write the data in chunks of 64 bytes
  let mut bytes_written = 0;

  let mut i = 0;
  for chunk in data.chunks(8) {
    println!("Writing {} chunk {:?}", i, chunk);
    bytes_written += port.write(chunk)
      .expect("Failed to write data to port");

    i += 1;
    std::thread::sleep(std::time::Duration::from_millis(5));
  }

  // port.write(&data)
    // .expect("Failed to write data to port");

  println!("Wrote {} bytes", bytes_written);

  Ok(())
}


fn main() {
    tauri::Builder::default()
        .invoke_handler(tauri::generate_handler![
          search_serial_ports,
          write_to_port
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
