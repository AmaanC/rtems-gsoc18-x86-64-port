/*  shm.h
 *
 *  This include file contains all the constants, structures,
 *  and global variables for this RTEMS based shared memory
 *  communications interface driver.
 *
 *  Processor board dependencies are in other files.
 *
 *  COPYRIGHT (c) 1989, 1990, 1991, 1992, 1993, 1994.
 *  On-Line Applications Research Corporation (OAR).
 *  All rights assigned to U.S. Government, 1994.
 *
 *  This material may be reproduced by or for the U.S. Government pursuant
 *  to the copyright license under the clause at DFARS 252.227-7013.  This
 *  notice must appear in all copies of this file and its derivatives.
 *
 *  shm.h,v 1.2 1995/05/09 20:23:03 joel Exp
 */

#ifndef __SHM_h
#define __SHM_h

#ifdef __cplusplus
extern "C" {
#endif

/*  The information contained in the Node Status, Locked Queue, and
 *  Envelope Control Blocks must be maintained in a NEUTRAL format.
 *  Currently the neutral format may be selected as big or little
 *  endian by simply defining either NEUTRAL_BIG or NEUTRAL_LITTLE.
 *
 *  It is CRITICAL to note that the neutral format can ONLY be
 *  changed by modifying this file and recompiling the ENTIRE
 *  SHM driver including ALL target specific support files.
 *
 *  The following table details the memory contents for the endian
 *  field of the Node Status Control Block in the various
 *  data format configurations (data is in hexadecimal):
 *
 *   NEUTRAL NATIVE BYTE 0  BYTE 1  BYTE 2  BYTE 3
 *   ======= ====== ======  ======  ======  ======
 *    BIG     BIG     00      00      00      01
 *    BIG    LITTLE   10      00      00      00
 *   LITTLE   BIG     01      00      00      00
 *   LITTLE  LITTLE   00      00      00      10
 *
 *
 *  NOTE: XXX
 *                PORTABILITY OF LOCKING INSTRUCTIONS
 *                ===================================
 *            The locking mechanism described below is not
 *            general enough.  Where the hardware supports
 *            it we should use "atomic swap" instructions
 *            so the values in the lock can be tailored to
 *            support a CPU with only weak atomic memory
 *            instructions.  There are combinations of
 *            CPUs with inflexible atomic memory instructions
 *            which appear to be incompatible.  For example,
 *            the SPARClite instruction uses a byte which is
 *            0xFF when locked.  The PA-RISC uses 1 to indicate
 *            locked and 0 when unlocked.  These CPUs appear to
 *            have incompatible lock instructions.  But
 *            they could be used in a heterogenous system
 *            with does not mix SPARCs and PA-RISCs.  For
 *            example, the i386 and SPARC or i386 and SPARC
 *            could work together.  The bottom line is that
 *            not every CPU will work together using this
 *            locking scheme.  There are supposed to be
 *            algorithms to do this without hardware assist
 *            and one of these should be incorporated into
 *            the shared memory driver.
 *
 *            The most flexible scheme using the instructions
 *            of the various CPUs for efficiency would be to use
 *            "atomic swaps" wherever possible.  Make the lock
 *            and unlock configurable much like BIG vs LITTLE
 *            endian use of shared memory is now.  The values
 *            of the lock could then reflect the "worst"
 *            CPU in a system.  This still results in mixes
 *            of CPUs which are incompatible.
 *
 *  The current locking mechanism is based upon the MC68020
 *  "tas" instruction which is atomic.  All ports to other CPUs
 *  comply with the restrictive placement of lock bit by this
 *  instruction.  The lock bit is the most significant bit in a
 *  big-endian rtems_unsigned32.  On other processors, the lock is
 *  typically implemented via an atomic swap or atomic modify
 *  bits type instruction.
 */

#define NEUTRAL_BIG

#ifdef NEUTRAL_BIG
#define SHM_BIG       0x00000001
#define SHM_LITTLE    0x10000000
#endif

#ifdef NEUTRAL_LITTLE
#define SHM_BIG       0x01000000
#define SHM_LITTLE    0x00000010
#endif

/*
 *  The following are the values used to fill in the lock field.  Some CPUs
 *  are able to write only a single value into field.  By making the
 *  lock and unlock values configurable, CPUs which support "atomic swap"
 *  instructions can generally be made to work in any heterogeneous
 *  configuration.  However, it is possible for two CPUs to be incompatible
 *  in regards to the lock field values.  This occurs when two CPUs
 *  which write only a single value to the field are used in a system
 *  but the two CPUs write different incompatible values.
 *
 *  NOTE:  The following is a first attempt at defining values which
 *         have a chance at working together.  The m68k should use
 *         chk2 instead of tas to be less restrictive.  Target endian
 *         problems (like the Force CPU386 which has (broken) big endian
 *         view of the VMEbus address space) are not addressed yet.
 */

#if defined(i960)
#define SHM_LOCK_VALUE    0x00000080
#define SHM_UNLOCK_VALUE  0
#elif defined(m68k)
#define SHM_LOCK_VALUE    0x80000000
#define SHM_UNLOCK_VALUE  0
#define SHM_LOCK_VALUE    0x80000000
#define SHM_UNLOCK_VALUE  0
#elif defined(i386)
#define SHM_LOCK_VALUE    0x80000000
#define SHM_UNLOCK_VALUE  0
#elif defined(hppa1_1)
#define SHM_LOCK_VALUE    0
#define SHM_UNLOCK_VALUE  1
#elif defined(unix)
#define SHM_LOCK_VALUE    0
#define SHM_UNLOCK_VALUE  1
#elif defined(no_cpu)               /* for this values are irrelevant */
#define SHM_LOCK_VALUE    1
#define SHM_UNLOCK_VALUE  0
#endif

#define Shm_Convert( value ) \
  ((Shm_Configuration->convert) ? \
    (*Shm_Configuration->convert)(value) : (value))

/* constants */

#define SHM_MASTER                  1     /* master initialization node */
#define SHM_FIRST_NODE              1

/* size constants */

#define KILOBYTE          (1024)
#define MEGABYTE          (1024*1024)

/* inter-node interrupt values */

#define NO_INTERRUPT            0     /* used for polled nodes */
#define BYTE                    1
#define WORD                    2
#define LONG                    4

/* operational mode constants -- used in SHM Configuration Table */
#define POLLED_MODE             0
#define INTR_MODE               1

/* error codes */

#define NO_ERROR                0
#define SHM_NO_FREE_PKTS        0xf0000

/* null pointers of different types */

#define NULL_ENV_CB             ((Shm_Envelope_control *) 0)
#define NULL_SHM_INFO           ((struct shm_info *) 0)
#define NULL_CONVERT            0
#if 0
#define NULL_CONVERT            (((rtems_unsigned32 *)())0) /* we want this */
#endif

/* The following is adjusted so envelopes are 0x80 bytes long. */
/* It should be >= MIN_PKT_SIZE in rtems.h                     */

#define MAX_PACKET_SIZE          (80)

/* constants pertinent to Locked Queue routines */

#define LQ_UNLOCKED              SHM_UNLOCK_VALUE
#define LQ_LOCKED                SHM_LOCK_VALUE

/* constants related to the Free Envelope Pool */

#define FREE_ENV_POOL            0
#define FREE_ENV_CB              (&Shm_Locked_queues[ FREE_ENV_POOL ])

/*  The following are important when dealing with
 *  the shared memory communications interface area.
 *
 *  NOTE: The starting address and length of the shared memory
 *        is defined in a system dependent file.
 */

#if 0
#define START_NS_CBS     ( (rtems_unsigned8 *) START_SHARED_MEM )
#define START_LQ_CBS     ( ((rtems_unsigned8 *) START_NS_CBS) + \
        ( (sizeof (Shm_Node_status_control)) * (Shm_Maximum_nodes + 1) ) )
