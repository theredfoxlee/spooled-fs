#pragma once

// This header describes files used by SpooledFS.


#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <memory>
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

    inline const std::string & get_fuse_path() const { return _fuse_path; }
    inline fuse_ino_t get_fuse_inode() const { return _fuse_inode; }
    inline const struct fuse_entry_param & get_fuse_param() const { return _fuse_param; }

    inline size_t get_size() const { return _fuse_param.attr.st_size; }
    inline mode_t get_mode() const { return _fuse_param.attr.st_mode; }

    inline bool is_file() const { return S_ISREG(_fuse_param.attr.st_mode); }
    inline bool is_dir() const { return S_ISDIR(_fuse_param.attr.st_mode); }
    inline bool is_link() const { return S_ISLNK(_fuse_param.attr.st_mode); }

    // stream representation of a file
    friend std::ostream &operator<<(std::ostream &stream, const BaseFile &file) {
        file.to_stream(stream);
        return stream;
    }

    virtual void to_stream(std::ostream &stream) const {
        stream << "BaseFile(fuse_path=\""
               << _fuse_path
               << "\",fuse_inode="
               << _fuse_inode
               << ",size="
               << _fuse_param.attr.st_size
               << ",mode="
               << _fuse_param.attr.st_mode
               << ")";
    }
};

class DirFile: public BaseFile {
    // DirFile represents a directory.
private:
    // This might be optimized in the future with pointers instead of strings.
    std::vector<std::string> _fuse_paths;

public:
    DirFile() = delete;
    DirFile(const std::string &fuse_path, fuse_ino_t fuse_inode, mode_t mode)
        : BaseFile(fuse_path, fuse_inode, mode, 4096) {}

    inline const std::vector<std::string> & get_paths() const { return _fuse_paths; }

    inline void add_path(const std::string &fuse_path) { _fuse_paths.push_back(fuse_path); }
    inline bool remove_path(const std::string &fuse_path) {
        auto iter = std::find(_fuse_paths.begin(), _fuse_paths.end(), fuse_path);
        if (_fuse_paths.end() == iter) {
            _fuse_paths.erase(iter);
            return true;
        }
        return false;
    }

    void to_stream(std::ostream &stream) const {
        stream << "DirFile(fuse_path=\""
               << _fuse_path
               << "\",fuse_inode="
               << _fuse_inode
               << ",size="
               << _fuse_param.attr.st_size
               << ",mode="
               << _fuse_param.attr.st_mode
               << ")";
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
    MemoryFile(const std::string &fuse_path, fuse_ino_t fuse_inode, mode_t mode,
               const char *buf = nullptr, size_t size = 0)
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

    virtual void to_stream(std::ostream &stream) const {
        stream << "MemoryFile(fuse_path=\""
               << _fuse_path
               << "\",fuse_inode="
               << _fuse_inode
               << ",size="
               << _fuse_param.attr.st_size
               << ",mode="
               << _fuse_param.attr.st_mode
               << ")";
    }
};

class DiskFile: public IOFile {
    // DiskFile wraps f<methods> for regular filesystem (e.g. ext4).
private:
    std::FILE *_fh;
    std::filesystem::path _disk_path;

    off_t _current_offset;

public:
    DiskFile() = delete;
    DiskFile(const std::string &fuse_path, fuse_ino_t fuse_inode, mode_t mode,
             const char *buf = nullptr, size_t size = 0)
        : IOFile(fuse_path, fuse_inode, mode, 0), _fh(nullptr), _disk_path(), _current_offset(0) {

        // set _disk_path to /tmp/<hash>
        _disk_path = std::filesystem::temp_directory_path()
            / std::to_string(std::filesystem::hash_value(fuse_path));
        std::filesystem::remove(_disk_path);
        // assert(!std::filesystem::exists(_disk_path));

        _fh = std::fopen(_disk_path.c_str(), "wb");
        if (nullptr != buf) {
            write(buf, size, 0);
        }
        close();
    }

