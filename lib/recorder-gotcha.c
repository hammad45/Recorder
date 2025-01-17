#include "recorder-gotcha.h"
#include "recorder.h"

static bool posix_tracing = true;
static bool mpi_tracing   = false;
static bool mpiio_tracing = true;
static bool hdf5_tracing  = true;

struct gotcha_binding_t posix_wrap_actions [] = {
    GOTCHA_WRAP_ACTION(creat),
    GOTCHA_WRAP_ACTION(creat64),
    GOTCHA_WRAP_ACTION(open),
    GOTCHA_WRAP_ACTION(open64),
    GOTCHA_WRAP_ACTION(close),
    GOTCHA_WRAP_ACTION(write),
    GOTCHA_WRAP_ACTION(read),
    GOTCHA_WRAP_ACTION(lseek),
    GOTCHA_WRAP_ACTION(lseek64),
    GOTCHA_WRAP_ACTION(pread),
    GOTCHA_WRAP_ACTION(pread64),
    GOTCHA_WRAP_ACTION(pwrite),
    GOTCHA_WRAP_ACTION(pwrite64),
    GOTCHA_WRAP_ACTION(readv),
    GOTCHA_WRAP_ACTION(writev),
    GOTCHA_WRAP_ACTION(mmap),
    GOTCHA_WRAP_ACTION(mmap64),
    GOTCHA_WRAP_ACTION(msync),
    GOTCHA_WRAP_ACTION(fopen),
    GOTCHA_WRAP_ACTION(fopen64),
    GOTCHA_WRAP_ACTION(fclose),
    GOTCHA_WRAP_ACTION(fread),
    GOTCHA_WRAP_ACTION(fwrite),
    GOTCHA_WRAP_ACTION(fflush),
    GOTCHA_WRAP_ACTION(ftell),
    GOTCHA_WRAP_ACTION(fseek),
    GOTCHA_WRAP_ACTION(fsync),
    GOTCHA_WRAP_ACTION(fdatasync),
#if __GLIBC__ == 2 && __GLIBC_MINOR__ < 33
#ifdef HAVE___XSTAT
    GOTCHA_WRAP_ACTION(__xstat),
#endif
#ifdef HAVE___XSTAT64
    GOTCHA_WRAP_ACTION(__xstat64),
#endif
#ifdef HAVE___LXSTAT
    GOTCHA_WRAP_ACTION(__lxstat),
#endif
#ifdef HAVE___LXSTAT64
    GOTCHA_WRAP_ACTION(__lxstat64),
#endif
#ifdef HAVE___FXSTAT
    GOTCHA_WRAP_ACTION(__fxstat),
#endif
#ifdef HAVE___FXSTAT64
    GOTCHA_WRAP_ACTION(__fxstat64),
#endif
#endif
    GOTCHA_WRAP_ACTION(getcwd),
    GOTCHA_WRAP_ACTION(mkdir),
    GOTCHA_WRAP_ACTION(rmdir),
    GOTCHA_WRAP_ACTION(chdir),
    GOTCHA_WRAP_ACTION(link),
    GOTCHA_WRAP_ACTION(linkat),
    GOTCHA_WRAP_ACTION(unlink),
    GOTCHA_WRAP_ACTION(symlink),
    GOTCHA_WRAP_ACTION(symlinkat),
    GOTCHA_WRAP_ACTION(readlink),
    GOTCHA_WRAP_ACTION(readlinkat),
    GOTCHA_WRAP_ACTION(rename),
    GOTCHA_WRAP_ACTION(chmod),
    GOTCHA_WRAP_ACTION(chown),
    GOTCHA_WRAP_ACTION(lchown),
    GOTCHA_WRAP_ACTION(utime),
    GOTCHA_WRAP_ACTION(opendir),
    GOTCHA_WRAP_ACTION(readdir),
    GOTCHA_WRAP_ACTION(closedir),
    GOTCHA_WRAP_ACTION(fcntl),
    GOTCHA_WRAP_ACTION(dup),
    GOTCHA_WRAP_ACTION(dup2),
    GOTCHA_WRAP_ACTION(pipe),
    GOTCHA_WRAP_ACTION(mkfifo),
    GOTCHA_WRAP_ACTION(umask),
    GOTCHA_WRAP_ACTION(fdopen),
    GOTCHA_WRAP_ACTION(fileno),
    GOTCHA_WRAP_ACTION(access),
    GOTCHA_WRAP_ACTION(faccessat),
    GOTCHA_WRAP_ACTION(tmpfile),
    GOTCHA_WRAP_ACTION(remove),
    GOTCHA_WRAP_ACTION(truncate),
    GOTCHA_WRAP_ACTION(ftruncate),
    GOTCHA_WRAP_ACTION(fseeko),
    GOTCHA_WRAP_ACTION(ftello)
};

