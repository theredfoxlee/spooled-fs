#pragma once

// This header describes files used by SpooledFS.


#include <ostream>
#include <string>

#include <assert.h>

#include <unistd.h>

#include <fuse3/fuse_lowlevel.h>


namespace sfs {

class BaseFile {
    // BaseFile specifies common file attributes.
protected:
    const std::string _fuse_path;
    const fuse_ino_t _fuse_inode;
    struct fuse_entry_param _fuse_param;

public:
    BaseFile() = delete;
    BaseFile(const std::string &fuse_path, fuse_ino_t fuse_inode, mode_t mode, size_t size)
        : _fuse_path(fuse_path), _fuse_inode(fuse_inode), _fuse_param() {
        _fuse_param.attr.st_dev = 1997;
        _fuse_param.attr.st_ino = fuse_inode;
        _fuse_param.attr.st_mode = mode;
        _fuse_param.attr.st_nlink = 1;
        _fuse_param.attr.st_uid = getuid();
        _fuse_param.attr.st_gid = getgid();
        _fuse_param.attr.st_size = size;

        struct timespec now;
        assert(-1 != clock_gettime(CLOCK_REALTIME, &now));

        _fuse_param.attr.st_atim = now;
        _fuse_param.attr.st_mtim = now;
        _fuse_param.attr.st_ctim = now;
    }

    inline const std::string & get_fuse_path() { return _fuse_path; }
    inline fuse_ino_t get_fuse_inode() { return _fuse_inode; }
    inline const struct fuse_entry_param & get_fuse_param() { return _fuse_param; }

    // stream representation of a file
    friend std::ostream &operator<<(std::ostream &stream, const BaseFile &file) {
        stream << "BaseFile(fuse_path=\""
               << file._fuse_path 
               << "\",fuse_inode="
               << file._fuse_inode
               << ",size="
               << file._fuse_param.attr.st_size
               << ",mode="
               << file._fuse_param.attr.st_mode
               << ")";
        return stream;
    }
};

class IOFile: public BaseFile {
    // IOFile specifies interface for regular files
    // - that support read/write operations.
public:
    class BufferView {
        // BufferView represents raw const char * pointer
        //   it stores also its size (moreover: it cleans up
        //   the buffer if required - which is necessary in
        //   case of adhoc allocation in DiskFile).
    private:
        const char *_buf;
        const size_t _size;
        const bool _delete_on_destroy;

    public:
        BufferView() = delete;
        BufferView(const char *buf, size_t size, bool delete_on_destroy)
            : _buf(buf), _size(size), _delete_on_destroy(delete_on_destroy) {}

        ~BufferView() {
            if (_delete_on_destroy) {
                delete _buf;
            }
        }

        inline const char * get_buf() { return _buf; }
        inline const size_t get_size() { return _size; }
    };

private:
    // no new attributes

public:
    using BaseFile::BaseFile;

    virtual inline void open() {}
    virtual inline void close() {}

    virtual size_t write(const char *buf, size_t size, off_t off) = 0;
    virtual BufferView read(size_t size, off_t off) = 0;
};

class MemoryFile: public IOFile {
    // MemoryFile represents simple memory located file.
private:
    std::string _blob;

public:
    MemoryFile() = delete;
    MemoryFile(const std::string &fuse_path, fuse_ino_t fuse_inode, mode_t mode)
        : IOFile(fuse_path, fuse_inode, mode, 0), _blob() {}
    MemoryFile(const std::string &fuse_path, fuse_ino_t fuse_inode, mode_t mode,
               const char *buf, size_t size)
        : IOFile(fuse_path, fuse_inode, mode, size), _blob(buf, size) {}

    size_t write(const char *buf, size_t size, off_t off) {
        if (off < _blob.size()) {
            if (off + size <= _blob.size()) {
                // it should fit inside a given _blob
                _blob.replace(off, size, buf, size);
            } else {
                // only `first_chunk_size` will fit inside a given _blob
                const size_t first_chunk_size = _blob.size() - off;
                // the `second_chunk_size` should extend _blob instance
                const size_t second_chunk_size = size - first_chunk_size;

                _blob.replace(off, first_chunk_size, buf, first_chunk_size);
                _blob.append(buf + first_chunk_size, second_chunk_size);

                _fuse_param.attr.st_size += second_chunk_size;
            }
        } else {
            // sometimes a program starts writing at random position
            // - so the space between already assigned values and op
            // position should be filled with zeros 
            const size_t new_zeros_size = off - _blob.size();

            _blob.append(new_zeros_size, 0); 
            _blob.append(buf, size);

            _fuse_param.attr.st_size += (new_zeros_size + size);
        }

        return size;
    }

    inline IOFile::BufferView read(size_t size, off_t off) {
        if (-1 == size) {
            size = _blob.size();
            off = 0;
        }
        return IOFile::BufferView(_blob.c_str() + off, size, false);
    }
};

}