#define START_ENVELOPES  ( ((rtems_unsigned8 *) START_LQ_CBS) + \
        ( (sizeof (Shm_Locked_queue_Control)) * (Shm_Maximum_nodes + 1) ) )
#define END_SHMCI_AREA    ( (rtems_unsigned8 *) START_ENVELOPES + \
        ( (sizeof (Shm_Envelope_control)) * Shm_Maximum_envelopes ) )
#define END_SHARED_MEM   ((rtems_unsigned32)START_SHARED_MEM+SHARED_MEM_LEN)
#endif

#define START_NS_CBS     ((void *)Shm_Configuration->base)
#define START_LQ_CBS     ((START_NS_CBS) + \
        ( (sizeof (Shm_Node_status_control)) * (Shm_Maximum_nodes + 1) ) )
#define START_ENVELOPES  ( ((void *) START_LQ_CBS) + \
        ( (sizeof (Shm_Locked_queue_Control)) * (Shm_Maximum_nodes + 1) ) )
#define END_SHMCI_AREA    ( (void *) START_ENVELOPES + \
        ( (sizeof (Shm_Envelope_control)) * Shm_Maximum_envelopes ) )
#define END_SHARED_MEM   (START_NS_CBS+Shm_Configuration->length)

/* macros */

#define Shm_Is_master_node() \
  ( SHM_MASTER == Shm_Local_node )