struct gotcha_binding_t mpiio_wrap_actions [] = {
    GOTCHA_WRAP_ACTION(MPI_File_close),
    GOTCHA_WRAP_ACTION(MPI_File_set_size),
    GOTCHA_WRAP_ACTION(MPI_File_iread_at),
    GOTCHA_WRAP_ACTION(MPI_File_iread),
    GOTCHA_WRAP_ACTION(MPI_File_iread_shared),
    GOTCHA_WRAP_ACTION(MPI_File_iwrite_at),
    GOTCHA_WRAP_ACTION(MPI_File_iwrite),
    GOTCHA_WRAP_ACTION(MPI_File_iwrite_shared),
    GOTCHA_WRAP_ACTION(MPI_File_open),
    GOTCHA_WRAP_ACTION(MPI_File_read_all_begin),
    GOTCHA_WRAP_ACTION(MPI_File_read_all),
    GOTCHA_WRAP_ACTION(MPI_File_read_at_all),
    GOTCHA_WRAP_ACTION(MPI_File_read_at_all_begin),
    GOTCHA_WRAP_ACTION(MPI_File_read_at),
    GOTCHA_WRAP_ACTION(MPI_File_read),
    GOTCHA_WRAP_ACTION(MPI_File_read_ordered_begin),
    GOTCHA_WRAP_ACTION(MPI_File_read_ordered),
    GOTCHA_WRAP_ACTION(MPI_File_read_shared),
    GOTCHA_WRAP_ACTION(MPI_File_set_view),
    GOTCHA_WRAP_ACTION(MPI_File_sync),
    GOTCHA_WRAP_ACTION(MPI_File_write_all_begin),
    GOTCHA_WRAP_ACTION(MPI_File_write_all),
    GOTCHA_WRAP_ACTION(MPI_File_write_at_all_begin),
    GOTCHA_WRAP_ACTION(MPI_File_write_at_all),
    GOTCHA_WRAP_ACTION(MPI_File_write_at),
    GOTCHA_WRAP_ACTION(MPI_File_write),
    GOTCHA_WRAP_ACTION(MPI_File_write_ordered_begin),
    GOTCHA_WRAP_ACTION(MPI_File_write_ordered),
    GOTCHA_WRAP_ACTION(MPI_File_write_shared),
    GOTCHA_WRAP_ACTION(MPI_File_seek),
    GOTCHA_WRAP_ACTION(MPI_File_seek_shared)
};

struct gotcha_binding_t mpi_wrap_actions [] = {
    GOTCHA_WRAP_ACTION(MPI_Finalized),
    GOTCHA_WRAP_ACTION(MPI_Comm_rank),
    GOTCHA_WRAP_ACTION(MPI_Comm_size),
    GOTCHA_WRAP_ACTION(MPI_Get_processor_name),
    GOTCHA_WRAP_ACTION(MPI_Comm_set_errhandler),
    GOTCHA_WRAP_ACTION(MPI_Barrier),
    GOTCHA_WRAP_ACTION(MPI_Bcast),
    GOTCHA_WRAP_ACTION(MPI_Gather),
    GOTCHA_WRAP_ACTION(MPI_Gatherv),
    GOTCHA_WRAP_ACTION(MPI_Scatter),
    GOTCHA_WRAP_ACTION(MPI_Scatterv),
    GOTCHA_WRAP_ACTION(MPI_Allgather),
    GOTCHA_WRAP_ACTION(MPI_Allgatherv),
    GOTCHA_WRAP_ACTION(MPI_Alltoall),
    GOTCHA_WRAP_ACTION(MPI_Reduce),
    GOTCHA_WRAP_ACTION(MPI_Allreduce),
    GOTCHA_WRAP_ACTION(MPI_Reduce_scatter),
    GOTCHA_WRAP_ACTION(MPI_Scan),
    GOTCHA_WRAP_ACTION(MPI_Type_commit),
    GOTCHA_WRAP_ACTION(MPI_Type_create_darray),
    GOTCHA_WRAP_ACTION(MPI_File_get_size),
    GOTCHA_WRAP_ACTION(MPI_Cart_rank),
    GOTCHA_WRAP_ACTION(MPI_Cart_create),
    GOTCHA_WRAP_ACTION(MPI_Cart_get),
    GOTCHA_WRAP_ACTION(MPI_Cart_shift),
    GOTCHA_WRAP_ACTION(MPI_Wait),
    GOTCHA_WRAP_ACTION(MPI_Send),
    GOTCHA_WRAP_ACTION(MPI_Recv),
    GOTCHA_WRAP_ACTION(MPI_Sendrecv),
    GOTCHA_WRAP_ACTION(MPI_Isend),
    GOTCHA_WRAP_ACTION(MPI_Irecv),
    GOTCHA_WRAP_ACTION(MPI_Waitall),
    GOTCHA_WRAP_ACTION(MPI_Waitsome),
    GOTCHA_WRAP_ACTION(MPI_Waitany),
    GOTCHA_WRAP_ACTION(MPI_Ssend),
    GOTCHA_WRAP_ACTION(MPI_Comm_split),
    GOTCHA_WRAP_ACTION(MPI_Comm_dup),
    GOTCHA_WRAP_ACTION(MPI_Comm_create),
    GOTCHA_WRAP_ACTION(MPI_Ibcast),
    GOTCHA_WRAP_ACTION(MPI_Test),
    GOTCHA_WRAP_ACTION(MPI_Testall),
    GOTCHA_WRAP_ACTION(MPI_Testsome),
    GOTCHA_WRAP_ACTION(MPI_Testany),
    GOTCHA_WRAP_ACTION(MPI_Ireduce),
    GOTCHA_WRAP_ACTION(MPI_Igather),
    GOTCHA_WRAP_ACTION(MPI_Iscatter),
    GOTCHA_WRAP_ACTION(MPI_Ialltoall),
    GOTCHA_WRAP_ACTION(MPI_Comm_free),
    GOTCHA_WRAP_ACTION(MPI_Cart_sub),
    GOTCHA_WRAP_ACTION(MPI_Comm_split_type)
};

