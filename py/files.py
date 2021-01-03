#!/usr/bin/env python3


""" This header conatins classes representing files in Spooled FS. """

import abc
import os
import stat
import tempfile
import time

import pyfuse3


class FileBaseMixin:
    
    def __init__(self, inode, size, mode):
        attrs = pyfuse3.EntryAttributes()

        attrs.st_ino = inode
        attrs.generation = 0
        attrs.entry_timeout = 300
        attrs.attr_timeout = 300
        attrs.st_mode = mode
        attrs.st_nlink = 1
        attrs.st_uid = os.getuid()
        attrs.st_gid = os.getgid()
        attrs.st_rdev = 1997
        attrs.st_size = size
        attrs.st_blksize = 512
        attrs.st_blocks = 1

        now = time.time_ns()

        attrs.st_atime_ns = now
        attrs.st_mtime_ns = now
        attrs.st_ctime_ns = now

        # public interface
        self.attrs = attrs


class DirFile(FileBaseMixin):

    def __init__(self, inode, mode=stat.S_IFDIR|0o666):
        super().__init__(inode, 4096, mode)

        # public interface
        self.files = []


class LinkFile(FileBaseMixin):

    def __init__(self, target, inode, mode=stat.S_IFLNK|0o666):
        super().__init__(inode, len(target), mode)

        # public interface
        self.target = target


class IOFileBase(abc.ABC):

    @abc.abstractmethod
    def open(self):
        pass
    
    @abc.abstractmethod
    def close(self):
        pass
    
    @abc.abstractmethod
    def write(self, buf, off):
        pass
    
    @abc.abstractmethod
    def read(self, size, off):
        pass

    @abc.abstractmethod
    def cleanup(self):
        pass


class MemoryFile(IOFileBase, FileBaseMixin):
    
    def __init__(self, inode, mode=stat.S_IFREG|0o666):
        super().__init__(inode, 0, mode) 
        self._blob = bytearray()

    def open(self):
        pass
    
    def close(self):
        pass

    def write(self, buf, off):
        if off < len(self._blob):
            nbytes_before = len(self._blob)
            self._blob[off:] = buf
            self.attrs.st_size += (len(self._blob) - nbytes_before)
        else:
            new_zeros_size = off - len(self._blob)
            self._blob.extend(0 for _ in range(new_zeros_size))
            self._blob += buf
            # update inner attributes
            self.attrs.st_size += (new_zeros_size + len(buf))
        return len(buf)

    def read(self, size, off):
        if -1 == size:
            return self._blob
        return self._blob[off:off+size]

    def cleanup(self):
        pass

def get_new_file_size_inc(old_size, off, nbytes):
    if off < old_size:
        new_size = off + nbytes
        if new_size > old_size:
            return new_size - old_size
    else:
        new_zeros_size = off - old_size
        return new_zeros_size + nbytes 
    return 0

class DiskFile(IOFileBase, FileBaseMixin):

    def __init__(self, inode, mode=stat.S_IFREG|0o666):
        super().__init__(inode, 0, mode) 
        self._fh = None
        fd, self._disk_path = tempfile.mkstemp()
        os.close(fd)

    @classmethod
    def from_memory_file(cls, memory_file):
        disk_file = cls(0, 0)
        for field in dir(pyfuse3.EntryAttributes):
            if field.startswith('st_') and field != 'st_size':
                setattr(disk_file.attrs, field, getattr(memory_file.attrs, field))
        disk_file.open()
        disk_file.write(memory_file.read(-1, 0), 0)
        disk_file.close()
        return disk_file


    def open(self):
        self._fh = os.open(self._disk_path, flags=os.O_RDWR)

    def close(self):
        os.close(self._fh)
        self._fh = None

    def write(self, buf, off):
        os.lseek(self._fh, off, os.SEEK_SET)
        nbytes = os.write(self._fh, buf)
        self.attrs.st_size += get_new_file_size_inc(
            self.attrs.st_size,
            off,
            nbytes
        )
        return nbytes

    def read(self, size, off):
        os.lseek(self._fh, off, os.SEEK_SET)
        if -1 == size:
            return os.read(self._fh, self.attrs.st_size)
        return os.read(self._fh, size)
    
    def cleanup(self):
        if self._fh is not None:
            self.close()
        os.remove(self._disk_path)

class SpoolFile(IOFileBase):

    spool_size = 10

    def __init__(self, inode, mode=stat.S_IFLNK|0o666):
        self.file = MemoryFile(inode, mode)

    @property
    def attrs(self):
        return self.file.attrs

    def open(self):
        return self.file.open()

    def close(self):
        return self.file.open(close)
    
    def write(self, buf, off):
        new_size = self.file.attrs.st_size + get_new_file_size_inc(
            self.file.attrs.st_size,
            off,
            len(buf)
        )
        if self.spool_size is not None and new_size > self.spool_size:
            disk_file = DiskFile.from_memory_file(self.file)
            self.file.close()
            self.file.cleanup()
            self.file = disk_file
            self.file.open()
        return self.file.write(buf, off)

    def read(self, size, off):
        return self.file.read(size, off)

    def cleanup(self):
        self.file.cleanup()

mf = SpoolFile(11)

mf.open()

print(mf.write(b'hello', 1))
print(mf.read(2, 1))
print(mf.read(-1, 0))
print(mf.write(b'worldhelloooooooo', 0))
print(mf.read(-1, 0))
print(mf.attrs.st_size)

mf.cleanup()