#define Shm_Free_envelope( ecb ) \
  Shm_Locked_queue_Add( FREE_ENV_CB, (ecb) )
#define Shm_Allocate_envelope() \
  Shm_Locked_queue_Get(FREE_ENV_CB)

#define Shm_Initialize_receive_queue(node) \
  Shm_Locked_queue_Initialize( &Shm_Locked_queues[node], node )

#define Shm_Append_to_receive_queue(node, ecb) \
  Shm_Locked_queue_Add( &Shm_Locked_queues[node], (ecb) )

#define Shm_Envelope_control_to_packet_prefix_pointer(ecb)  \
   ((void *)(ecb)->packet)

#define Shm_Packet_prefix_to_envelope_control_pointer( pkt )   \
   ((Shm_Envelope_control *)((rtems_unsigned8 *)(pkt) - \
   (sizeof(Shm_Envelope_preamble) + 4*sizeof(vol_u32))))

#define Shm_Build_preamble(ecb, node) \
       (ecb)->Preamble.endian = Shm_Configuration->format

#define Shm_Build_postamble( ecb )

/* structures */

typedef volatile rtems_unsigned8  vol_u8;
typedef volatile rtems_unsigned32 vol_u32;

/* shm control information */

struct shm_info {
  vol_u32 not_currently_used_0;
  vol_u32 not_currently_used_1;
  vol_u32 not_currently_used_2;
  vol_u32 not_currently_used_3;
};

typedef struct {
  /*byte start_of_text;*/
  vol_u32 endian;
  vol_u32 not_currently_used_0;
  vol_u32 not_currently_used_1;
  vol_u32 not_currently_used_2;
} Shm_Envelope_preamble;

typedef struct {
  vol_u32 not_currently_used_0;
  vol_u32 not_currently_used_1;
  vol_u32 not_currently_used_2;
  vol_u32 not_currently_used_3;
  /*byte end_of_text;*/
} Shm_Envelope_postable;

/* WARNING! If you change this structure, don't forget to change
 *          Shm_Envelope_control_to_packet_prefix_pointer() and
 *          Shm_Packet_prefix_to_envelope_control_pointer() above.
 */

/*  This comment block describes the contents of each field
 *  of the Envelope Control Block:
 *
 *  next      - The index of the next envelope on this queue.
 *  queue     - The index of the queue this envelope is on.
 *  index     - The index of this envelope.
 *  Preamble  - Generic packet preamble.  One day this structure
 *              could be enhanced to contain routing information.
 *  packet    - RTEMS MPCI packet.  Untouched by SHM Driver
 *              other than copying and format conversion as
 *              documented in the RTEMS User's Guide.
 *  Postamble - Generic packet postamble.  One day this structure
 *              could be enhanced to contain checksum information.
 */

typedef struct {
  vol_u32           next;     /* next envelope on queue       */
  vol_u32           queue;    /* queue on which this resides  */
  vol_u32           index;    /* index into array of envelopes*/
  vol_u32           pad0;     /* insure the next one is aligned */
  Shm_Envelope_preamble    Preamble; /* header information           */
  vol_u8            packet[MAX_PACKET_SIZE]; /* RTEMS INFO    */
  Shm_Envelope_postable   Postamble;/* trailer information          */
} Shm_Envelope_control;

/*  This comment block describes the contents of each field
 *  of the Locked Queue Control Block:
 *
 *  lock      - Lock used to insure mutually exclusive access.
 *  front     - Index of first envelope on queue.  This field
 *              is used to remove head of queue (receive).
 *  rear      - Index of last envelope on queue.  This field
 *              is used to add evelope to queue (send).
 *  owner     - The node number of the recipient (owning) node.
 *              RTEMS does not use the node number zero (0).
 *              The zero node is used by the SHM Driver for the
 *              Free Envelope Queue shared by all nodes.
 */

