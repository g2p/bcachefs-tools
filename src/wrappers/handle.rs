use std::{path::Path, os::unix::ffi::OsStrExt, ffi::CString};

use bch_bindgen::c::{bchfs_handle, BCH_IOCTL_SUBVOLUME_CREATE, BCH_IOCTL_SUBVOLUME_DESTROY, bch_ioctl_subvolume, bcache_fs_open, BCH_SUBVOL_SNAPSHOT_CREATE, bcache_fs_close};
use errno::Errno;

/// A handle to a bcachefs filesystem
/// This can be used to send [`libc::ioctl`] to the underlying filesystem.
pub(crate) struct BcachefsHandle {
    inner: bchfs_handle
}

impl BcachefsHandle {
    /// Opens a bcachefs filesystem and returns its handle
    /// TODO(raitobezarius): how can this not be faillible?
    pub(crate) unsafe fn open<P: AsRef<Path>>(path: P) -> Self {
        let path = CString::new(path.as_ref().as_os_str().as_bytes()).expect("Failed to cast path into a C-style string");
        Self {
            inner: bcache_fs_open(path.as_ptr())
        }
    }
}

/// I/O control commands that can be sent to a bcachefs filesystem
/// Those are non-exhaustive 
#[repr(u64)]
#[non_exhaustive]
pub enum BcachefsIoctl {
    SubvolumeCreate = BCH_IOCTL_SUBVOLUME_CREATE,
    SubvolumeDestroy = BCH_IOCTL_SUBVOLUME_DESTROY,
}

/// I/O control commands payloads
#[non_exhaustive]
pub enum BcachefsIoctlPayload {
    Subvolume(bch_ioctl_subvolume),
}

impl From<&BcachefsIoctlPayload> for *const libc::c_void {
    fn from(value: &BcachefsIoctlPayload) -> Self {
        match value {
            BcachefsIoctlPayload::Subvolume(p) => p as *const _ as *const libc::c_void
        }
    }
}

impl BcachefsHandle {
    /// Type-safe [`libc::ioctl`] for bcachefs filesystems
    pub fn ioctl(&self, request: BcachefsIoctl, payload: &BcachefsIoctlPayload) -> Result<(), Errno> {
        let payload_ptr: *const libc::c_void = payload.into();
        let ret = unsafe { libc::ioctl(self.inner.ioctl_fd, request as u64, payload_ptr) };

        if ret == -1 {
            Err(errno::errno())
        } else {
            Ok(())
        }
    }

    /// Create a subvolume for this bcachefs filesystem
    /// at the given path
    pub fn create_subvolume<P: AsRef<Path>>(&self, dst: P) -> Result<(), Errno> {
        let dst = CString::new(dst.as_ref().as_os_str().as_bytes()).expect("Failed to cast destination path for subvolume in a C-style string");
        self.ioctl(BcachefsIoctl::SubvolumeCreate, &BcachefsIoctlPayload::Subvolume(bch_ioctl_subvolume {
            dirfd: libc::AT_FDCWD,
            mode: 0o777,
            dst_ptr: dst.as_ptr() as u64,
            ..Default::default()
        }))
    }

    /// Delete the subvolume at the given path
    /// for this bcachefs filesystem
    pub fn delete_subvolume<P: AsRef<Path>>(&self, dst: P) -> Result<(), Errno> {
        let dst = CString::new(dst.as_ref().as_os_str().as_bytes()).expect("Failed to cast destination path for subvolume in a C-style string");
        self.ioctl(BcachefsIoctl::SubvolumeDestroy, &BcachefsIoctlPayload::Subvolume(bch_ioctl_subvolume {
            dirfd: libc::AT_FDCWD,
            mode: 0o777,
            dst_ptr: dst.as_ptr() as u64,
            ..Default::default()
        }))
    }

    /// Snapshot a subvolume for this bcachefs filesystem
    /// at the given path
    pub fn snapshot_subvolume<P: AsRef<Path>>(&self, extra_flags: u32, src: P, dst: P) -> Result<(), Errno> {
        let src = CString::new(src.as_ref().as_os_str().as_bytes()).expect("Failed to cast source path for subvolume in a C-style string");
        let dst = CString::new(dst.as_ref().as_os_str().as_bytes()).expect("Failed to cast destination path for subvolume in a C-style string");
        self.ioctl(BcachefsIoctl::SubvolumeCreate, &BcachefsIoctlPayload::Subvolume(bch_ioctl_subvolume {
            flags: BCH_SUBVOL_SNAPSHOT_CREATE | extra_flags,
            dirfd: libc::AT_FDCWD,
            mode: 0o777,
            src_ptr: src.as_ptr() as u64,
            dst_ptr: dst.as_ptr() as u64,
            ..Default::default()
        }))
    }
}

impl Drop for BcachefsHandle {
    fn drop(&mut self) {
        unsafe { bcache_fs_close(self.inner) };
    }
}