struct gotcha_binding_t hdf5_wrap_actions [] = {
    GOTCHA_WRAP_ACTION(H5Fcreate),
    GOTCHA_WRAP_ACTION(H5Fopen),
    GOTCHA_WRAP_ACTION(H5Fclose),
    GOTCHA_WRAP_ACTION(H5Fflush),
    GOTCHA_WRAP_ACTION(H5Gclose),
    GOTCHA_WRAP_ACTION(H5Gcreate1),
    GOTCHA_WRAP_ACTION(H5Gcreate2),
    GOTCHA_WRAP_ACTION(H5Gget_objinfo),
    GOTCHA_WRAP_ACTION(H5Giterate),
    GOTCHA_WRAP_ACTION(H5Gopen1),
    GOTCHA_WRAP_ACTION(H5Gopen2),
    GOTCHA_WRAP_ACTION(H5Dclose),
    GOTCHA_WRAP_ACTION(H5Dcreate1),
    GOTCHA_WRAP_ACTION(H5Dcreate2),
    GOTCHA_WRAP_ACTION(H5Dget_create_plist),
    GOTCHA_WRAP_ACTION(H5Dget_space),
    GOTCHA_WRAP_ACTION(H5Dget_type),
    GOTCHA_WRAP_ACTION(H5Dopen1),
    GOTCHA_WRAP_ACTION(H5Dopen2),
    GOTCHA_WRAP_ACTION(H5Dread),
    GOTCHA_WRAP_ACTION(H5Dwrite),
    GOTCHA_WRAP_ACTION(H5Dset_extent),
    GOTCHA_WRAP_ACTION(H5Sclose),
    GOTCHA_WRAP_ACTION(H5Screate),
    GOTCHA_WRAP_ACTION(H5Screate_simple),
    GOTCHA_WRAP_ACTION(H5Sget_select_npoints),
    GOTCHA_WRAP_ACTION(H5Sget_simple_extent_dims),
    GOTCHA_WRAP_ACTION(H5Sget_simple_extent_npoints),
    GOTCHA_WRAP_ACTION(H5Sselect_elements),
    GOTCHA_WRAP_ACTION(H5Sselect_hyperslab),
    GOTCHA_WRAP_ACTION(H5Sselect_none),
    GOTCHA_WRAP_ACTION(H5Tclose),
    GOTCHA_WRAP_ACTION(H5Tcopy),
    GOTCHA_WRAP_ACTION(H5Tget_class),
    GOTCHA_WRAP_ACTION(H5Tget_size),
    GOTCHA_WRAP_ACTION(H5Tset_size),
    GOTCHA_WRAP_ACTION(H5Tcreate),
    GOTCHA_WRAP_ACTION(H5Tinsert),
    GOTCHA_WRAP_ACTION(H5Aclose),
    GOTCHA_WRAP_ACTION(H5Acreate1),
    GOTCHA_WRAP_ACTION(H5Acreate2),
    GOTCHA_WRAP_ACTION(H5Aget_name),
    GOTCHA_WRAP_ACTION(H5Aget_num_attrs),
    GOTCHA_WRAP_ACTION(H5Aget_space),
    GOTCHA_WRAP_ACTION(H5Aget_type),
    GOTCHA_WRAP_ACTION(H5Aopen),
    GOTCHA_WRAP_ACTION(H5Aopen_idx),
    GOTCHA_WRAP_ACTION(H5Aopen_name),
    GOTCHA_WRAP_ACTION(H5Aread),
    GOTCHA_WRAP_ACTION(H5Awrite),
    GOTCHA_WRAP_ACTION(H5Pclose),
    GOTCHA_WRAP_ACTION(H5Pcreate),
    GOTCHA_WRAP_ACTION(H5Pget_chunk),
    GOTCHA_WRAP_ACTION(H5Pget_mdc_config),
    GOTCHA_WRAP_ACTION(H5Pset_alignment),
    GOTCHA_WRAP_ACTION(H5Pset_chunk),
    GOTCHA_WRAP_ACTION(H5Pset_dxpl_mpio),
    GOTCHA_WRAP_ACTION(H5Pset_fapl_core),
    GOTCHA_WRAP_ACTION(H5Pset_fapl_mpio),
    GOTCHA_WRAP_ACTION(H5Pset_istore_k),
    GOTCHA_WRAP_ACTION(H5Pset_mdc_config),
    GOTCHA_WRAP_ACTION(H5Pset_meta_block_size),
    GOTCHA_WRAP_ACTION(H5Lexists),
    GOTCHA_WRAP_ACTION(H5Lget_val),
    GOTCHA_WRAP_ACTION(H5Literate),
    GOTCHA_WRAP_ACTION(H5Literate1),
    GOTCHA_WRAP_ACTION(H5Literate2),
    GOTCHA_WRAP_ACTION(H5Oclose),
    GOTCHA_WRAP_ACTION(H5Oopen),
    GOTCHA_WRAP_ACTION(H5Pset_coll_metadata_write),
    GOTCHA_WRAP_ACTION(H5Pget_coll_metadata_write),
    GOTCHA_WRAP_ACTION(H5Pset_all_coll_metadata_ops),
    GOTCHA_WRAP_ACTION(H5Pget_all_coll_metadata_ops)
};