typedef struct {
  vol_u32 lock;  /* lock field for this queue    */
  vol_u32 front; /* first envelope on queue      */
  vol_u32 rear;  /* last envelope on queue       */
  vol_u32 owner; /* receiving (i.e. owning) node */
} Shm_Locked_queue_Control;

/*  This comment block describes the contents of each field
 *  of the Node Status Control Block:
 *
 *  status    - Node status.  Current values are Pending Initialization,
 *              Initialization Complete, and Active Node.  Other values
 *              could be added to enhance fault tolerance.
 *  error     - Zero if the node has not failed.  Otherwise,
 *              this field contains a status indicating the
 *              failure reason.
 *  int_address, int_value, and int_length
 *            - These field are the Interrupt Information table
 *              for this node in neutral format.  This is how
 *              each node knows how to generate interrupts.
 */

typedef struct {
  vol_u32  status;         /* node status information     */
  vol_u32  error;          /* fatal error code            */
  vol_u32  int_address;    /* write here for interrupt    */
  vol_u32  int_value;      /* this value causes interrupt */
  vol_u32  int_length;     /* for this length (0,1,2,4)   */
  vol_u32  not_currently_used_0;
  vol_u32  not_currently_used_1;
  vol_u32  not_currently_used_2;
} Shm_Node_status_control;

/*  This comment block describes the contents of each field
 *  of the Interrupt Information Table.  This table describes
 *  how another node can generate an interrupt to this node.
 *  This information is target board dependent.  If the
 *  SHM Driver is in POLLED_MODE, then all fields should
 *  be initialized to NO_INTERRUPT.
 *
 *  address   - The address to which another node should
 *              write to cause an interrupt.
 *  value     - The value which must be written
 *  length    - The size of the value to write.  Valid
 *              values are BYTE, WORD, and LONG.
 *
 *  NOTE:  The Node Status Control Block contains this
 *         information in neutral format and not in a
 *         structure to avoid potential alignment problems.
 */

typedef struct {
  vol_u32 *address;        /* write here for interrupt    */
  vol_u32  value;          /* this value causes interrupt */
  vol_u32  length;         /* for this length (0,1,2,4)   */
} Shm_Interrupt_information;

/*  SHM Configuration Table
 *
 *  This comment block describes the contents of each field
 *  of the SHM Configuration Table.
 *
 *  base       - The base address of the shared memory.  This
 *               address may be specific to this node.
 *  length     - The length of the shared memory in bytes.
 *  format     - The natural format for rtems_unsigned32's in the
 *               shared memory.  Valid values are currently
 *               only SHM_LITTLE and SHM_BIG.
 *  convert    - The address of the routine which converts
 *               between neutral and local format.
 *  poll_intr  - The operational mode of the driver.  Some
 *               target boards may not provide hardware for
 *               an interprocessor interrupt.  If POLLED_MODE
 *               is selected, the SHM driver will install a
 *               wrapper around the Clock_isr() to poll for
 *               incoming packets.  Throughput is dependent
 *               on the time between clock interrupts.
 *               Valid values are POLLED_MODE and INTR_MODE.
 *  cause_intr - This is the address of the routine used to
 *               write to a particular address and cause an
 *               interrupt on another node.  This routine
 *               may need to be target dependent if something
 *               other than a normal write from C does not work.
 *  Intr       - This structure describes the operation required
 *               to cause an interrupt to this node.  The actual
 *               contents of this structure are described above.
 */

struct shm_config_info {
  vol_u32           *base;     /* base address of SHM         */
  vol_u32            length;   /* length (in bytes) of SHM    */
  vol_u32            format;   /* SHM is big or little endian */
  vol_u32          (*convert)();/* neutral conversion routine */
  vol_u32            poll_intr;/* POLLED or INTR driven mode  */
  void             (*cause_intr)( rtems_unsigned32 );
  Shm_Interrupt_information   Intr;     /* cause intr information      */
};

typedef struct shm_config_info shm_config_table;

/* global variables */

#ifdef _SHM_INIT
#define SHM_EXTERN
#else
#define SHM_EXTERN extern
#endif

