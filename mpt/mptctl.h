/*
 *  linux/drivers/message/fusion/mptioctl.h
 *      Fusion MPT misc device (ioctl) driver.
 *      For use with PCI chip/adapter(s):
 *          LSIFC9xx/LSI409xx Fibre Channel
 *      running LSI Logic Fusion MPT (Message Passing Technology) firmware.
 *
 *  Copyright (c) 1999-2005 LSI Logic Corporation
 *  (mailto:mpt_linux_developer@lsil.com)
 *
 *  $Id: mptctl.h,v 1.14 2003/03/18 22:49:51 Exp $
 */
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; version 2 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    NO WARRANTY
    THE PROGRAM IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR
    CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED INCLUDING, WITHOUT
    LIMITATION, ANY WARRANTIES OR CONDITIONS OF TITLE, NON-INFRINGEMENT,
    MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. Each Recipient is
    solely responsible for determining the appropriateness of using and
    distributing the Program and assumes all risks associated with its
    exercise of rights under this Agreement, including but not limited to
    the risks and costs of program errors, damage to or loss of data,
    programs or equipment, and unavailability or interruption of operations.

    DISCLAIMER OF LIABILITY
    NEITHER RECIPIENT NOR ANY CONTRIBUTORS SHALL HAVE ANY LIABILITY FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING WITHOUT LIMITATION LOST PROFITS), HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
    TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
    USE OR DISTRIBUTION OF THE PROGRAM OR THE EXERCISE OF ANY RIGHTS GRANTED
    HEREUNDER, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef MPTCTL_H_INCLUDED
#define MPTCTL_H_INCLUDED
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

// #include "linux/version.h"


/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *
 */
#define MPT_MISCDEV_BASENAME            "mptctl"
#define MPT_MISCDEV_PATHNAME            "/dev/" MPT_MISCDEV_BASENAME
#define MPT_CSMI_DESCRIPTION	        "LSI Logic Corporation: Fusion MPT Driver "MPT_LINUX_VERSION_COMMON

#define MPT_PRODUCT_LENGTH              12

/*
 *  Generic MPT Control IOCTLs and structures
 */
#define MPT_MAGIC_NUMBER	'm'

#define MPTRWPERF		_IOWR(MPT_MAGIC_NUMBER,0,struct mpt_raw_r_w)

#define MPTFWDOWNLOAD		_IOWR(MPT_MAGIC_NUMBER,15,struct mpt_fw_xfer)
#define MPTFWDOWNLOADBOOT	_IOWR(MPT_MAGIC_NUMBER,16,struct mpt_fw_xfer)
#define MPTCOMMAND		_IOWR(MPT_MAGIC_NUMBER,20,struct mpt_ioctl_command)

#if defined(__KERNEL__) && defined(CONFIG_COMPAT)
#define MPTFWDOWNLOAD32		_IOWR(MPT_MAGIC_NUMBER,15,struct mpt_fw_xfer32)
#define MPTCOMMAND32		_IOWR(MPT_MAGIC_NUMBER,20,struct mpt_ioctl_command32)
#endif

#define MPTIOCINFO		_IOWR(MPT_MAGIC_NUMBER,17,struct mpt_ioctl_iocinfo)
#define MPTIOCINFO1		_IOWR(MPT_MAGIC_NUMBER,17,struct mpt_ioctl_iocinfo_rev0)
#define MPTIOCINFO2		_IOWR(MPT_MAGIC_NUMBER,17,struct mpt_ioctl_iocinfo_rev1)
#define MPTTARGETINFO		_IOWR(MPT_MAGIC_NUMBER,18,struct mpt_ioctl_targetinfo)
#define MPTTEST			_IOWR(MPT_MAGIC_NUMBER,19,struct mpt_ioctl_test)
#define MPTEVENTQUERY		_IOWR(MPT_MAGIC_NUMBER,21,struct mpt_ioctl_eventquery)
#define MPTEVENTENABLE		_IOWR(MPT_MAGIC_NUMBER,22,struct mpt_ioctl_eventenable)
#define MPTEVENTREPORT		_IOWR(MPT_MAGIC_NUMBER,23,struct mpt_ioctl_eventreport)
#define MPTHARDRESET		_IOWR(MPT_MAGIC_NUMBER,24,struct mpt_ioctl_diag_reset)
#define MPTFWREPLACE		_IOWR(MPT_MAGIC_NUMBER,25,struct mpt_ioctl_replace_fw)
#define MPTDIAGREGISTER		_IOWR(MPT_MAGIC_NUMBER,26,mpt_diag_register_t)
#define MPTDIAGRELEASE		_IOWR(MPT_MAGIC_NUMBER,27,mpt_diag_release_t)
#define MPTDIAGUNREGISTER	_IOWR(MPT_MAGIC_NUMBER,28,mpt_diag_unregister_t)
#define MPTDIAGQUERY		_IOWR(MPT_MAGIC_NUMBER,29,mpt_diag_query_t)
#define MPTDIAGREADBUFFER	_IOWR(MPT_MAGIC_NUMBER,30,mpt_diag_read_buffer_t)

