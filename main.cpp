#define FUSE_USE_VERSION 35
#include "sfs_files.hpp"

#include <iostream>
#include <sys/stat.h>


void
test_io_file(sfs::IOFile *file) {
    file->open();
    file->write("321", 3, 3);

    sfs::IOFile::BufferView view = file->read(-1, 0);

    std::cout.write(view.get_buf(), view.get_size()) 
        << ',' << view.get_size() << ',' << *file << '\n';

    file->close();
}


int
main() {
    sfs::MemoryFile x("/hello", 15, S_IFREG | 0666, "123", 3);
    sfs::DiskFile y("/hello", 15, S_IFREG | 0666, "123", 3);
    sfs::SpoolFile z("/hello", 15, S_IFREG | 0666, "123", 3);
    z.spool_size = 3; 
    sfs::MemoryFile x1("/hello1", 15, S_IFREG | 0666);
    sfs::DiskFile y1("/hello1", 15, S_IFREG | 0666);
    sfs::MemoryFile x2("/hello2", 15, S_IFREG | 0666, "1234", 4);
    sfs::DiskFile y2("/hello2", 15, S_IFREG | 0666, "1234", 4);
    sfs::MemoryFile x3("/hello3", 15, S_IFREG | 0666, "123456", 6);
    sfs::DiskFile y3("/hello3", 15, S_IFREG | 0666, "1234456", 6);

    test_io_file(&x);
    test_io_file(&y);
    test_io_file(&z);
    test_io_file(&x1);
    test_io_file(&y1);
    test_io_file(&x2);
    test_io_file(&y2);
    test_io_file(&x3);
    test_io_file(&y3);
}
