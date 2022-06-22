//! ----------------------------------------------------------------------------
//! Copyright Edgio Inc.
//!
//! \file:    TODO
//! \details: TODO
//!
//! Licensed under the terms of the Apache 2.0 open source license.
//! Please refer to the LICENSE file in the project root for the terms.
//! ----------------------------------------------------------------------------
//! ----------------------------------------------------------------------------
//! includes
//! ----------------------------------------------------------------------------
#include "status.h"
#include "support/file_util.h"
#include "support/nbq.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
namespace ns_hurl {
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
int32_t read_file(const char *a_file, char **a_buf, uint32_t *a_len)
{
        // Check is a file
        struct stat l_stat;
        int32_t l_s = STATUS_OK;
        l_s = stat(a_file, &l_stat);
        if(l_s != 0)
        {
                printf("Error performing stat on file: %s.  Reason: %s\n", a_file, strerror(errno));
                return STATUS_ERROR;
        }
        // Check if is regular file
        if(!(l_stat.st_mode & S_IFREG))
        {
                printf("Error opening file: %s.  Reason: is NOT a regular file\n", a_file);
                return STATUS_ERROR;
        }
        // Open file...
        FILE * l_file;
        l_file = fopen(a_file,"r");
        if (NULL == l_file)
        {
                printf("Error opening file: %s.  Reason: %s\n", a_file, strerror(errno));
                return STATUS_ERROR;
        }
        // Read in file...
        int32_t l_size = l_stat.st_size;
        *a_buf = (char *)malloc(sizeof(char)*l_size);
        *a_len = l_size;
        int32_t l_read_size;
        l_read_size = fread(*a_buf, 1, l_size, l_file);
        if(l_read_size != l_size)
        {
                printf("Error performing fread.  Reason: %s [%d:%d]\n",
                                strerror(errno), l_read_size, l_size);
                return STATUS_ERROR;
        }
        // Close file...
        l_s = fclose(l_file);
        if (STATUS_OK != l_s)
        {
                printf("Error performing fclose.  Reason: %s\n", strerror(errno));
                return STATUS_ERROR;
        }
        return STATUS_OK;
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
int32_t read_file_nbq(nbq &ao_nbq, uint32_t &ao_len, const char *a_file)
{
        // Check is a file
        struct stat l_stat;
        int32_t l_s = STATUS_OK;
        l_s = stat(a_file, &l_stat);
        if(l_s != 0)
        {
                printf("Error performing stat on file: %s.  Reason: %s\n", a_file, strerror(errno));
                return STATUS_ERROR;
        }
        // Check if is regular file
        if(!(l_stat.st_mode & S_IFREG))
        {
                printf("Error opening file: %s.  Reason: is NOT a regular file\n", a_file);
                return STATUS_ERROR;
        }
        // Open file...
        int l_fd;
        l_fd = open(a_file, O_RDONLY);
        if(l_fd == -1)
        {
                printf("Error opening file: %s.  Reason: %s\n", a_file, strerror(errno));
                return STATUS_ERROR;
        }
        // Read in file...
        ssize_t l_status;
        int64_t l_write_size;
        l_write_size = ao_nbq.write_fd(l_fd, l_stat.st_size, l_status);
        ao_len = l_write_size;
        // Close file...
        l_s = close(l_fd);
        if (STATUS_OK != l_s)
        {
                printf("Error performing close.  Reason: %s\n", strerror(errno));
                return STATUS_ERROR;
        }
        return STATUS_OK;
}
}