/*
 * SPARC PLATFORM REMARKS:
 * IOCTL data structures that contain pointers
 * will have different sizes in the driver and applications
 * (as the app. will not use 8-byte pointers).
 * Apps should use MPTFWDOWNLOAD and MPTCOMMAND.
 * The driver will convert data from
 * mpt_fw_xfer32 (mpt_ioctl_command32) to mpt_fw_xfer (mpt_ioctl_command)
 * internally.
 *
 * If data structures change size, must handle as in IOCGETINFO.
 */
struct mpt_fw_xfer {
	unsigned int	 iocnum;	/* IOC unit number */
	unsigned int	 fwlen;
	void		__user *bufp;	/* Pointer to firmware buffer */
};

#if defined(__KERNEL__) && defined(CONFIG_COMPAT)
struct mpt_fw_xfer32 {
	unsigned int iocnum;
	unsigned int fwlen;
	u32 bufp;
};
#endif	/*}*/

/*
 *  IOCTL header structure.
 *  iocnum - must be defined.
 *  port - must be defined for all IOCTL commands other than MPTIOCINFO
 *  maxDataSize - ignored on MPTCOMMAND commands
 *		- ignored on MPTFWREPLACE commands
 *		- on query commands, reports the maximum number of bytes to be returned
 *		  to the host driver (count includes the header).
 *		  That is, set to sizeof(struct mpt_ioctl_iocinfo) for fixed sized commands.
 *		  Set to sizeof(struct mpt_ioctl_targetinfo) + datasize for variable
 *			sized commands. (MPTTARGETINFO, MPTEVENTREPORT)
 */
typedef struct _mpt_ioctl_header {
	unsigned int	 iocnum;	/* IOC unit number */
	unsigned int	 port;		/* IOC port number */
	int		 maxDataSize;	/* Maximum Num. bytes to transfer on read */
} mpt_ioctl_header;

/*
 * Issue a diagnostic reset
 */
struct mpt_ioctl_diag_reset {
	mpt_ioctl_header hdr;
};


/*
 *  PCI bus/device/function information structure.
 */
struct mpt_ioctl_pci_info {
	union {
		struct {
			unsigned int  deviceNumber   :  5;
			unsigned int  functionNumber :  3;
			unsigned int  busNumber      : 24;
		} bits;
		unsigned int  asUlong;
	} u;
};

struct mpt_ioctl_pci_info2 {
	union {
		struct {
			unsigned int  deviceNumber   :  5;
			unsigned int  functionNumber :  3;
			unsigned int  busNumber      : 24;
		} bits;
		unsigned int  asUlong;
	} u;
  int segmentID;
};

/*
 *  Adapter Information Page
 *  Read only.
 *  Data starts at offset 0xC
 */
#define MPT_IOCTL_INTERFACE_SCSI	(0x00)
#define MPT_IOCTL_INTERFACE_FC		(0x01)
#define MPT_IOCTL_INTERFACE_FC_IP	(0x02)
#define MPT_IOCTL_INTERFACE_SAS		(0x03)
#define MPT_IOCTL_VERSION_LENGTH	(32)

struct mpt_ioctl_iocinfo {
	mpt_ioctl_header hdr;
	int		 adapterType;	/* SCSI or FCP */
	int		 port;		/* port number */
	int		 pciId;		/* PCI Id. */
	int		 hwRev;		/* hardware revision */
	int		 subSystemDevice;	/* PCI subsystem Device ID */
	int		 subSystemVendor;	/* PCI subsystem Vendor ID */
	int		 numDevices;		/* number of devices */
	int		 FWVersion;		/* FW Version (integer) */
	int		 BIOSVersion;		/* BIOS Version (integer) */
	char		 driverVersion[MPT_IOCTL_VERSION_LENGTH];	/* Driver Version (string) */
	char		 busChangeEvent;
	char		 hostId;
	char		 rsvd[2];
	struct mpt_ioctl_pci_info2  pciInfo; /* Added Rev 2 */
};

