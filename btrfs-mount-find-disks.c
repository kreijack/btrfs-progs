#define _XOPEN_SOURCE 500
#define _GNU_SOURCE 1

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/mount.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <blkid/blkid.h>
#include <uuid/uuid.h>
#include <libmount/libmount.h>

#include "crc32c.h"

#include "kerncompat.h"
#include "extent_io.h"
#include "ctree.h"
#include "disk-io.h"
#include "btrfs-mount.h"

#define BTRFS_UUID_UNPARSED_SIZE 37

/*
 * checks if a path is a block device node
 * Returns negative errno on failure, otherwise
 * returns 1 for blockdev, 0 for not-blockdev
 */
static int is_block_device(const char *path)
{
	struct stat statbuf;

	if (stat(path, &statbuf) < 0)
		return -errno;

	return S_ISBLK(statbuf.st_mode);
}

/* add a new btrfs_device to the list */
static void add_to_list(struct btrfs_device **head, struct btrfs_device *d)
{
	d->next = (*head);
	*head = d;
}

/* free a btrfs_device struct */
static void free_btrfs_device(struct btrfs_device *p)
{
	if (!p) return;

	free( p->device_name );
	free( p->device_uuid );
	free( p->fs_name );
	free( p->fs_uuid );
	free(p);
}

/* free a btrfs devices(s) list */
void free_btrfs_devices_list(struct btrfs_device **p)
{
	while (*p) {
		struct btrfs_device *next;
		next = (*p)->next;
		free_btrfs_device(*p);
		*p = next;
	}
}

/* TBD: from disk-io.c, should we get from a library ? */
static u32 csum_data(char *data, u32 seed, size_t len)
{
        return crc32c(seed, data, len);
}

static void csum_final(u32 crc, char *result)
{
	*(__le32 *)result = ~cpu_to_le32(crc);
}

static int check_csum_sblock(void *sb, int csum_size)
{
	char result[BTRFS_CSUM_SIZE];
	u32 crc = ~(u32)0;

	crc = csum_data((char *)sb + BTRFS_CSUM_SIZE,
				crc, BTRFS_SUPER_INFO_SIZE - BTRFS_CSUM_SIZE);
	csum_final(crc, result);

	return !memcmp(sb, &result, csum_size);
}

/*
 * Load and check superblock info
 * return values:
 * 0	ok
 * >0	sb content invalid
 * <0	other error
 */

static int load_and_check_sb_info(char *devname, char **dev_uuid, char **fs_uuid,
			char **fs_label, long long unsigned *num_devices,
			long long unsigned *generation) {

	u8 super_block_data[BTRFS_SUPER_INFO_SIZE];
	struct btrfs_super_block *sb;
	u64 ret;
	int fd;
	u64 sb_bytenr = btrfs_sb_offset(0);

	if (!is_block_device(devname)) {
		fprintf(stderr, "ERROR: '%s' is not a block device\n", devname);
		return -4;
	}

	fd = open(devname, O_RDONLY, 0666);
	if (fd < 0) {
		fprintf(stderr, "ERROR: Can't acces the device '%s'\n", devname);
		return -3;
	}
	sb = (struct btrfs_super_block *)super_block_data;

	ret = pread64(fd, super_block_data, BTRFS_SUPER_INFO_SIZE, sb_bytenr);
	close(fd);

	if (ret != BTRFS_SUPER_INFO_SIZE) {
		int e = errno;

		fprintf(stderr,
		   "ERROR: Failed to read the superblock on %s at %llu\n",
		   devname, (unsigned long long)sb_bytenr);
		fprintf(stderr,
		   "ERROR: error = '%s', errno = %d\n", strerror(e), e);
		return -4;
	}

	/*
	 * TBD: this would be the place to check for further superblock
	 * 	if the first one fails
	 */

	if (btrfs_super_magic(sb) != BTRFS_MAGIC) {
		fprintf(stderr, "ERROR: Failed check of BTRFS_MAGIC (device=%s)\n",
			devname);
		return 4;
	}

	if (!check_csum_sblock(sb, btrfs_super_csum_size(sb))) {
		fprintf(stderr, "ERROR: Failed check of checksum (device=%s)\n",
			devname);
		return 5;
	}

	*dev_uuid = malloc(BTRFS_UUID_UNPARSED_SIZE+1);
	*fs_uuid = malloc(BTRFS_UUID_UNPARSED_SIZE+1);
	*fs_label = strdup(sb->label);
	if (!*dev_uuid || !*fs_uuid || !*fs_label) {
		fprintf(stderr, "ERROR: not enough memory\n");
		return 6;
	}

	uuid_unparse(sb->fsid, *fs_uuid);
	uuid_unparse(sb->dev_item.uuid, *dev_uuid);

	*num_devices = (unsigned long long)btrfs_super_num_devices(sb);
	*generation = (unsigned long long)btrfs_super_generation(sb);
	return 0;

}

