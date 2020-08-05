use std::env;
use std::io::prelude::*;
use std::net::{TcpListener, TcpStream};
use std::slice;
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;

fn handle_connection(keys_unlocked: Arc<Mutex<Vec<String>>>, mut stream: TcpStream, leased_key: String) {
    let mut sequence_nr = 0u8;
    loop {
        thread::sleep(Duration::from_secs(2));
        match stream.write(slice::from_ref(&sequence_nr)) {
            Ok(_) => {},
            Err(err) => {
                eprintln!("Failed to write keep-alive request to socket, returning key '{}' to the set: {}", leased_key, err);
                break;
            }
        }

        let mut response = 0u8;
        match stream.read_exact(slice::from_mut(&mut response)) {
            Ok(_) => {
                if response != sequence_nr {
                    eprintln!("Client returned incorrect response {} (expected {}), returning key '{}' to the set", response, sequence_nr, leased_key);
                    break;
                }
            }
            Err(err) => { 
                eprintln!("Failed to read from socket, returning key '{}' to the set: {}", leased_key, err);
                break;
            }
        }
        sequence_nr += 1;
    }

    let mut keys_locked = keys_unlocked.lock().expect("Failed to acquire the key set mutex");
    keys_locked.push(leased_key);
}

fn main() {
    let args: Vec<String> = env::args().collect();
    println!("Args = {:?}", args);
    let _program_name = &args[0];

    let mut keys = Vec::new();
    for key in args[1..].iter() {
        keys.push(String::from(key));
    }

    let keys_unlocked = Arc::new(Mutex::new(keys));
    let listener = TcpListener::bind("127.0.0.1:26624").expect("Failed to bind TCP listener to local address");

    let mut all_threads = Vec::new();

    for stream in listener.incoming() {
        let mut stream = stream.expect("Failed to create new TCP stream from incoming connection");
        stream.set_read_timeout(Some(Duration::from_millis(100))).expect("Failed to set socket read timeout");
        stream.set_write_timeout(Some(Duration::from_millis(10))).expect("Failed to set socket write timeout");
        let mut setname_len = 0;
        stream.read_exact(slice::from_mut(&mut setname_len)).unwrap();
        let mut setname_buffer = vec![0; setname_len as usize];
        stream.read_exact(&mut setname_buffer).unwrap();

        let setname = String::from_utf8_lossy(&setname_buffer[..]);
        println!("Request: {}", setname);

        let mut keys_locked = keys_unlocked.lock().expect("Failed to acquire the key set mutex");
        match keys_locked.pop() {
            None => {
                println!("No available keys!");
                let null = 0u8;
                match stream.write(slice::from_ref(&null)) {
                    Ok(_) => {},
                    Err(err) => {
                        eprintln!("Failed to write no-keys response to socket: {}", err);
                    }
                }
            },
            Some(leased_key) => {
                println!("Leased key: {}", leased_key);
                let key_len = leased_key.len() as u8;
                let key_len_result = stream.write(slice::from_ref(&key_len));
                let key_result = stream.write(leased_key.as_bytes());

                match key_len_result {
                    Ok(_) => match key_result {
                        Ok(_) => {
                            let key_cloneref = Arc::clone(&keys_unlocked);
                            all_threads.push(thread::spawn(move || {
                                handle_connection(key_cloneref, stream, leased_key);
                            }));
                        },
                        Err(err) => {
                            eprintln!("Failed to write key to socket, returning key '{}' to the set: {}", leased_key, err);
                            break;
                        }
                    },
                    Err(err) => {
                        eprintln!("Failed to write key length to socket, returning key '{}' to the set: {}", leased_key, err);
                        break;
                    }
                }
            }
        }
    }
}