struct mpt_ioctl_iocinfo_rev1 {
	mpt_ioctl_header hdr;
	int		 adapterType;	/* SCSI or FCP */
	int		 port;		/* port number */
	int		 pciId;		/* PCI Id. */
	int		 hwRev;		/* hardware revision */
	int		 subSystemDevice;	/* PCI subsystem Device ID */
	int		 subSystemVendor;	/* PCI subsystem Vendor ID */
	int		 numDevices;		/* number of devices */
	int		 FWVersion;		/* FW Version (integer) */
	int		 BIOSVersion;		/* BIOS Version (integer) */
	char		 driverVersion[MPT_IOCTL_VERSION_LENGTH];	/* Driver Version (string) */
	char		 busChangeEvent;
	char		 hostId;
	char		 rsvd[2];
	struct mpt_ioctl_pci_info  pciInfo; /* Added Rev 1 */
};

/* Original structure, must always accept these
 * IOCTLs. 4 byte pads can occur based on arch with
 * above structure. Wish to re-align, but cannot.
 */
struct mpt_ioctl_iocinfo_rev0 {
	mpt_ioctl_header hdr;
	int		 adapterType;	/* SCSI or FCP */
	int		 port;		/* port number */
	int		 pciId;		/* PCI Id. */
	int		 hwRev;		/* hardware revision */
	int		 subSystemDevice;	/* PCI subsystem Device ID */
	int		 subSystemVendor;	/* PCI subsystem Vendor ID */
	int		 numDevices;		/* number of devices */
	int		 FWVersion;		/* FW Version (integer) */
	int		 BIOSVersion;		/* BIOS Version (integer) */
	char		 driverVersion[MPT_IOCTL_VERSION_LENGTH];	/* Driver Version (string) */
	char		 busChangeEvent;
	char		 hostId;
	char		 rsvd[2];
};

/*
 * Device Information Page
 * Report the number of, and ids of, all targets
 * on this IOC.  The ids array is a packed structure
 * of the known targetInfo.
 * bits 31-24: reserved
 *      23-16: LUN
 *      15- 8: Bus Number
 *       7- 0: Target ID
 */
struct mpt_ioctl_targetinfo {
	mpt_ioctl_header hdr;
	int		 numDevices;	/* Num targets on this ioc */
	int		 targetInfo[1];
};


/*
 * Event reporting IOCTL's.  These IOCTL's will
 * use the following defines:
 */
struct mpt_ioctl_eventquery {
	mpt_ioctl_header hdr;
	unsigned short	 eventEntries;
	unsigned short	 reserved;
	unsigned int	 eventTypes;
};

struct mpt_ioctl_eventenable {
	mpt_ioctl_header hdr;
	unsigned int	 eventTypes;
};

#ifndef __KERNEL__
typedef struct {
	uint	event;
	uint	eventContext;
	uint	data[2];
} MPT_IOCTL_EVENTS;
#endif

struct mpt_ioctl_eventreport {
	mpt_ioctl_header	hdr;
	MPT_IOCTL_EVENTS	eventData[1];
};

#define MPT_MAX_NAME	32
struct mpt_ioctl_test {
	mpt_ioctl_header hdr;
	u8		 name[MPT_MAX_NAME];
	int		 chip_type;
	u8		 product [MPT_PRODUCT_LENGTH];
};

/* Replace the FW image cached in host driver memory
 * newImageSize - image size in bytes
 * newImage - first byte of the new image
 */
typedef struct mpt_ioctl_replace_fw {
	mpt_ioctl_header hdr;
	int		 newImageSize;
	u8		 newImage[1];
} mpt_ioctl_replace_fw_t;

/* General MPT Pass through data strucutre
 *
 * iocnum
 * timeout - in seconds, command timeout. If 0, set by driver to
 *		default value.
 * replyFrameBufPtr - reply location
 * dataInBufPtr - destination for read
 * dataOutBufPtr - data source for write
 * senseDataPtr - sense data location
 * maxReplyBytes - maximum number of reply bytes to be sent to app.
 * dataInSize - num bytes for data transfer in (read)
 * dataOutSize - num bytes for data transfer out (write)
 * dataSgeOffset - offset in words from the start of the request message
 *		to the first SGL
 * MF[1];
 *
 * Remark:  Some config pages have bi-directional transfer,
 * both a read and a write. The basic structure allows for
 * a bidirectional set up. Normal messages will have one or
 * both of these buffers NULL.
 */