/*
 * this function extracts information from a device
 * 0	ok
 * >0	sb content invalid
 * <0	other error
 */
static int get_btrfs_dev_info(const char *devname, struct btrfs_device **devret)
{
	int ret=0;
	struct btrfs_device *device = NULL;

	*devret = NULL;

	device = calloc(sizeof(struct btrfs_device), 1);
	if (!device) {
		fprintf(stderr, "ERROR: not enough memory!\n");
		ret = -20;
		goto quit;
	}
	device->device_name = strdup(devname);
	if (!device->device_name) {
		fprintf(stderr, "ERROR: not enough memory!\n");
		ret = -21;
		goto quit;
	}

	ret = load_and_check_sb_info(device->device_name, &device->device_uuid,
		&device->fs_uuid, &device->fs_name,
		&device->num_devices, &device->generation);

quit:
	/* if failed, clean *device memory allocation */
	if (ret && device)
		free_btrfs_device(device);
	else
		*devret = device;

	return ret;
}

/*
 * 	this function get all the devices related to a filesystem
 * 	return values:
 * 	0 	-> OK
 * 	>0	-> sb error
 * 	<0	-> other error
 */
static int _get_devices_list(int flag, struct btrfs_device *device0,
	 blkid_cache *bcache)
{
	/*blkid_cache 		bcache;*/
	blkid_dev_iterate	bit;
	blkid_dev 		bdev;

	int 			ret=0;
	struct btrfs_device *devices=NULL;

	assert(device0);

	bit = blkid_dev_iterate_begin(*bcache);
	if (blkid_dev_set_search(bit, "UUID", device0->fs_uuid)) {
		fprintf(stderr,"ERROR: unable to setup blkid_dev_set_search()\n");
		ret = -4;
		goto exit;
	}

	while (!blkid_dev_next(bit, &bdev)) {
		struct btrfs_device *p;
		const char *dev = strdup(blkid_dev_devname(bdev));

		if ((ret = get_btrfs_dev_info(dev, &p)) != 0)
			break;

		if (!strcmp(device0->device_name, p->device_name))
			continue;

		add_to_list(&devices, p);
	}

exit:
	blkid_dev_iterate_end(bit);
	if (ret && devices) {
		free_btrfs_devices_list(&devices);
	}

	blkid_dev_iterate_end(bit);
	device0->next = devices;

	return ret;
}

/*
 * Check that the superblock info are coherent and the device are enough
 *
 * <0		error
 * 0		ok
 * >0		not enough device, retry
 */