SHM_EXTERN shm_config_table             *Shm_Configuration;
SHM_EXTERN Shm_Interrupt_information     Shm_Interrupt_table[16];
SHM_EXTERN Shm_Node_status_control      *Shm_Node_statuses;
SHM_EXTERN Shm_Locked_queue_Control     *Shm_Locked_queues;
SHM_EXTERN Shm_Envelope_control         *Shm_Envelopes;
SHM_EXTERN rtems_configuration_table    *Shm_RTEMS_Configuration;
SHM_EXTERN rtems_multiprocessing_table  *Shm_RTEMS_MP_Configuration;
SHM_EXTERN rtems_unsigned32              Shm_Receive_message_count;
SHM_EXTERN rtems_unsigned32              Shm_Null_message_count;
SHM_EXTERN rtems_unsigned32              Shm_Interrupt_count;
SHM_EXTERN rtems_unsigned32              Shm_Local_node;
SHM_EXTERN Shm_Locked_queue_Control      *Shm_Local_receive_queue;
SHM_EXTERN Shm_Node_status_control       *Shm_Local_node_status;
SHM_EXTERN rtems_unsigned32              Shm_isrstat;
                                                     /* reported by shmdr */

SHM_EXTERN rtems_unsigned32 Shm_Pending_initialization;
SHM_EXTERN rtems_unsigned32 Shm_Initialization_complete;
SHM_EXTERN rtems_unsigned32 Shm_Active_node;

SHM_EXTERN rtems_unsigned32 Shm_Maximum_nodes;
SHM_EXTERN rtems_unsigned32 Shm_Maximum_envelopes;

SHM_EXTERN rtems_unsigned32 Shm_Locked_queue_End_of_list;
SHM_EXTERN rtems_unsigned32 Shm_Locked_queue_Not_on_list;

/* functions */

/* locked queue routines */
void           Shm_Locked_queue_Add(
                  Shm_Locked_queue_Control *, Shm_Envelope_control * );
Shm_Envelope_control *Shm_Locked_queue_Get( Shm_Locked_queue_Control * );
void           Shm_Locked_queue_Initialize(
                  Shm_Locked_queue_Control *, rtems_unsigned32 );
            /* Shm_Initialize_lock is CPU dependent */
            /* Shm_Lock is CPU dependent */
            /* Shm_Unlock is CPU dependent */

/* portable routines */
void           Init_env_pool();
void           Shm_Print_statistics( void );
void           MPCI_Fatal( rtems_unsigned32 );
rtems_task     Shm_Cause_interrupt( rtems_unsigned32 );
void           Shm_Poll();
void           Shm_setclockvec();
void           Shm_Convert_packet( rtems_packet_prefix * );

/* CPU specific routines are inlined in shmcpu.h */

/* target specific routines */
void          *Shm_Convert_address( void * );
void           Shm_Get_configuration( rtems_unsigned32, shm_config_table ** );
void           Shm_isr();
void           Shm_setvec( void );

void           Shm_Initialize_lock( Shm_Locked_queue_Control * );
void           Shm_Lock( Shm_Locked_queue_Control * );
void           Shm_Unlock( Shm_Locked_queue_Control * );

/* MPCI entry points */
rtems_mpci_entry Shm_Get_packet(
  rtems_packet_prefix **
);

rtems_mpci_entry Shm_Initialization(
  rtems_configuration_table   *configuration,
  rtems_cpu_table             *cpu_configuration,
  rtems_multiprocessing_table *mp_configuration
);

rtems_mpci_entry Shm_Receive_packet(
  rtems_packet_prefix **
);

rtems_mpci_entry Shm_Return_packet(
  rtems_packet_prefix *
);

rtems_mpci_entry Shm_Send_packet(
  rtems_unsigned32,
  rtems_packet_prefix *
);

#ifdef _SHM_INIT

/* multiprocessor communications interface (MPCI) table */

rtems_mpci_table MPCI_table  = {
  100000,                     /* default timeout value in ticks */
  Shm_Initialization,         /* initialization procedure   */
  Shm_Get_packet,             /* get packet procedure       */
  Shm_Return_packet,          /* return packet procedure    */
  Shm_Send_packet,            /* packet send procedure      */
  Shm_Receive_packet          /* packet receive procedure   */
};

#else

extern rtems_mpci_table MPCI_table;

#endif

#ifdef __cplusplus
}
#endif

#endif
/* end of include file */