struct mpt_ioctl_command {
	mpt_ioctl_header hdr;
	int		timeout;	/* optional (seconds) */
	char		__user *replyFrameBufPtr;
	char		__user *dataInBufPtr;
	char		__user *dataOutBufPtr;
	char		__user *senseDataPtr;
	int		maxReplyBytes;
	int		dataInSize;
	int		dataOutSize;
	int		maxSenseBytes;
	int		dataSgeOffset;
	char		MF[1];
};

/*
 * SPARC PLATFORM: See earlier remark.
 */
#if defined(__KERNEL__) && defined(CONFIG_COMPAT)
struct mpt_ioctl_command32 {
	mpt_ioctl_header hdr;
	int	timeout;
	u32	replyFrameBufPtr;
	u32	dataInBufPtr;
	u32	dataOutBufPtr;
	u32	senseDataPtr;
	int	maxReplyBytes;
	int	dataInSize;
	int	dataOutSize;
	int	maxSenseBytes;
	int	dataSgeOffset;
	char	MF[1];
};
#endif	/*}*/


/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *	IOCTL Defines and Structures
 */

#define CPQFCTS_IOC_MAGIC 'Z'
#define HP_IOC_MAGIC 'Z'
#define HP_GETHOSTINFO		_IOR(HP_IOC_MAGIC, 20, hp_host_info_t)
#define HP_GETHOSTINFO1		_IOR(HP_IOC_MAGIC, 20, hp_host_info_rev0_t)
#define HP_GETTARGETINFO	_IOR(HP_IOC_MAGIC, 21, hp_target_info_t)

typedef struct _hp_header {
	unsigned int iocnum;
	unsigned int host;
	unsigned int channel;
	unsigned int id;
	unsigned int lun;
} hp_header_t;

/*
 *  Header:
 *  iocnum 	required (input)
 *  host 	ignored
 *  channe	ignored
 *  id		ignored
 *  lun		ignored
 */
typedef struct _hp_host_info {
	hp_header_t	 hdr;
	u16		 vendor;
	u16		 device;
	u16		 subsystem_vendor;
	u16		 subsystem_id;
	u8		 devfn;
	u8		 bus;
	ushort		 host_no;		/* SCSI Host number, if scsi driver not loaded*/
	u8		 fw_version[16];	/* string */
	u8		 serial_number[24];	/* string */
	u32		 ioc_status;
	u32		 bus_phys_width;
	u32		 base_io_addr;
	u32		 rsvd;
	unsigned int	 hard_resets;		/* driver initiated resets */
	unsigned int	 soft_resets;		/* ioc, external resets */
	unsigned int	 timeouts;		/* num timeouts */
} hp_host_info_t;

/* replace ulongs with uints, need to preserve backwards
 * compatibility.
 */
typedef struct _hp_host_info_rev0 {
	hp_header_t	 hdr;
	u16		 vendor;
	u16		 device;
	u16		 subsystem_vendor;
	u16		 subsystem_id;
	u8		 devfn;
	u8		 bus;
	ushort		 host_no;		/* SCSI Host number, if scsi driver not loaded*/
	u8		 fw_version[16];	/* string */
	u8		 serial_number[24];	/* string */
	u32		 ioc_status;
	u32		 bus_phys_width;
	u32		 base_io_addr;
	u32		 rsvd;
	unsigned long	 hard_resets;		/* driver initiated resets */
	unsigned long	 soft_resets;		/* ioc, external resets */
	unsigned long	 timeouts;		/* num timeouts */
} hp_host_info_rev0_t;

/*
 *  Header:
 *  iocnum 	required (input)
 *  host 	required
 *  channel	required	(bus number)
 *  id		required
 *  lun		ignored
 *
 *  All error values between 0 and 0xFFFF in size.
 */
typedef struct _hp_target_info {
	hp_header_t	 hdr;
	u32 parity_errors;
	u32 phase_errors;
	u32 select_timeouts;
	u32 message_rejects;
	u32 negotiated_speed;
	u8  negotiated_width;
	u8  rsvd[7];				/* 8 byte alignment */
} hp_target_info_t;

#define HP_STATUS_OTHER		1
#define HP_STATUS_OK		2
#define HP_STATUS_FAILED	3

#define HP_BUS_WIDTH_UNK	1
#define HP_BUS_WIDTH_8		2
#define HP_BUS_WIDTH_16		3
#define HP_BUS_WIDTH_32		4