void gotcha_register_functions() {
    char* posix_tracing_env = getenv(RECORDER_POSIX_TRACING);
    char* mpi_tracing_env   = getenv(RECORDER_MPI_TRACING);
    char* mpiio_tracing_env = getenv(RECORDER_MPIIO_TRACING);
    char* hdf5_tracing_env  = getenv(RECORDER_HDF5_TRACING);
    if (posix_tracing_env) posix_tracing = atoi(posix_tracing_env);
    if (mpi_tracing_env) mpi_tracing = atoi(mpi_tracing_env);
    if (mpiio_tracing_env) mpiio_tracing = atoi(mpiio_tracing_env);
    if (hdf5_tracing_env) hdf5_tracing = atoi(hdf5_tracing_env);

    if (posix_tracing)
        gotcha_wrap(posix_wrap_actions,
                    sizeof(posix_wrap_actions)/sizeof(struct gotcha_binding_t),
                    "recorder_posix_actions");
    if (mpi_tracing)
        gotcha_wrap(mpi_wrap_actions,
                    sizeof(mpi_wrap_actions)/sizeof(struct gotcha_binding_t),
                    "recorder_mpi_actions");
    if (mpiio_tracing)
        gotcha_wrap(mpiio_wrap_actions,
                    sizeof(mpiio_wrap_actions)/sizeof(struct gotcha_binding_t),
                    "recorder_mpiio_actions");
    if (hdf5_tracing)
        gotcha_wrap(hdf5_wrap_actions,
                    sizeof(hdf5_wrap_actions)/sizeof(struct gotcha_binding_t),
                    "recorder_hdf5_actions");
}

bool gotcha_posix_tracing() {
    return posix_tracing;
}
bool gotcha_mpi_tracing() {
    return mpi_tracing;
}
bool gotcha_mpiio_tracing() {
    return mpiio_tracing;
}
bool gotcha_hdf5_tracing() {
    return hdf5_tracing;
}

void gotcha_init() {
    gotcha_register_functions();
}
