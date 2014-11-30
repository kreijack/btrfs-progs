
#define MOUNT_FLAG_FAKE_MOUNT		1
#define MOUNT_FLAG_VERBOSE		2
#define MOUNT_FLAG_NOT_WRITIING_MTAB	4
#define MOUNT_FLAG_IGNORE_SLOPPY_OPTS	8


/* seconds to wait for devices */
#define DEVICE_TIMEOUT		10
#define DEVICE_TIMEOUT_OPTS	"device_timeout"

#define DEGRADED_OPTS	"degraded"

struct btrfs_device {
	char			*device_name;
	char			*device_uuid;
	char 			*fs_name;
	char			*fs_uuid;
	long long unsigned	num_devices;
	struct btrfs_device	*next;
	unsigned long long	generation;
};

/* free a btrfs devices(s) list */
void free_btrfs_devices_list(struct btrfs_device **p);

/* load devices info */
int get_devices_list(int flag, struct btrfs_device *devices, int timeout);
/* load device info */
int get_device_info(char *spec, struct btrfs_device **device);

#define DEBUG 1

#ifdef DEBUG

  #define DPRINTF(x...) \
	do { fprintf(stderr,"DPRINTF: %s()@%s,%d: ", __FUNCTION__, \
		__FILE__, __LINE__); \
		fprintf(stderr, x); \
	}while(0)

#else

  #define DPRINTF(x...)

#endif

