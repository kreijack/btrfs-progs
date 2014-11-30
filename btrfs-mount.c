#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/mount.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <blkid/blkid.h>
#include <libmount/libmount.h>

#include "btrfs-mount.h"

/* Parse program args, and set the related variables */
static int parse_args(int argc, char **argv, char **options,
				char **spec, char **dir, int *flag)
{
	char	opt;

	*options = NULL;

	while ((opt = getopt(argc, argv, "sfnvo:")) != -1) {

		switch (opt) {

		case 's':	/* tolerate sloppy mount options */
			*flag |= MOUNT_FLAG_IGNORE_SLOPPY_OPTS;
			break;
		case 'f':	/* fake mount */
			*flag |= MOUNT_FLAG_FAKE_MOUNT;
			break;
		case 'n':	/* mount without writing in mtab */
			*flag |= MOUNT_FLAG_NOT_WRITIING_MTAB;
			break;
		case 'v':	/* verbose */
			*flag |= MOUNT_FLAG_VERBOSE;
			break;
		case 'o':
			*options = optarg;
			break;
		default:
			fprintf( stderr,"ERROR: unknown option: '%c'\n", opt);
			return 1;
		}
	}

	if (argc-optind != 2) {
		fprintf(stderr, "ERROR: two arguments are needed\n");
		return 1;
	}

	*spec = argv[optind];
	*dir  = argv[optind+1];

	return 0;

}

/* joins two options string */
static int join_options(char **dst, char *fs_opts, char *vfs_opts)
{
	int l1=0, l2=0;

	if (fs_opts && *fs_opts)
		l1 = strlen(fs_opts);

	if (vfs_opts && *vfs_opts)
		l2 = strlen(vfs_opts);

	if (!l1 && !l2) {
		*dst = strdup("");
		return *dst == NULL;
	} else if(!l1) {
		*dst = strdup(vfs_opts);
		return *dst == NULL;
	} else if(!l2) {
		*dst = strdup(fs_opts);
		return *dst == NULL;
	} else {

		*dst = calloc(l1+l2+2, 1);
		if (!*dst)
			return 3;

		strcpy(*dst, fs_opts);
		strcat(*dst, ",");
		strcat(*dst, vfs_opts);

		return 0;
	}

}

/*
 * This function rearrange the options
 * 1) removes from "options":
 * - the vfs_options (which became bits in mount_flags)
 * - eventually device=<xxx> options passed (these aren't used)
 * 2) adds to "options" a true list of device=<xxx>
 * 3) put all the options in all_options, which will be used in
 *    updating mtab
 */
static int rearrange_options(int flags, char **options,
			     unsigned long *mount_flags,
			     char **all_options,
			     struct btrfs_device *devices)
{
	int 	rc;
	char	*user_opts=NULL, *vfs_opts=NULL, *fs_opts=NULL;
	int 	ret=0;
	struct btrfs_device *device;

	*all_options = NULL;

	rc = mnt_split_optstr(*options, &user_opts, &vfs_opts, &fs_opts, 0, 0);
	if (rc) {
		fprintf(stderr, "ERROR: not enough memory\n");
		ret = 1;
		goto exit;
	}

        rc = mnt_optstr_get_flags(vfs_opts, mount_flags,
				  mnt_get_builtin_optmap(MNT_LINUX_MAP));
        if (rc) {
		fprintf(stderr, "ERROR: not enough memory\n");
		ret = 2;
		goto exit;
	}

	/*
	 * If additional devices are passed via option,
	 * the device scan is NOT performed
	 */
	if (devices) {

		/* skip the first device, but append additional devices */
		device = devices->next;
		while (device) {
			rc = mnt_optstr_append_option(&fs_opts,
				"device", device->device_name);
			if (rc) {
				fprintf(stderr, "ERROR: not enough memory\n");
				ret = 4;
				goto exit;
			}
			device = device->next;
		}
	}

	if (mnt_optstr_remove_option(&fs_opts, DEVICE_TIMEOUT_OPTS) < 0 ) {
		fprintf(stderr, "ERROR: not enough memory\n");
		ret = 4;
		goto exit;
	}

	if (join_options(all_options, fs_opts, vfs_opts)) {
		fprintf(stderr, "ERROR: not enough memory\n");
		ret = 4;
		goto exit;
	}

	*options = fs_opts;
	fs_opts = NULL;

exit:
	free(vfs_opts);
	free(fs_opts);
	free(user_opts);
	return ret;

}

/* this function update the mtab file (if needed )*/
static int update_mtab(int flags, char *device, char *target, char *all_opts )
{

	struct libmnt_fs	*fs = NULL;
	struct libmnt_update	*update = NULL;

	char			*vfs_opts = NULL;
	int			ret = 0, rc;

	fs = mnt_new_fs();
	if (!fs)
		goto memoryerror;
	if (mnt_fs_set_options(fs, all_opts))
		goto memoryerror;
	if (mnt_fs_set_source(fs, device))
		goto memoryerror;
	if (mnt_fs_set_target(fs, target))
		goto memoryerror;
	if (mnt_fs_set_fstype(fs, "btrfs"))
		goto memoryerror;

	if (!(update = mnt_new_update()))
		goto memoryerror;

	rc = mnt_update_set_fs(update, 0, NULL, fs);

	if (rc == 1) {
		/* FIXME: check the reason that rc is always 1 */
		/*fprintf(stderr, "WARNING: update of mtab not needed\n");*/
		ret = 0;
		goto exit;
	} else if (rc) {
		fprintf(stderr, "ERROR: failed to set fs\n");
		ret = 10;
		goto exit;
	}

	ret = mnt_update_table(update, NULL);
	if (ret)
		fprintf(stderr, "ERROR: failed to update mtab\n");
	else if (flags & MOUNT_FLAG_VERBOSE)
		printf("INFO: 'mtab' updated\n");
	goto exit;

memoryerror:
	fprintf(stderr, "ERROR: not enough memory\n");
	if (fs)     mnt_free_fs(fs);
	if (update) mnt_free_update(update);

	free(vfs_opts);

	return 100;

exit:
	if (fs)     mnt_free_fs(fs);
	if (update) mnt_free_update(update);

	free(vfs_opts);

	return ret;
}

