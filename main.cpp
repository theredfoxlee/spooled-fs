#define FUSE_USE_VERSION 35
#include "sfs_files.hpp"

#include <iostream>
#include <assert.h>
#include <sys/stat.h>


#include <fuse3/fuse_lowlevel.h>


void
sfs_init_callbacks(struct fuse_lowlevel_ops *ops) {
    ops->init = NULL;
    ops->destroy = NULL;
    ops->lookup = NULL;     // TODO: implement
    ops->forget = NULL;     // TODO: implement
    ops->getattr = NULL;    // TODO: implement
    ops->setattr = NULL;
    ops->readlink = NULL;   // TODO: implement
    ops->mknod = NULL;
    ops->mkdir = NULL;      // TODO: implement
    ops->unlink = NULL;     // TODO: implement
    ops->rmdir = NULL;      // TODO: implement
    ops->symlink = NULL;    // TODO: implement
    ops->rename = NULL;     // TODO: implement
    ops->link = NULL;
    ops->open = NULL;
    ops->read = NULL;
    ops->write = NULL;
    ops->flush = NULL;
    ops->release = NULL;
    ops->fsync = NULL;
    ops->opendir = NULL;
    ops->readdir = NULL;
    ops->releasedir = NULL;
    ops->fsyncdir = NULL;
    ops->statfs = NULL;
    ops->setxattr = NULL;
    ops->getxattr = NULL;
    ops->listxattr = NULL;
    ops->removexattr = NULL;
    ops->access = NULL;
    ops->create = NULL;
    ops->getlk = NULL;
    ops->setlk = NULL;
    ops->bmap = NULL;
    ops->ioctl = NULL;
    ops->poll = NULL;
    ops->write_buf = NULL;
    ops->retrieve_reply = NULL;
    ops->forget_multi = NULL;
    ops->flock = NULL;
    ops->fallocate = NULL;
    ops->readdirplus = NULL;
    ops->copy_file_range = NULL;
    ops->lseek = NULL;
}

int
main(int argc, char **argv) {
    // main taken from: https://github.com/libfuse/libfuse/blob/master/example/hello_ll.c 

    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    struct fuse_session *se;
    struct fuse_cmdline_opts opts;
    struct fuse_loop_config config;
    int ret = -1;

    if (fuse_parse_cmdline(&args, &opts) != 0)
        return 1;
    if (opts.show_help) {
        printf("usage: %s [options] <mountpoint>\n\n", argv[0]);
        fuse_cmdline_help();
        fuse_lowlevel_help();
        ret = 0;
        goto err_out1;
    } else if (opts.show_version) {
        printf("FUSE library version %s\n", fuse_pkgversion());
        fuse_lowlevel_version();
        ret = 0;
        goto err_out1;
    }

    if(opts.mountpoint == NULL) {
        printf("usage: %s [options] <mountpoint>\n", argv[0]);
        printf("       %s --help\n", argv[0]);
        ret = 1;
        goto err_out1;
    }

    struct fuse_lowlevel_ops ops;
    sfs_init_callbacks(&ops);

    se = fuse_session_new(&args, &ops,
                  sizeof(ops), NULL);
    if (se == NULL)
        goto err_out1;

    if (fuse_set_signal_handlers(se) != 0)
        goto err_out2;

    if (fuse_session_mount(se, opts.mountpoint) != 0)
        goto err_out3;

    fuse_daemonize(opts.foreground);

    /* Block until ctrl+c or fusermount -u */
    if (opts.singlethread)
        ret = fuse_session_loop(se);
    else {
        config.clone_fd = opts.clone_fd;
        config.max_idle_threads = opts.max_idle_threads;
        ret = fuse_session_loop_mt(se, &config);
    }

    fuse_session_unmount(se);
err_out3:
    fuse_remove_signal_handlers(se);
err_out2:
    fuse_session_destroy(se);
err_out1:
    free(opts.mountpoint);
    fuse_opt_free_args(&args);

    return ret ? 1 : 0;
}

// void
// test_io_file(sfs::IOFile *file) {
//     file->open();
//     file->write("321", 3, 3);

//     sfs::IOFile::BufferView view = file->read(-1, 0);

//     std::cout.write(view.get_buf(), view.get_size()) 
//         << ',' << view.get_size() << ',' << *file << '\n';

//     file->close();
// }


// int
// main() {
//     sfs::MemoryFile x("/hello", 15, S_IFREG | 0666, "123", 3);
//     sfs::DiskFile y("/hello", 15, S_IFREG | 0666, "123", 3);
//     sfs::SpoolFile z("/hello", 15, S_IFREG | 0666, "123", 3);
//     z.spool_size = 3; 
//     sfs::MemoryFile x1("/hello1", 15, S_IFREG | 0666);
//     sfs::DiskFile y1("/hello1", 15, S_IFREG | 0666);
//     sfs::MemoryFile x2("/hello2", 15, S_IFREG | 0666, "1234", 4);
//     sfs::DiskFile y2("/hello2", 15, S_IFREG | 0666, "1234", 4);
//     sfs::MemoryFile x3("/hello3", 15, S_IFREG | 0666, "123456", 6);
//     sfs::DiskFile y3("/hello3", 15, S_IFREG | 0666, "1234456", 6);

//     test_io_file(&x);
//     test_io_file(&y);
//     test_io_file(&z);
//     test_io_file(&x1);
//     test_io_file(&y1);
//     test_io_file(&x2);
//     test_io_file(&y2);
//     test_io_file(&x3);
//     test_io_file(&y3);
// }