    ~DiskFile() {
        std::filesystem::remove(_disk_path);
    }

    inline void open() {
        assert(nullptr == _fh);
        _fh = std::fopen(_disk_path.c_str(), "rb+");
        assert(NULL != _fh);
        assert(nullptr != _fh);
    }

    inline void close() {
        assert(nullptr != _fh);
        assert(0 == std::fclose(_fh));
        _fh = nullptr;
        _current_offset = 0;
    }

    size_t write(const char *buf, size_t size, off_t off) {
        assert(nullptr != _fh);

        const size_t file_size = _fuse_param.attr.st_size;
        size_t new_bytes = 0;

        if (off < file_size) {
            if (off + size <= file_size) {
                // no new_bytes
            } else {
                new_bytes += (size - (file_size - off));
            }
        } else {
            new_bytes += (off - file_size + size);
        }

        if (0 != _current_offset || 0 != off) {
            assert(0 == std::fseek(_fh, off, SEEK_SET));
        }
        size_t nbytes = std::fwrite(buf, sizeof(char), size, _fh);
        assert(size == nbytes);
        _fuse_param.attr.st_size += new_bytes;
        _current_offset = off + size;

        return nbytes;
    }

    IOFile::BufferView read(size_t size, off_t off) {
        assert(nullptr != _fh);
        if (-1 == size) {
            size = _fuse_param.attr.st_size;
            off = 0;
        }

        if (0 != _current_offset || 0 != off) {
            assert(0 == std::fseek(_fh, off, SEEK_SET));
            _current_offset = off;
        }
        char *buf = new char[size];  // it will be removed by BufferView

        size_t nbytes = std::fread(buf, sizeof(char), size, _fh);
        _current_offset = off + nbytes;
        assert(0 != nbytes);

        return IOFile::BufferView(buf, nbytes, true);
    }

    void to_stream(std::ostream &stream) const {
        stream << "DiskFile(fuse_path=\""
               << _fuse_path
               << "\",disk_path="
               << _disk_path
               << ",fuse_inode="
               << _fuse_inode
               << ",size="
               << _fuse_param.attr.st_size
               << ",mode="
               << _fuse_param.attr.st_mode
               << ")";
    }
};

class SpoolFile: public IOFile {
    // SpoolFile is a context which picks one of IOFile (strategies)
    // - it's either MemoryFile or DiskFile; based on file size.
public:
    size_t spool_size = 1024;

private:
    IOFile *_strategy = nullptr;
    bool _spooled = false;

public:
    SpoolFile() = delete;
    SpoolFile(const std::string &fuse_path, fuse_ino_t fuse_inode, mode_t mode,
                const char *buf = nullptr, size_t size = 0):
                IOFile(fuse_path, fuse_inode, mode, size) {
        if (size > spool_size) {
            _strategy = new DiskFile(fuse_path, fuse_inode, mode, buf, size);
            _spooled = true;
        } else {
            _strategy = new MemoryFile(fuse_path, fuse_inode, mode, buf, size);
        }
    }

    inline void open() { _strategy->open(); }
    inline void close() { _strategy->close(); }

    inline size_t write(const char *buf, size_t size, off_t off) {
        size_t nbytes = _strategy->write(buf, size, off);

        if (!_spooled && _strategy->get_size() > spool_size) {
            IOFile::BufferView view = _strategy->read(-1, 0);
            IOFile *new_strategy = new DiskFile(
                _strategy->get_fuse_path(),
                _strategy->get_fuse_inode(),
                _strategy->get_mode(),
                view.get_buf(),
                view.get_size()
            );
            new_strategy->open();
            delete _strategy;
            _strategy = new_strategy;
            _spooled = true;
        }

        return nbytes;
    }

    IOFile::BufferView read(size_t size, off_t off) {
        return _strategy->read(size, off);
    }

    ~SpoolFile() {
        delete _strategy;
    }

    void to_stream(std::ostream &stream) const {
        stream << "SpoolFile[";
        _strategy->to_stream(stream);
        stream << "]";
    }
};

}
