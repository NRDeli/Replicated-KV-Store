use std::fs::{File, OpenOptions};
use std::io::{Read, Write, Seek};
use std::ptr;
use std::sync::Mutex;
use lazy_static::lazy_static;

#[repr(C)]
pub struct WalEntry {
    pub index: u64,
    pub term: u64,
    pub key_ptr: *const u8,
    pub key_len: usize,
    pub val_ptr: *const u8,
    pub val_len: usize,
}

struct Wal {
    dir: String,
    file: File,
    entries: Vec<Vec<u8>>,
    snapshot_index: u64,
}

lazy_static::lazy_static! {
    static ref GLOBAL: Mutex<Option<Wal>> = Mutex::new(None);
}

fn encode(index: u64, term: u64, key: &[u8], val: &[u8]) -> Vec<u8> {
    let mut buf = Vec::new();
    buf.extend(&index.to_le_bytes());
    buf.extend(&term.to_le_bytes());
    buf.extend(&(key.len() as u32).to_le_bytes());
    buf.extend(&(val.len() as u32).to_le_bytes());
    buf.extend(key);
    buf.extend(val);
    buf
}

fn decode_all(mut file: &File) -> Vec<Vec<u8>> {
    let mut buf = Vec::new();
    let _ = file.read_to_end(&mut buf);

    let mut pos = 0;
    let mut out = Vec::new();

    while pos + 24 <= buf.len() {
        let key_len =
            u32::from_le_bytes(buf[pos + 16..pos + 20].try_into().unwrap()) as usize;
        let val_len =
            u32::from_le_bytes(buf[pos + 20..pos + 24].try_into().unwrap()) as usize;

        let total = 24 + key_len + val_len;
        if pos + total > buf.len() {
            break;
        }

        out.push(buf[pos..pos + total].to_vec());
        pos += total;
    }
    out
}

#[no_mangle]
pub extern "C" fn wal_open(path: *const i8) -> i32 {
    if path.is_null() {
        return -1;
    }

    let cstr = unsafe { std::ffi::CStr::from_ptr(path) };
    let p = cstr.to_str().unwrap();

    let file = OpenOptions::new()
        .create(true)
        .append(true)
        .read(true)
        .open(p)
        .unwrap();

    let entries = decode_all(&file);

    let wal = Wal {
        dir: p.to_string(),
        file,
        entries,
        snapshot_index: 0,
    };

    *GLOBAL.lock().unwrap() = Some(wal);
    0
}

#[no_mangle]
pub extern "C" fn wal_append(
    index: u64,
    term: u64,
    key_ptr: *const u8,
    key_len: usize,
    val_ptr: *const u8,
    val_len: usize,
) -> i32 {
    let mut g = GLOBAL.lock().unwrap();
    let wal = g.as_mut().unwrap();

    let key = unsafe { std::slice::from_raw_parts(key_ptr, key_len) };
    let val = unsafe { std::slice::from_raw_parts(val_ptr, val_len) };

    let rec = encode(index, term, key, val);

    wal.file.write_all(&rec).unwrap();
    wal.file.flush().unwrap();

    wal.entries.push(rec);
    0
}

#[no_mangle]
pub extern "C" fn wal_count() -> u64 {
    GLOBAL
        .lock()
        .unwrap()
        .as_ref()
        .map(|w| w.entries.len() as u64)
        .unwrap_or(0)
}

#[no_mangle]
pub extern "C" fn wal_read(
    idx: u64,
    entry: *mut WalEntry,
) -> i32 {
    let g = GLOBAL.lock().unwrap();
    let wal = g.as_ref().unwrap();

    if idx >= wal.entries.len() as u64 {
        return -1;
    }

    let rec = &wal.entries[idx as usize];

    let index = u64::from_le_bytes(rec[0..8].try_into().unwrap());
    let term = u64::from_le_bytes(rec[8..16].try_into().unwrap());
    let klen = u32::from_le_bytes(rec[16..20].try_into().unwrap()) as usize;
    let vlen = u32::from_le_bytes(rec[20..24].try_into().unwrap()) as usize;

    let key_ptr = rec[24..24 + klen].as_ptr();
    let val_ptr = rec[24 + klen..24 + klen + vlen].as_ptr();

    unsafe {
        (*entry).index = index;
        (*entry).term = term;
        (*entry).key_ptr = key_ptr;
        (*entry).key_len = klen;
        (*entry).val_ptr = val_ptr;
        (*entry).val_len = vlen;
    }

    0
}

#[no_mangle]
pub extern "C" fn wal_last_index() -> u64 {
    GLOBAL
        .lock()
        .unwrap()
        .as_ref()
        .map(|w| w.entries.len() as u64)
        .unwrap_or(0)
}

#[no_mangle]
pub extern "C" fn wal_truncate_from(index: u64) -> i32 {
    let mut g = GLOBAL.lock().unwrap();
    let wal = g.as_mut().unwrap();

    if index >= wal.entries.len() as u64 {
        return 0;
    }

    wal.entries.truncate(index as usize);

    // rewrite file from scratch
    wal.file.set_len(0).unwrap();
    wal.file.seek(std::io::SeekFrom::Start(0)).unwrap();

    for rec in &wal.entries {
        wal.file.write_all(rec).unwrap();
    }

    wal.file.flush().unwrap();
    0
}

#[no_mangle]
pub extern "C" fn wal_create_snapshot(
    data_ptr: *const u8,
    data_len: usize,
    last_index: u64,
) -> i32 {
    let mut g = GLOBAL.lock().unwrap();
    let wal = g.as_mut().unwrap();

    let data = unsafe { std::slice::from_raw_parts(data_ptr, data_len) };

    let path = format!("{}/snapshot.bin", wal.dir);

    std::fs::write(&path, data).unwrap();

    wal.snapshot_index = last_index;

    // compact log below snapshot
    if last_index > 0 && last_index <= wal.entries.len() as u64 {
        wal.entries.drain(..last_index as usize);
    }

    wal.file.set_len(0).unwrap();
    wal.file.seek(std::io::SeekFrom::Start(0)).unwrap();

    for rec in &wal.entries {
        wal.file.write_all(rec).unwrap();
    }

    wal.file.flush().unwrap();

    0
}

#[no_mangle]
pub extern "C" fn wal_load_snapshot(
    out_ptr: *mut *const u8,
    out_len: *mut usize,
    out_index: *mut u64,
) -> i32 {
    let g = GLOBAL.lock().unwrap();
    let wal = g.as_ref().unwrap();

    let path = format!("{}/snapshot.bin", wal.dir);

    if let Ok(data) = std::fs::read(&path) {
        unsafe {
            *out_ptr = data.as_ptr();
            *out_len = data.len();
            *out_index = wal.snapshot_index;
        }

        std::mem::forget(data);
        return 0;
    }

    -1
}