int main(int argc, char **argv)
{

	char *fs_opts, *spec, *dir, *all_options;
	int ret, flags=0;
	struct btrfs_device *devices;
	unsigned long mount_flags = 0;
	size_t size;
	int try_degraded = 0;
	char *value;
	int explicit_devices=0;
	int timeout=DEVICE_TIMEOUT;

	ret = parse_args(argc, argv, &fs_opts, &spec, &dir, &flags);

	if (ret)
		goto incorrect_invocation;

	if (!mnt_optstr_get_option(fs_opts, DEGRADED_OPTS,&value, &size))
		try_degraded = 1;

	if (!mnt_optstr_get_option(fs_opts, "device", &value, &size))
		explicit_devices = 1;

	if (!mnt_optstr_get_option(fs_opts, DEVICE_TIMEOUT_OPTS, &value,
		&size)) {
		if (sscanf(value, "%d", &timeout) != 1 || timeout < 1) {
			fprintf(stderr, "ERROR: error parsing '"
				DEVICE_TIMEOUT_OPTS
				"' option\n");
			goto incorrect_invocation;
		}
	}

	if (flags & MOUNT_FLAG_VERBOSE)
		printf("INFO: scan the first device\n");
	/*
	 * get_devices_info returns the "spec" device
	 */
	ret = get_device_info(spec, &devices);
	if (ret>0)
		goto mountfailure;
	if (ret<0)
		goto internalerror;

	if (flags & MOUNT_FLAG_VERBOSE)
		printf("INFO: find filesystem '%s' [%s]\n",
			devices->fs_name, devices->fs_uuid);

	assert(devices != NULL);

	if (!explicit_devices && devices->num_devices>1) {
		/*
		 * get_devices_list() must returns at least the "spec" device
		 */
		ret = get_devices_list(flags, devices, timeout);
		if (ret<0)
			goto mountfailure;
		assert(devices != NULL);
	}

	ret = rearrange_options(flags, &fs_opts, &mount_flags,
		&all_options, NULL);
	if (ret)
		goto internalerror;

	if (flags & MOUNT_FLAG_VERBOSE) {
		char *vfs_opts=NULL;
		struct btrfs_device *p;
		printf("INFO: source: %s\n",  devices->device_name);
		printf("INFO: target: %s\n",  dir);
		mnt_optstr_apply_flags(&vfs_opts, mount_flags,
			mnt_get_builtin_optmap(MNT_LINUX_MAP));
		printf("INFO: vfs_opts: 0x%08lx - %s\n",
		       mount_flags, vfs_opts);
		printf("INFO: fs_opts: %s\n", fs_opts);
		free(vfs_opts);

		for (p = devices ; p ; p = p-> next )
			printf("INFO:    dev='%s' UUID='%s' gen=%llu\n",
				p->device_name,
				p->device_uuid,
				p->generation);
	}

	if (flags & MOUNT_FLAG_FAKE_MOUNT) {
		printf("INFO: FAKE mount\n");
		exit(0);
	}

	if (!explicit_devices) {
		/*
		 * check the number of devices
		 */
		unsigned long long c = 0;
		struct btrfs_device *dev;
		for (dev = devices ; dev ; dev = dev->next)
			c++;
		if (c != devices->num_devices) {
			if (try_degraded) {
				fprintf(stderr, "WARNING: "
					"required %llu disks, only %llu found\n"
					"WARNING: mount in degraded mode\n",
					devices->num_devices, c);
			} else {
				fprintf(stderr, "ERROR: "
					"required %llu disks, only %llu found\n",
					devices->num_devices, c);

				goto mountfailure;
			}
		}

		for (dev = devices->next ; dev ; dev = dev->next)
			if (dev->generation != devices->generation) {
				fprintf(stderr, "WARNING: generation numbers mismatch.\n");
				break;
			}
	}

	ret = mount(devices->device_name, dir, "btrfs", mount_flags,
		fs_opts);
	if (ret) {
		int e = errno;
		fprintf(stderr, "ERROR: mount failed : %d - %s\n",
			e, strerror(e));
		goto mountfailure;
	}
	if (!(flags & MOUNT_FLAG_NOT_WRITIING_MTAB)) {
		ret = update_mtab(flags, devices->device_name, dir,
			all_options);
		/* update_mtab error messages alredy printed */
		if (ret)
			goto errormtab;
	}

	if (flags & MOUNT_FLAG_VERBOSE)
		printf("INFO: mount succeded\n");

	exit(0);

mountfailure:
	exit(32);

errormtab:
	exit(16);

internalerror:
	exit(2);
incorrect_invocation:
	exit(1);

}