static int check_devices(struct btrfs_device *l)
{
	u64 c;
	int e=0;
	struct btrfs_device *p;

	assert(l);

	/* check the superblocks disk count*/
	p = l->next;
	while (p) {
		if (p->num_devices != l->num_devices) {
			fprintf(stderr, "ERROR: "
				"superblock number of device mismatch (device=%s)",
				p->device_name);
				e--;
		}
		p = p-> next;
	}
	if (e)
		return e;

	/* check for the superblock disk uuid */
	for (p = l ; p ; p = p->next ) {
		struct btrfs_device *p2 = p->next;
		while (p2) {
			if (!strcmp(p->device_uuid, p2->device_uuid)) {
				fprintf(stderr, "ERROR: "
					"disk '%s' and '%s' have the same disk uuid\n",
					p->device_name, p2->device_name);
					e--;
			}
			p2 = p2->next;
		}
	}

	if (e)
		return e;

	for ( c=0, p=l ; p ; p= p->next, c++ ) ;
	if (c > l->num_devices) {
		fprintf(stderr, "ERROR: found more device than required.\n");
		return -1;
	}

	/* not enough device; wait for further */
	if (c < l->num_devices)
		return 1;

	return 0;

}

/*
 * 	this function get info for a device)
 * 	return values:
 * 	0 	-> OK
 * 	<0	-> error
 */
int get_device_info(char *spec, struct btrfs_device **device)
{
	blkid_cache	bcache;
	int 		ret;
	char		*dev;

	if (blkid_get_cache(&bcache, NULL)) {
		fprintf(stderr, "ERROR: cannot get blkid cache\n");
		return -1;
	}

	if (!strncmp(spec, "LABEL=", 6)) {
		dev = blkid_evaluate_tag("LABEL", spec+6, &bcache);
	} else if (!strncmp(spec, "UUID=", 5)) {
		dev = blkid_evaluate_tag("UUID", spec+5, &bcache);
	} else {
		dev = strdup(spec);
	}

	ret = get_btrfs_dev_info(dev, device);
	if (ret)
		ret = 1;
	blkid_put_cache(bcache);
	return ret;

}

/*
 * 	this function get all the devices related to a filesystem
 * 	return values:
 * 	0 	-> OK
 * 	400	-> not enough disk
 * 	>0	-> error on the sb content
 *	<0	-> other error
 */
int get_devices_list(int flag, struct btrfs_device *device0, int timeout)
{
	blkid_cache	bcache = NULL;
	int 		ret;
	int		first=1;

	assert(device0);
	assert(device0->num_devices > 1);

	if (blkid_get_cache(&bcache, NULL)) {
		fprintf(stderr, "ERROR: cannot get blkid cache\n");
		return -1;
	}

	do {

		free_btrfs_devices_list(&device0->next);
		ret = _get_devices_list(flag, device0, &bcache);

		if (ret)
			break;

		/* check if the devices are ok */
		ret = check_devices(device0);

		/* all ok */
		if (!ret)
			break;

		/*
		 * error or not enough device: regenerate cache and
		 * try another time
		 */
		if (first) {
			free(bcache);
			if (blkid_get_cache(&bcache, "/dev/null")) {
				free_btrfs_devices_list(&device0->next);
				fprintf(stderr, "ERROR: cannot get blkid cache\n");
				return -1;
			}
			blkid_probe_all_new(bcache);
			first = 0;
			continue;
		}

		/* error even with cache regenerate */
		if (ret < 0)
			break;

		if (flag & MOUNT_FLAG_VERBOSE) {
			struct btrfs_device *p;
			printf("INFO: sleep 1s [timeout=%ds][", timeout);
			for (p=device0 ; p ; p=p->next) {
				char *dev = p->device_name;
				if (!strncmp(dev, "/dev/", 5))
					dev += 5;
				printf("%s", dev);
				if (p->next)
					printf(",");
			}
			printf(" / %llu]\n",device0->num_devices);
		}
		/* wait and check for new entryes */
		sleep(1);
		timeout--;
		blkid_probe_all_new(bcache);

	}while(timeout>0);

	if (timeout <=0) {
		fprintf(stderr, "WARNING: not enough devices\n");
		ret = 400;
	}

	free(bcache);
	return ret;
}


