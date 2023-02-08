/* ns_show.c

    Licensed under the GNU General Public License v2 or later.
*/
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <linux/sched.h>

#ifndef NS_GET_USERNS
#define NSIO    0xb7
#define NS_GET_USERNS       _IO(NSIO, 0x1)
#define NS_GET_PARENT       _IO(NSIO, 0x2)
#define NS_GET_NSTYPE       _IO(NSIO, 0x3)
#define NS_GET_OWNER_UID    _IO(NSIO, 0x4)
#endif

int
main(int argc, char *argv[])
{
    int fd, userns_fd, parent_fd, nstype;
    uid_t uid;
    struct stat sb;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s /proc/[pid]/ns/[file] [p|u|o]\n",
                argv[0]);
        fprintf(stderr, "\nDisplay the result of one or both "
                "of NS_GET_USERNS (u) or NS_GET_PARENT (p) or NS_GET_OWNER_UID (o)\n"
                "for the specified /proc/[pid]/ns/[file]. If neither "
                "'p' nor 'u' nor 'o' is specified,\n"
                "NS_GET_USERNS is the default.\n");
        exit(EXIT_FAILURE);
    }

    /* Obtain a file descriptor for the 'ns' file specified
        in argv[1]. */

    fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    /* Obtain a file descriptor for the owning user namespace and
        then obtain and display the inode number of that namespace. */

    if (argc < 3 || strchr(argv[2], 'u')) {
        userns_fd = ioctl(fd, NS_GET_USERNS);

        if (userns_fd == -1) {
            if (errno == EPERM)
                printf("The owning user namespace is outside "
                        "your namespace scope\n");
            else
                perror("ioctl-NS_GET_USERNS");
            exit(EXIT_FAILURE);
        }

        if (fstat(userns_fd, &sb) == -1) {
            perror("fstat-userns");
            exit(EXIT_FAILURE);
        }
        printf("Device/Inode of owning user namespace is: "
                "[%jx,%jx] / %ju\n",
                (uintmax_t) major(sb.st_dev),
                (uintmax_t) minor(sb.st_dev),
                (uintmax_t) sb.st_ino);

        close(userns_fd);
    }

    /* Obtain a file descriptor for the parent namespace and
        then obtain and display the inode number of that namespace. */

    if (argc > 2 && strchr(argv[2], 'p')) {
        parent_fd = ioctl(fd, NS_GET_PARENT);

        if (parent_fd == -1) {
            if (errno == EINVAL)
                printf("Can' get parent namespace of a "
                        "nonhierarchical namespace\n");
            else if (errno == EPERM)
                printf("The parent namespace is outside "
                        "your namespace scope\n");
            else
                perror("ioctl-NS_GET_PARENT");
            exit(EXIT_FAILURE);
        }

        if (fstat(parent_fd, &sb) == -1) {
            perror("fstat-parentns");
            exit(EXIT_FAILURE);
        }
        printf("Device/Inode of parent namespace is: [%jx,%jx] / %ju\n",
                (uintmax_t) major(sb.st_dev),
                (uintmax_t) minor(sb.st_dev),
                (uintmax_t) sb.st_ino);

        close(parent_fd);
    }

    if (argc > 2 && strchr(argv[2], 'o')) {
        nstype = ioctl(fd, NS_GET_NSTYPE);

        if (nstype != CLONE_NEWUSER) {
            printf("The namespace is not user namespace\n");
            exit(EXIT_FAILURE);
        }

        ioctl(fd, NS_GET_OWNER_UID, &uid);
        printf("The owner user ID is %d\n", uid);
    }
    close(fd);
    exit(EXIT_SUCCESS);
}