#define HP_DEV_SPEED_ASYNC	2
#define HP_DEV_SPEED_FAST	3
#define HP_DEV_SPEED_ULTRA	4
#define HP_DEV_SPEED_ULTRA2	5
#define HP_DEV_SPEED_ULTRA160	6
#define HP_DEV_SPEED_SCSI1	7
#define HP_DEV_SPEED_ULTRA320	8

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#define MPI_FW_DIAG_IOCTL               (0x80646961)    // dia
#define MPI_FW_DIAG_TYPE_REGISTER       (0x00000001)
#define MPI_FW_DIAG_TYPE_UNREGISTER     (0x00000002)
#define MPI_FW_DIAG_TYPE_QUERY          (0x00000003)
#define MPI_FW_DIAG_TYPE_READ_BUFFER    (0x00000004)
#define MPI_FW_DIAG_TYPE_RELEASE        (0x00000005)

#define MPI_FW_DIAG_INVALID_UID         (0x00000000)
#define FW_DIAGNOSTIC_BUFFER_COUNT      (3)
#define FW_DIAGNOSTIC_UID_NOT_FOUND     (0xFF)

#define MPI_FW_DIAG_ERROR_SUCCESS           (0x00000000)
#define MPI_FW_DIAG_ERROR_FAILURE           (0x00000001)
#define MPI_FW_DIAG_ERROR_INVALID_PARAMETER (0x00000002)
#define MPI_FW_DIAG_ERROR_POST_FAILED       (0x00000010)
#define MPI_FW_DIAG_ERROR_INVALID_UID       (0x00000011)
#define MPI_FW_DIAG_ERROR_RELEASE_FAILED    (0x00000012)
#define MPI_FW_DIAG_ERROR_NO_BUFFER         (0x00000013)
#define MPI_FW_DIAG_ERROR_ALREADY_RELEASED  (0x00000014)

#define MPT_DIAG_CAPABILITY(bufftype) (MPI_IOCFACTS_CAPABILITY_DIAG_TRACE_BUFFER << bufftype)

typedef struct _MPI_FW_DIAG_REGISTER
{
    U8                  TraceLevel;
    U8                  BufferType;
    U16                 Flags;
    U32                 ExtendedType;
    U32                 ProductSpecific[4];
    U32                 RequestedBufferSize;
    U32                 UniqueId;
} MPI_FW_DIAG_REGISTER, *PTR_MPI_FW_DIAG_REGISTER;

typedef struct _mpt_diag_register {
	mpt_ioctl_header hdr;
	MPI_FW_DIAG_REGISTER data;
} mpt_diag_register_t;

typedef struct _MPI_FW_DIAG_UNREGISTER
{
    U32                 UniqueId;
} MPI_FW_DIAG_UNREGISTER, *PTR_MPI_FW_DIAG_UNREGISTER;

typedef struct _mpt_diag_unregister {
	mpt_ioctl_header hdr;
	MPI_FW_DIAG_UNREGISTER data;
} mpt_diag_unregister_t;

#define MPI_FW_DIAG_FLAG_APP_OWNED          (0x0001)
#define MPI_FW_DIAG_FLAG_BUFFER_VALID       (0x0002)
#define MPI_FW_DIAG_FLAG_FW_BUFFER_ACCESS   (0x0004)

typedef struct _MPI_FW_DIAG_QUERY
{
    U8                  TraceLevel;
    U8                  BufferType;
    U16                 Flags;
    U32                 ExtendedType;
    U32                 ProductSpecific[4];
    U32                 DataSize;   
    U32                 DriverAddedBufferSize;
    U32                 UniqueId;
} MPI_FW_DIAG_QUERY, *PTR_MPI_FW_DIAG_QUERY;

typedef struct _mpt_diag_query {
	mpt_ioctl_header hdr;
	MPI_FW_DIAG_QUERY data;
} mpt_diag_query_t;

typedef struct _MPI_FW_DIAG_RELEASE
{
    U32                 UniqueId;
} MPI_FW_DIAG_RELEASE, *PTR_MPI_FW_DIAG_RELEASE;

typedef struct _mpt_diag_release {
	mpt_ioctl_header hdr;
	MPI_FW_DIAG_RELEASE data;
} mpt_diag_release_t;

#define MPI_FW_DIAG_FLAG_REREGISTER         (0x0001)

typedef struct _MPI_FW_DIAG_READ_BUFFER
{
    U8                  Status;
    U8                  Reserved;
    U16                 Flags;
    U32                 StartingOffset;
    U32                 BytesToRead;
    U32                 UniqueId;
    U32                 DiagnosticData[1];
} MPI_FW_DIAG_READ_BUFFER, *PTR_MPI_FW_DIAG_READ_BUFFER;

typedef struct _mpt_diag_read_buffer {
	mpt_ioctl_header hdr;
	MPI_FW_DIAG_READ_BUFFER data;
} mpt_diag_read_buffer_t;

#endif

