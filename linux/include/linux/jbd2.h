/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * linux/include/linux/jbd2.h
 *
 * Written by Stephen C. Tweedie <sct@redhat.com>
 *
 * Copyright 1998-2000 Red Hat, Inc --- All Rights Reserved
 *
 * Definitions for transaction data structures for the buffer cache
 * filesystem journaling support.
 */

#ifndef _LINUX_JBD2_H
#define _LINUX_JBD2_H

/* Allow this file to be included directly into e2fsprogs */
#ifndef __KERNEL__
#include "jfs_compat.h"
#define JBD2_DEBUG
#else

#include <linux/types.h>
#include <linux/buffer_head.h>
#include <linux/journal-head.h>
#include <linux/stddef.h>
#include <linux/mutex.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/bit_spinlock.h>
#include <linux/blkdev.h>
#include <crypto/hash.h>
#endif

#define journal_oom_retry 1

/*
 * Define JBD2_PARANIOD_IOFAIL to cause a kernel BUG() if ext4 finds
 * certain classes of error which can occur due to failed IOs.  Under
 * normal use we want ext4 to continue after such errors, because
 * hardware _can_ fail, but for debugging purposes when running tests on
 * known-good hardware we may want to trap these errors.
 */
#undef JBD2_PARANOID_IOFAIL

/*
 * The default maximum commit age, in seconds.
 * JBD2 주기적 커밋의 주기(단위: 초)
 */
#define JBD2_DEFAULT_MAX_COMMIT_AGE 5

/* 디버깅 코드 시작 */
#ifdef CONFIG_JBD2_DEBUG
/*
 * Define JBD2_EXPENSIVE_CHECKING to enable more expensive internal
 * consistency checks.  By default we don't do this unless
 * CONFIG_JBD2_DEBUG is on.
 */
#define JBD2_EXPENSIVE_CHECKING
void __jbd2_debug(int level, const char *file, const char *func,
		  unsigned int line, const char *fmt, ...);

#define jbd2_debug(n, fmt, a...) \
	__jbd2_debug((n), __FILE__, __func__, __LINE__, (fmt), ##a)
#else
#define jbd2_debug(n, fmt, a...)  no_printk(fmt, ##a)
#endif
/* 디버깅 코드 끝*/

//TODO: 설명 추가
extern void *jbd2_alloc(size_t size, gfp_t flags);
extern void jbd2_free(void *ptr, size_t size);

/* 
 * 저널 영역 최소 크기: 1024 블럭
 * FC area 기본 크기: 256 블럭
 */
#define JBD2_MIN_JOURNAL_BLOCKS 1024
#define JBD2_DEFAULT_FAST_COMMIT_BLOCKS 256

#ifdef __KERNEL__

/**
 * typedef handle_t - The handle_t type represents a single atomic update being performed by some process.
 *
 * All filesystem modifications made by the process go
 * through this handle.  Recursive operations (such as quota operations)
 * are gathered into a single update.
 *
 * The buffer credits field is used to account for journaled buffers
 * being modified by the running process.  To ensure that there is
 * enough log space for all outstanding operations, we need to limit the
 * number of outstanding buffers possible at any time.  When the
 * operation completes, any buffer credits not used are credited back to
 * the transaction, so that at all times we know how many buffers the
 * outstanding updates on a transaction might possibly touch.
 *
 * This is an opaque datatype.
 * 프로세스가 파일 시스템의 메타데이터를 수정하기 위해서는 handle을 취득해야함.
 * 핸들 취득과 buffer credit이 연관된 것으로 보임.
 * buffer credit은 각 file operation이 저널 영역에 충분한 연속된 공간을 예약하는 것.
 **/
typedef struct jbd2_journal_handle handle_t;	/* Atomic operation type */


/**
 * typedef journal_t - The journal_t maintains all of the journaling state information for a single filesystem.
 *
 * journal_t is linked to from the fs superblock structure.
 *
 * We use the journal_t to keep track of all outstanding transaction
 * activity on the filesystem, and to manage the state of the log
 * writing process.
 *
 * This is an opaque datatype.
 * 저널 타입.
 * 저널 개체는 파일 시스템의 수퍼블럭으로부터 접근 가능
 * 저널을 통해 처리 중인 모든 트랜잭션의 상태를 트래킹하고 관리할 수 있음.
 **/
typedef struct journal_s	journal_t;	/* Journal control structure */
#endif

/*
 * Internal structures used by the logging mechanism:
 * 공통으로 사용되는 매직 넘버
 */

#define JBD2_MAGIC_NUMBER 0xc03b3998U /* The first 4 bytes of /dev/random! */

/*
 * On-disk structures
 */

/*
 * Descriptor block types:
 * 저널 영역에 적히는 블럭 중 메타데이터가 아닌 특수 블럭들을 descriptor block이라고 함.
 * 이 블럭들의 종류를 나타내기 위한 매크로 변수들
 */

#define JBD2_DESCRIPTOR_BLOCK	1
#define JBD2_COMMIT_BLOCK	2
#define JBD2_SUPERBLOCK_V1	3
#define JBD2_SUPERBLOCK_V2	4
#define JBD2_REVOKE_BLOCK	5

/*
 * Standard header for all descriptor blocks:
 * 디스크립터 블럭 정보를 담는 헤더
 * 매직넘버, 디스크립터 블럭의 종류(디스크립터, 커밋, 수퍼블럭), 시퀀스 넘버로 구성
 * TODO: 시퀀스 넘버 설명 추가
 */
typedef struct journal_header_s
{
	__be32		h_magic;
	__be32		h_blocktype;
	__be32		h_sequence;
} journal_header_t;

/* 체크섬은 저널링 동작보다는 error correction 기능이므로 우선 무시한다 */
/*
 * Checksum types.
 */
#define JBD2_CRC32_CHKSUM   1
#define JBD2_MD5_CHKSUM     2
#define JBD2_SHA1_CHKSUM    3
#define JBD2_CRC32C_CHKSUM  4

#define JBD2_CRC32_CHKSUM_SIZE 4

#define JBD2_CHECKSUM_BYTES (32 / sizeof(u32))
/*
 * Commit block header for storing transactional checksums:
 *
 * NOTE: If FEATURE_COMPAT_CHECKSUM (checksum v1) is set, the h_chksum*
 * fields are used to store a checksum of the descriptor and data blocks.
 *
 * If FEATURE_INCOMPAT_CSUM_V2 (checksum v2) is set, then the h_chksum
 * field is used to store crc32c(uuid+commit_block).  Each journal metadata
 * block gets its own checksum, and data block checksums are stored in
 * journal_block_tag (in the descriptor).  The other h_chksum* fields are
 * not used.
 *
 * If FEATURE_INCOMPAT_CSUM_V3 is set, the descriptor block uses
 * journal_block_tag3_t to store a full 32-bit checksum.  Everything else
 * is the same as v2.
 *
 * Checksum v1, v2, and v3 are mutually exclusive features.
 *
 * 커밋 블럭의 헤더
 * 커밋 블럭도 디스크립터 블럭으 한 종류이므로 처음 3개 필드는 journal_header_t와 동일
 * 그 이후에는 체크섬 정보, 체크섬 값, 커밋 시점 등의 정보를 저장.
 */
struct commit_header {
	__be32		h_magic;
	__be32          h_blocktype;
	__be32          h_sequence;
	unsigned char   h_chksum_type;
	unsigned char   h_chksum_size;
	unsigned char 	h_padding[2];
	__be32 		h_chksum[JBD2_CHECKSUM_BYTES];
	__be64		h_commit_sec;
	__be32		h_commit_nsec;
};

/*
 * The block tag: used to describe a single buffer in the journal.
 * t_blocknr_high is only used if INCOMPAT_64BIT is set, so this
 * raw struct shouldn't be used for pointer math or sizeof() - use
 * journal_tag_bytes(journal) instead to compute this.
 *
 * t_blocknr 블럭에 일어난 이벤트에 대한 정보를 t_flags로 표시하여 저장.
 * 아마 revoke record등을 관리하기 위해 사용하는 것 같다.
 * t_blocknr은 저널 영역이 아닌 메인 영역에서 해당 블록의 주소이다.
 */
typedef struct journal_block_tag3_s
{
	__be32		t_blocknr;	/* The on-disk block number */
	__be32		t_flags;	/* See below */
	__be32		t_blocknr_high; /* most-significant high 32bits. */
	__be32		t_checksum;	/* crc32c(uuid+seq+block) */
} journal_block_tag3_t;

typedef struct journal_block_tag_s
{
	__be32		t_blocknr;	/* The on-disk block number */
	__be16		t_checksum;	/* truncated crc32c(uuid+seq+block) */
	__be16		t_flags;	/* See below */
	__be32		t_blocknr_high; /* most-significant high 32bits. */
} journal_block_tag_t;

/* Tail of descriptor or revoke block, for checksumming 
 * 저널 디스크립터 블럭이나 revoke 블럭의 끝에 checksum을 저장하기 위한 구조체로 보임.
 */
struct jbd2_journal_block_tail {
	__be32		t_checksum;	/* crc32c(uuid+descr_block) */
};

/*
 * The revoke descriptor: used on disk to describe a series of blocks to
 * be revoked from the log
 * 
 * revoke 블럭의 헤더.
 * 몇 개의 블럭이 reuse되었는지 나타내기 위해 r_count 사용
 */
typedef struct jbd2_journal_revoke_header_s
{
	journal_header_t r_header;
	__be32		 r_count;	/* Count of bytes used in the block */
} jbd2_journal_revoke_header_t;

/* Definitions for the journal tag flags word: 
 * tag가 어떤 정보를 담는지 알려준다. t_falgs에 이 값이 들어가는 듯.
 */
#define JBD2_FLAG_ESCAPE		1	/* on-disk block is escaped */
#define JBD2_FLAG_SAME_UUID	2	/* block has same uuid as previous */
#define JBD2_FLAG_DELETED	4	/* block deleted by this transaction */
#define JBD2_FLAG_LAST_TAG	8	/* last tag in this descriptor block */


/*
 * The journal superblock.  All fields are in big-endian byte order.
 *
 * 저널 영역은 저널 수퍼블럭과 트랜잭션들이 저장되는 로그 영역으로 구성된다.
 * Layout 출처: https://www.kernel.org/doc/html/latest/filesystems/ext4/journal.html
 * 맨 앞에 s_header에서 superblock v1인지 v2인지 확인할 수 있다.
 * v2일 경우 0x0024 오프셋 이후에 있는 확장된 필드들을 제공하는 것으로 보임.
 * TODO: v1, v2 어디서 결정하는 지 설명 추가
 * 저널 영역의 크기, 현재 저장된 트랜잭션의 시작 주소, TID등을 저장한다.
 * FastCommit 블럭이 몇개 인지도 저장함.
 */
typedef struct journal_superblock_s
{
/* 0x0000 */
	journal_header_t s_header;

/* 0x000C */
	/* Static information describing the journal */
	__be32	s_blocksize;		/* journal device blocksize */
	__be32	s_maxlen;		/* total blocks in journal file */
	__be32	s_first;		/* first block of log information */

/* 0x0018 */
	/* Dynamic information describing the current state of the log */
	__be32	s_sequence;		/* first commit ID expected in log */
	__be32	s_start;		/* blocknr of start of log */

/* 0x0020 */
	/* Error value, as set by jbd2_journal_abort(). */
	__be32	s_errno;

/* 0x0024 */
	/* Remaining fields are only valid in a version-2 superblock */
	__be32	s_feature_compat;	/* compatible feature set */
	__be32	s_feature_incompat;	/* incompatible feature set */
	__be32	s_feature_ro_compat;	/* readonly-compatible feature set */
/* 0x0030 */
	__u8	s_uuid[16];		/* 128-bit uuid for journal */

/* 0x0040 */
	__be32	s_nr_users;		/* Nr of filesystems sharing log */

	__be32	s_dynsuper;		/* Blocknr of dynamic superblock copy*/

/* 0x0048 */
	__be32	s_max_transaction;	/* Limit of journal blocks per trans.*/
	__be32	s_max_trans_data;	/* Limit of data blocks per trans. */

/* 0x0050 */
	__u8	s_checksum_type;	/* checksum type */
	__u8	s_padding2[3];
/* 0x0054 */
	__be32	s_num_fc_blks;		/* Number of fast commit blocks */
	__be32	s_head;			/* blocknr of head of log, only uptodate
					 * while the filesystem is clean */
/* 0x005C */
	__u32	s_padding[40];
	__be32	s_checksum;		/* crc32c(superblock) */

/* 0x0100 */
	__u8	s_users[16*48];		/* ids of all fs'es sharing the log */
/* 0x0400 */
} journal_superblock_t;

/* 아래 매크로들과 비트 연산을 사용해서 현재 커널이 지원하는 저널링 피쳐들을 설정함 */
#define JBD2_FEATURE_COMPAT_CHECKSUM		0x00000001

#define JBD2_FEATURE_INCOMPAT_REVOKE		0x00000001
#define JBD2_FEATURE_INCOMPAT_64BIT		0x00000002
#define JBD2_FEATURE_INCOMPAT_ASYNC_COMMIT	0x00000004
#define JBD2_FEATURE_INCOMPAT_CSUM_V2		0x00000008
#define JBD2_FEATURE_INCOMPAT_CSUM_V3		0x00000010
#define JBD2_FEATURE_INCOMPAT_FAST_COMMIT	0x00000020

/* See "journal feature predicate functions" below */

/* Features known to this kernel version: */
#define JBD2_KNOWN_COMPAT_FEATURES	JBD2_FEATURE_COMPAT_CHECKSUM
#define JBD2_KNOWN_ROCOMPAT_FEATURES	0
#define JBD2_KNOWN_INCOMPAT_FEATURES	(JBD2_FEATURE_INCOMPAT_REVOKE | \
					JBD2_FEATURE_INCOMPAT_64BIT | \
					JBD2_FEATURE_INCOMPAT_ASYNC_COMMIT | \
					JBD2_FEATURE_INCOMPAT_CSUM_V2 | \
					JBD2_FEATURE_INCOMPAT_CSUM_V3 | \
					JBD2_FEATURE_INCOMPAT_FAST_COMMIT)

#ifdef __KERNEL__

#include <linux/fs.h>
#include <linux/sched.h>

/* 버퍼 헤드의 JBD 관려 상태 비트를 정의함 
 * TODO: 각 비트에 대한 설명 추가
 */
enum jbd_state_bits {
	BH_JBD			/* Has an attached ext3 journal_head */
	  = BH_PrivateStart,
	BH_JWrite,		/* Being written to log (@@@ DEBUGGING) */
	BH_Freed,		/* Has been freed (truncated) */
	BH_Revoked,		/* Has been revoked from the log */
	BH_RevokeValid,		/* Revoked flag is valid */
	BH_JBDDirty,		/* Is dirty but journaled */
	BH_JournalHead,		/* Pins bh->b_private and jh->b_bh */
	BH_Shadow,		/* IO on shadow buffer is running */
	BH_Verified,		/* Metadata block has been verified ok */
	BH_JBDPrivateStart,	/* First bit available for private use by FS */
};

/*
 * BUFFER_FNS(A, a) 매크로는 버퍼 헤드에 대한 비트 설정 함수 3개를 정의해주는 매크로.
 * buffer_head.h에 정의되어 있음.
 * set_buffer_a, clear_buffer_a, buffer_a라는 A 상태 비트에 대한 3가지 함수를 정의함. 
 * 
 * TAS_BUFFER_FNS(A, a)의 경우 test_set_buffer_a와 test_clear_buffer_a라는
 * Test And Swap 버전의 설정 함수들을 정의해줌.
 * buffer_head.h에 정의되어 있음.
 */
BUFFER_FNS(JBD, jbd)
BUFFER_FNS(JWrite, jwrite)
BUFFER_FNS(JBDDirty, jbddirty)
TAS_BUFFER_FNS(JBDDirty, jbddirty)
BUFFER_FNS(Revoked, revoked)
TAS_BUFFER_FNS(Revoked, revoked)
BUFFER_FNS(RevokeValid, revokevalid)
TAS_BUFFER_FNS(RevokeValid, revokevalid)
BUFFER_FNS(Freed, freed)
BUFFER_FNS(Shadow, shadow)
BUFFER_FNS(Verified, verified)

/* 저널 헤드와 버퍼 헤드 간 역참조에 사용되는 함수
 * TODO: b_private이 역할이 다양했던 것으로 기억하는데 다시 알아보기
 */

static inline struct buffer_head *jh2bh(struct journal_head *jh)
{
	return jh->b_bh;
}

static inline struct journal_head *bh2jh(struct buffer_head *bh)
{
	return bh->b_private;
}

/*
 * BH_JournalHead 비트값을 기준으로 lock acquire release를 하는 함수로 보임.
 * TODO: 이 함수의 사용처 추가
 */

static inline void jbd_lock_bh_journal_head(struct buffer_head *bh)
{
	bit_spin_lock(BH_JournalHead, &bh->b_state);
}

static inline void jbd_unlock_bh_journal_head(struct buffer_head *bh)
{
	bit_spin_unlock(BH_JournalHead, &bh->b_state);
}

/* Assertion과 저널링 실패 시 처리에 대한 함수.
 * 우선 생략
 * TODO: 저널링 핵심 동작에서 아래의 매크로 사용 시 설명 추가
 */
#define J_ASSERT(assert)	BUG_ON(!(assert))

#define J_ASSERT_BH(bh, expr)	J_ASSERT(expr)
#define J_ASSERT_JH(jh, expr)	J_ASSERT(expr)

#if defined(JBD2_PARANOID_IOFAIL)
#define J_EXPECT(expr, why...)		J_ASSERT(expr)
#define J_EXPECT_BH(bh, expr, why...)	J_ASSERT_BH(bh, expr)
#define J_EXPECT_JH(jh, expr, why...)	J_ASSERT_JH(jh, expr)
#else
#define __journal_expect(expr, why...)					     \
	({								     \
		int val = (expr);					     \
		if (!val) {						     \
			printk(KERN_ERR					     \
			       "JBD2 unexpected failure: %s: %s;\n",	     \
			       __func__, #expr);			     \
			printk(KERN_ERR why "\n");			     \
		}							     \
		val;							     \
	})
#define J_EXPECT(expr, why...)		__journal_expect(expr, ## why)
#define J_EXPECT_BH(bh, expr, why...)	__journal_expect(expr, ## why)
#define J_EXPECT_JH(jh, expr, why...)	__journal_expect(expr, ## why)
#endif

/* Flags in jbd_inode->i_flags 
 * jbd2_inode가 참조하는 파일(아이노드)가 어떤 상태인지 나타내는 비트 넘버 
 * 이거 쉬프트해서 플래그 만들면 아래 3개가 나온다.
 */
#define __JI_COMMIT_RUNNING 0
#define __JI_WRITE_DATA 1
#define __JI_WAIT_DATA 2

/*
 * Commit of the inode data in progress. We use this flag to protect us from
 * concurrent deletion of inode. We cannot use reference to inode for this
 * since we cannot afford doing last iput() on behalf of kjournald
 */
#define JI_COMMIT_RUNNING (1 << __JI_COMMIT_RUNNING)
/* Write allocated dirty buffers in this inode before commit */
#define JI_WRITE_DATA (1 << __JI_WRITE_DATA)
/* Wait for outstanding data writes for this inode before commit */
#define JI_WAIT_DATA (1 << __JI_WAIT_DATA)

/**
 * struct jbd2_inode - The jbd_inode type is the structure linking inodes in
 * ordered mode present in a transaction so that we can sync them during commit.
 * 역할: 파일 시스템 마다 inode 구조가 다르므로 이를 wrapping 하기 위한 구조체로 보임.
 * 저널 헤드는 각 버퍼(블럭)을 wrapping하는 개체고 이건 inode를 wrapping하는 것.
 * TODO: 역할 맞는 지 확인 
 */
struct jbd2_inode {
	/**
	 * @i_transaction:
	 *
	 * Which transaction does this inode belong to? Either the running
	 * transaction or the committing one. [j_list_lock]
	 * 아이노드가 속한 러닝 or 커밋 트랜잭션
	 * 
	 */
	transaction_t *i_transaction;

	/**
	 * @i_next_transaction:
	 *
	 * Pointer to the running transaction modifying inode's data in case
	 * there is already a committing transaction touching it. [j_list_lock]
	 * 트랜잭션 충돌 발생시 러닝 트랜잭션을 가리킨다.
	 */
	transaction_t *i_next_transaction;

	/**
	 * @i_list: List of inodes in the i_transaction [j_list_lock]
	 * 현재 속한 트랜잭션에서 inode를 이중 연결 리스트로 관리하는데,
	 * 이 리스트를 만들기 위한 포인터들
	 */
	struct list_head i_list;

	/**
	 * @i_vfs_inode:
	 *
	 * VFS inode this inode belongs to [constant for lifetime of structure]
	 * VFS레이어에서 대응되는 아이노드에 대한 포인터
	 */
	struct inode *i_vfs_inode;

	/**
	 * @i_flags: Flags of inode [j_list_lock]
	 * 바로 위에서 정의한 JI 플래그들로 현재 파일의 상태를 나타낸다.
	 */
	unsigned long i_flags;

	/**
	 * @i_dirty_start:
	 *
	 * Offset in bytes where the dirty range for this inode starts.
	 * [j_list_lock]
	 * TODO: 설명 추가
	 */
	loff_t i_dirty_start;

	/**
	 * @i_dirty_end:
	 *
	 * Inclusive offset in bytes where the dirty range for this inode
	 * ends. [j_list_lock]
	 * TODO: 설명 추가
	 */
	loff_t i_dirty_end;
};

struct jbd2_revoke_table_s;

/**
 * struct jbd2_journal_handle - The jbd2_journal_handle type is the concrete
 *     type associated with handle_t.
 * @h_transaction: Which compound transaction is this update a part of?
 * @h_journal: Which journal handle belongs to - used iff h_reserved set.
 * @h_rsv_handle: Handle reserved for finishing the logical operation.
 * @h_total_credits: Number of remaining buffers we are allowed to add to
 *	journal. These are dirty buffers and revoke descriptor blocks.
 * @h_revoke_credits: Number of remaining revoke records available for handle
 * @h_ref: Reference count on this handle.
 * @h_err: Field for caller's use to track errors through large fs operations.
 * @h_sync: Flag for sync-on-close.
 * @h_jdata: Flag to force data journaling.
 * @h_reserved: Flag for handle for reserved credits.
 * @h_aborted: Flag indicating fatal error on handle.
 * @h_type: For handle statistics.
 * @h_line_no: For handle statistics.
 * @h_start_jiffies: Handle Start time.
 * @h_requested_credits: Holds @h_total_credits after handle is started.
 * @h_revoke_credits_requested: Holds @h_revoke_credits after handle is started.
 * @saved_alloc_context: Saved context while transaction is open.
 **/

/* Docbook can't yet cope with the bit fields, but will leave the documentation
 * in so it can be fixed later.
 */

struct jbd2_journal_handle
{
	union {
		/* 핸들을 발급받고 수정사항을 삽입하려는 러닝 트랜잭션에 대한 포인터 */
		transaction_t	*h_transaction;
		/* Which journal handle belongs to - used iff h_reserved set */
		journal_t	*h_journal;
	};

	handle_t		*h_rsv_handle;          // TODO: 설명 추가
	int			h_total_credits;            // 로그 영역 예약한 블럭 몇 개 남았는지
	int			h_revoke_credits;           // 예약한 revoke record 몇 개 남았는지 
	int			h_revoke_credits_requested; // TODO: 설명 추가
	int			h_ref;											// 핸들의 reference counter
	int			h_err;											// 에러 발생시 에러 코드 확인용

	/* 
	 * Flags [no locking] 
	 * 핸들에 특수한 동작(sync-on-close: close마다 fsync인듯?)을 부여하는 플래그 같다.
	 */
	unsigned int	h_sync:		1;
	unsigned int	h_jdata:	1;
	unsigned int	h_reserved:	1;
	unsigned int	h_aborted:	1;
	unsigned int	h_type:		8;
	unsigned int	h_line_no:	16;

	unsigned long		h_start_jiffies;     // 핸들 발급된 시간
	unsigned int		h_requested_credits; // 최초로 예약할 로그 영역 블럭 수

	unsigned int		saved_alloc_context; 
};


/*
 * Some stats for checkpoint phase
 * 체크포인트 프로파일링용 구조체인듯
 */
struct transaction_chp_stats_s {
	unsigned long		cs_chp_time;
	__u32			cs_forced_to_close;
	__u32			cs_written;
	__u32			cs_dropped;
};

/* 트랜잭션은 상태 분류: jbd2_journal_commit_transaction()에 의해 아래 순서로 변화한다.
 * The transaction_t type is the guts of the journaling mechanism.  It
 * tracks a compound transaction through its various states:
 *
 * RUNNING:	accepting new updates
 * LOCKED:	Updates still running but we don't accept new ones
 * RUNDOWN:	Updates are tidying up but have finished requesting
 *		new buffers to modify (state not used for now)
 * FLUSH:       All updates complete, but we are still writing to disk
 * COMMIT:      All data on disk, writing commit record
 * FINISHED:	We still have to keep the transaction for checkpointing.
 *
 * The transaction keeps track of all of the buffers modified by a
 * running transaction, and all of the buffers committed but not yet
 * flushed to home for finished transactions.
 * (Locking Documentation improved by LockDoc)
 */

/*
 * TODO: 락 획득 순서 맞는지 확인
 * 아래 그래프에 없는 링크에 따라서 락을 획득하면 안 됨. 
 * +---------+              +---------+
 * | j_state | -----------> | j_list  |---------> jbd_lock_bh_journal_head() 
 * +---------+      \       +---------+
 *                   \          /\
 *                    \         ||
 *                     ---> +---------+
 *                          | b_state |
 *                          +---------+
 *
 * Lock ranking:
 *
 *    j_list_lock
 *      ->jbd_lock_bh_journal_head()	(This is "innermost")
 *
 *    j_state_lock
 *    ->b_state_lock
 *
 *    b_state_lock
 *    ->j_list_lock
 *
 *    j_state_lock
 *    ->j_list_lock			(journal_unmap_buffer)
 *
 */

/* 트랜잭션 구조체 */
struct transaction_s
{
	/* Pointer to the journal for this transaction. [no locking]
	 * 트랜잭션을 관리하는 저널에 대한 포인터 
	 */
	journal_t		*t_journal;

	/* Sequence number for this transaction [no locking] 
	 * 트랜잭션 id
	 */
	tid_t			t_tid;

	/*
	 * Transaction's current state
	 * [no locking - only kjournald2 alters this]
	 * [j_list_lock] guards transition of a transaction into T_FINISHED
	 * state and subsequent call of __jbd2_journal_drop_transaction()
	 * FIXME: needs barriers
	 * KLUDGE: [use j_state_lock]
	 *
	 * JBD2 데몬(kjournald2)만이 트랜잭션의 상태를 수정하므로 동기화 필요 x
	 * TODO: j_list_lock 관련 주석 설명 추가
	 */
	enum {
		T_RUNNING,
		T_LOCKED,
		T_SWITCH,
		T_FLUSH,
		T_COMMIT,
		T_COMMIT_DFLUSH,
		T_COMMIT_JFLUSH,
		T_COMMIT_CALLBACK,
		T_FINISHED
	}			t_state;

	/*
	 * Where in the log does this transaction's commit start? [no locking]
	 * 로그 영역에서 이 트랜잭션이 시작되는 지점.
	 */
	unsigned long		t_log_start;

	/*
	 * Number of buffers on the t_buffers list [j_list_lock, no locks
	 * needed for jbd2 thread]
	 * 수정된 메타데이터 버퍼들을 묶어놓은 t_buffers 리스트에 속하는 버퍼의 수
	 */
	int			t_nr_buffers;

	/*
	 * Doubly-linked circular list of all buffers reserved but not yet
	 * modified by this transaction [j_list_lock, no locks needed fo
	 * jbd2 thread]
	 * TODO: buffer를 reserve했다는 것의 의미 이해하고 설명 추가
	 */
	struct journal_head	*t_reserved_list;

	/*
	 * Doubly-linked circular list of all metadata buffers owned by this
	 * transaction [j_list_lock, no locks needed for jbd2 thread]
	 * 트랜잭션에 삽입된 수정된 메타데이터 버퍼들의 리스트
	 */
	struct journal_head	*t_buffers;

	/*
	 * Doubly-linked circular list of all forget buffers (superseded
	 * buffers which we can un-checkpoint once this transaction commits)
	 * [j_list_lock]
	 * 버퍼가 저널에 기록되고 나면 여기에 저장된다. DMA 끝난 버퍼들
	 * TODO: 팩트 체크
	 */
	struct journal_head	*t_forget;

	/*
	 * Doubly-linked circular list of all buffers still to be flushed before
	 * this transaction can be checkpointed. [j_list_lock]
	 * 이 트랜잭션이 checkpoint되기 전 flush되어야 하는 모든 버퍼들
	 * TODO: t_forget이랑 차이 알아보기
	 */
	struct journal_head	*t_checkpoint_list;

	/*
	 * Doubly-linked circular list of metadata buffers being
	 * shadowed by log IO.  The IO buffers on the iobuf list and
	 * the shadow buffers on this list match each other one for
	 * one at all times. [j_list_lock, no locks needed for jbd2
	 * thread]
	 * DMA 중인 버퍼들의 리스트
	 * TODO: 팩트 체크
	 */
	struct journal_head	*t_shadow_list;

	/*
	 * List of inodes associated with the transaction; e.g., ext4 uses
	 * this to track inodes in data=ordered and data=journal mode that
	 * need special handling on transaction commit; also used by ocfs2.
	 * [j_list_lock]
	 *
	 * 이 트랜잭션에 기록된 파일 연산이 수정한 파일들의 아이노드들
	 */
	struct list_head	t_inode_list;

	/*
	 * Longest time some handle had to wait for running transaction
	 * TODO: 설명 추가 
	 */
	unsigned long		t_max_wait;

	/*
	 * When transaction started
	 * 트랜잭션 시작 시점.
	 * TODO: 단위 알아보기, 할당 시점부터 측정되는 것인지?
	 */
	unsigned long		t_start;

	/*
	 * When commit was requested [j_state_lock]
	 * 커밋 요청된 시점.
	 */
	unsigned long		t_requested;

	/*
	 * Checkpointing stats [j_list_lock]
	 * 체크포인트 프로파일링용 구조체. 시간 등등 들어있음.
	 */
	struct transaction_chp_stats_s t_chp_stats;

	/*
	 * Number of outstanding updates running on this transaction
	 * [none]
	 * 트랜잭션에 기록 중인 파일 연산의 수
	 * TODO: 어떻게 카운트되는지 설명 추가
	 */
	atomic_t		t_updates;

	/*
	 * Number of blocks reserved for this transaction in the journal.
	 * This is including all credits reserved when starting transaction
	 * handles as well as all journal descriptor blocks needed for this
	 * transaction. [none]
	 * 
	 * 트랜잭션이 로그 영역에서 차지할 것으로 예상되는 블럭의 수
	 * 각 핸들의 credit의 합에 디스크립터 블럭의 수까지 더한 것.
	 */
	atomic_t		t_outstanding_credits;

	/*
	 * Number of revoke records for this transaction added by already
	 * stopped handles. [none]
	 * TODO: 설명 추가
	 */
	atomic_t		t_outstanding_revokes;

	/*
	 * How many handles used this transaction? [none]
	 * TODO: t_updates랑 차이 설명 추가
	 */
	atomic_t		t_handle_count;

	/*
	 * Forward and backward links for the circular list of all transactions
	 * awaiting checkpoint. [j_list_lock]
	 * 체크포인트 트랜잭션 리스트를 구성하기 위한 포인터들
	 */
	transaction_t		*t_cpnext, *t_cpprev;

	/*
	 * When will the transaction expire (become due for commit), in jiffies?
	 * [no locking]
	 * 주기적 커밋이 되어야하는 데드라인
	 */
	unsigned long		t_expires;

	/*
	 * When this transaction started, in nanoseconds [no locking]
	 * TODO: t_start와의 차이는 무엇인지? 단위의 차이인가?
	 */
	ktime_t			t_start_time;

	/*
	 * This transaction is being forced and some process is
	 * waiting for it to finish.
	 * TODO: 설명 추가
	 */
	unsigned int t_synchronous_commit:1;

	/* Disk flush needs to be sent to fs partition [no locking] 
	 * TODO: 설명 추가
	 */
	int			t_need_data_flush;

	/*
	 * For use by the filesystem to store fs-specific data
	 * structures associated with the transaction
	 * TODO: EXT4의 경우 이걸로 어떤 트랜잭션 리스트를 구성하는지 알아보기
	 */
	struct list_head	t_private_list;
};

/* 트랜잭션의 생애가 어떻게 구성됐는지 등을 프로파일링하기 위한 구조체들 */
struct transaction_run_stats_s {
	unsigned long		rs_wait;
	unsigned long		rs_request_delay;
	unsigned long		rs_running;
	unsigned long		rs_locked;
	unsigned long		rs_flushing;
	unsigned long		rs_logging;

	__u32			rs_handle_count;
	__u32			rs_blocks;
	__u32			rs_blocks_logged;
};

struct transaction_stats_s {
	unsigned long		ts_tid;
	unsigned long		ts_requested;
	struct transaction_run_stats_s run;
};

/* TODO: if문 아닌 다른 return 값 설명 추가 */
static inline unsigned long
jbd2_time_diff(unsigned long start, unsigned long end)
{
	if (end >= start)
		return end - start;

	return end + (MAX_JIFFY_OFFSET - start);
}

/* TODO: 설명 추가 */
#define JBD2_NR_BATCH	64

enum passtype {PASS_SCAN, PASS_REVOKE, PASS_REPLAY};

#define JBD2_FC_REPLAY_STOP	0
#define JBD2_FC_REPLAY_CONTINUE	1

/**
 * struct journal_s - The journal_s type is the concrete type associated with
 *     journal_t.
 *
 * 저널 구조체
 */
struct journal_s
{
	/**
	 * @j_flags: General journaling state flags [j_state_lock,
	 * no lock for quick racy checks]
	 * 저널 상태 표시 플래그
	 */
	unsigned long		j_flags;

	/**
	 * @j_errno:
	 *
	 * Is there an outstanding uncleared error on the journal (from a prior
	 * abort)? [j_state_lock]
	 * 저널링 과정에서 에러 발생 시 에러코드를 저장함.
	 */
	int			j_errno;

	/**
	 * @j_abort_mutex: Lock the whole aborting procedure.
	 * TODO: 설명 추가
	 */
	struct mutex		j_abort_mutex;

	/**
	 * @j_sb_buffer: The first part of the superblock buffer.
	 * TODO: 설명 추가, 파일 시스템 수퍼블럭 버퍼를 말하는 것으로 예상됨
	 */
	struct buffer_head	*j_sb_buffer;

	/**
	 * @j_superblock: The second part of the superblock buffer.
	 * 인메모리 저널 수퍼블럭 개체에 대한 포인터
	 * 저널 영역 = 저널 수퍼블럭 + 로그 영역
	 */
	journal_superblock_t	*j_superblock;

	/**
	 * @j_state_lock: Protect the various scalars in the journal.
	 * j_flags등을 읽고 쓰기 위해 획득해야하는 락
	 */
	rwlock_t		j_state_lock;

	/**
	 * @j_barrier_count:
	 *
	 * Number of processes waiting to create a barrier lock [j_state_lock,
	 * no lock for quick racy checks]
	 * TODO: 설명 추가
	 */
	int			j_barrier_count;

	/**
	 * @j_barrier: The barrier lock itself.
	 * TODO: 설명 추가
	 */
	struct mutex		j_barrier;

	/**
	 * @j_running_transaction:
	 *
	 * Transactions: The current running transaction...
	 * [j_state_lock, no lock for quick racy checks] [caller holding
	 * open handle]
	 * 
	 * 저널의 러닝 트랜잭션에 대한 포인터
	 */
	transaction_t		*j_running_transaction;

	/**
	 * @j_committing_transaction:
	 *
	 * the transaction we are pushing to disk
	 * [j_state_lock] [caller holding open handle]
	 *
	 * 저널의 커밋 트랜잭션에 대한 포인터
	 */
	transaction_t		*j_committing_transaction;

	/**
	 * @j_checkpoint_transactions:
	 *
	 * ... and a linked circular list of all transactions waiting for
	 * checkpointing. [j_list_lock]
	 *
	 * JBD에 의해 커밋이 완료된 체크포인트 트랜잭션 묶음에 대한 포인터
	 */
	transaction_t		*j_checkpoint_transactions;

	/**
	 * @j_wait_transaction_locked:
	 *
	 * Wait queue for waiting for a locked transaction to start committing,
	 * or for a barrier lock to be released.
	 *
	 * transaction lock-up 상태에서 파일 연산 스레드들이 대기하는 큐로 보임.
	 */
	wait_queue_head_t	j_wait_transaction_locked;

	/**
	 * @j_wait_done_commit: Wait queue for waiting for commit to complete.
	 *
	 * fsync 호출 스레드가 JBD 데몬을 깨우고 대기하는 큐로 보임.
	 */
	wait_queue_head_t	j_wait_done_commit;

	/**
	 * @j_wait_commit: Wait queue to trigger commit.
	 * 
	 * JBD 데몬이 커밋 요청을 대기하는 큐로 보임.
	 */
	wait_queue_head_t	j_wait_commit;

	/**
	 * @j_wait_updates: Wait queue to wait for updates to complete.
	 *
	 * TODO: 설명 추가
	 */
	wait_queue_head_t	j_wait_updates;

	/**
	 * @j_wait_reserved:
	 *
	 * Wait queue to wait for reserved buffer credits to drop.
	 *
	 * TODO: 설명 추가
	 */
	wait_queue_head_t	j_wait_reserved;

	/**
	 * @j_fc_wait:
	 *
	 * Wait queue to wait for completion of async fast commits.
	 *
	 * TODO: 설명 추가
	 */
	wait_queue_head_t	j_fc_wait;

	/**
	 * @j_checkpoint_mutex:
	 *
	 * Semaphore for locking against concurrent checkpoints.
	 * TODO: 설명 추가
	 */
	struct mutex		j_checkpoint_mutex;

	/**
	 * @j_chkpt_bhs:
	 *
	 * List of buffer heads used by the checkpoint routine.  This
	 * was moved from jbd2_log_do_checkpoint() to reduce stack
	 * usage.  Access to this array is controlled by the
	 * @j_checkpoint_mutex.  [j_checkpoint_mutex]
	 * TODO: 설명 추가
	 */
	struct buffer_head	*j_chkpt_bhs[JBD2_NR_BATCH];

	/**
	 * @j_shrinker:
	 *
	 * Journal head shrinker, reclaim buffer's journal head which
	 * has been written back.
	 * TODO: 설명 추가
	 */
	struct shrinker		*j_shrinker;

	/**
	 * @j_checkpoint_jh_count:
	 *
	 * Number of journal buffers on the checkpoint list. [j_list_lock]
	 * 체크포인트 리스트에 존재하는 버퍼의 수.
	 * TODO: 언제 사용되는지 그리고 왜 percpu인지 확인 후 설명 추가
	 */
	struct percpu_counter	j_checkpoint_jh_count;

	/**
	 * @j_shrink_transaction:
	 *
	 * Record next transaction will shrink on the checkpoint list.
	 * [j_list_lock]
	 * TODO: 설명 추가
	 */
	transaction_t		*j_shrink_transaction;

	/* TODO: head, first 그리고 tail, last의 차이에 대해 알아보고 설명 추가 */
	/**
	 * @j_head:
	 *
	 * Journal head: identifies the first unused block in the journal.
	 * [j_state_lock]
	 */
	unsigned long		j_head;

	/**
	 * @j_tail:
	 *
	 * Journal tail: identifies the oldest still-used block in the journal.
	 * [j_state_lock]
	 */
	unsigned long		j_tail;

	/**
	 * @j_free:
	 *
	 * Journal free: how many free blocks are there in the journal?
	 * [j_state_lock]
	 * 자유 영역의 블럭 수
	 */
	unsigned long		j_free;

	/**
	 * @j_first:
	 *
	 * The block number of the first usable block in the journal
	 * [j_state_lock].
	 */
	unsigned long		j_first;

	/**
	 * @j_last:
	 *
	 * The block number one beyond the last usable block in the journal
	 * [j_state_lock].
	 */
	unsigned long		j_last;

	/**
	 * @j_fc_first:
	 *
	 * The block number of the first fast commit block in the journal
	 * [j_state_lock].
	 *
	 * FC 영역에서 현재 사용중인 영역의 시작 블럭 번호
	 */
	unsigned long		j_fc_first;

	/**
	 * @j_fc_off:
	 *
	 * Number of fast commit blocks currently allocated. Accessed only
	 * during fast commit. Currently only process can do fast commit, so
	 * this field is not protected by any lock.
	 *
	 * 현재 사용 중인  FC영역의 블럭 수
	 * fsync호출 프로세스 하나만 동시에 커밋을 진행할 수 있어서 락이 필요없다는 것 같다.
	 */
	unsigned long		j_fc_off;

	/**
	 * @j_fc_last:
	 *
	 * The block number one beyond the last fast commit block in the journal
	 * [j_state_lock].
	 * 
	 * FC 영역에서 현재 사용 중인 영역의 마지막 블럭 다음 번호
	 */
	unsigned long		j_fc_last;

	/**
	 * @j_dev: Device where we store the journal.
	 * 외부 저널 디바이스를 사용할 경우 그 디바이스의 인메모리 개체를 가리킨다.
	 */
	struct block_device	*j_dev;

	/**
	 * @j_blocksize: Block size for the location where we store the journal.
	 *
	 * 저널을 저장하는 디바이스의 블록 크기
	 */
	int			j_blocksize;

	/**
	 * @j_blk_offset:
	 *
	 * Starting block offset into the device where we store the journal.
	 * 저널 영역 시작 블럭 번호
	 */
	unsigned long long	j_blk_offset;

	/**
	 * @j_devname: Journal device name.
	 * 저널을 저장하는 디바이스의 이름 
	 */
	char			j_devname[BDEVNAME_SIZE+24];

	/**
	 * @j_fs_dev:
	 *
	 * Device which holds the client fs.  For internal journal this will be
	 * equal to j_dev.
	 * 메인 영역(파일, 디렉토리, 비트맵, 아이노드)를 저장하는 디바이스
	 * 외부 저널 디바이스를 쓰기 않으면 저널 디바이스와 동일함.
	 */
	struct block_device	*j_fs_dev;

	/**
	 * @j_fs_dev_wb_err:
	 *
	 * Records the errseq of the client fs's backing block device.
	 * TODO: 설명 추가
	 */
	errseq_t		j_fs_dev_wb_err;

	/**
	 * @j_total_len: Total maximum capacity of the journal region on disk.
	 * 저널 영역 최대 용량
	 */
	unsigned int		j_total_len;

	/**
	 * @j_reserved_credits:
	 *
	 * Number of buffers reserved from the running transaction.
	 * 러닝 트랜잭션에 의해 예약된 저널 영역 블럭 수
	 */
	atomic_t		j_reserved_credits;

	/**
	 * @j_list_lock: Protects the buffer lists and internal buffer state.
	 * 리스트 접근을 동기화하는 락
	 * 위에 락 순서도 참고
	 */
	spinlock_t		j_list_lock;

	/**
	 * @j_inode:
	 *
	 * Optional inode where we store the journal.  If present, all
	 * journal block numbers are mapped into this inode via bmap().
	 * 
	 * 저널 영역을 파일로 취급할 경우 저널 영역을 가리키는 아이노드
	 * TODO: 이건 언제 아이노드 만들고 블럭 맵핑하는지 알아보고 설명 추가. 아마 초기화?
	 */
	struct inode		*j_inode;

	/**
	 * @j_tail_sequence:
	 *
	 * Sequence number of the oldest transaction in the log [j_state_lock]
	 * 점유 영역에서 가장 오래된 트랜잭션의 TID
	 */
	tid_t			j_tail_sequence;

	/**
	 * @j_transaction_sequence:
	 *
	 * Sequence number of the next transaction to grant [j_state_lock]
	 * 트랜잭션이 할당될 때 부여할 tid. 부여할 때마다 증가함. 
	 */
	tid_t			j_transaction_sequence;

	/**
	 * @j_commit_sequence:
	 *
	 * Sequence number of the most recently committed transaction
	 * [j_state_lock, no lock for quick racy checks]
	 * 가장 최근 커밋된 트랜잭션의 tid. 점유 영역 tid 범위 = j_tail_sequence - j_commit_sequence
	 */
	tid_t			j_commit_sequence;

	/**
	 * @j_commit_request:
	 *
	 * Sequence number of the most recent transaction wanting commit
	 * [j_state_lock, no lock for quick racy checks]
	 * 가장 최근 커밋 요청된 트랜잭션의 TID.
	 * TODO: 이건 커밋 도중 중복 커밋 방지하려고 있는 것으로 기억하는데 확인 후 설명 추가
	 */
	tid_t			j_commit_request;

	/**
	 * @j_uuid:
	 *
	 * Journal uuid: identifies the object (filesystem, LVM volume etc)
	 * backed by this journal.  This will eventually be replaced by an array
	 * of uuids, allowing us to index multiple devices within a single
	 * journal and to perform atomic updates across them.
	 * 저널 개체의 uuid
	 */
	__u8			j_uuid[16];

	/**
	 * @j_task: Pointer to the current commit thread for this journal.
	 * 현재 커밋 스레드에 대한 포인터
	 * TODO: JBD 데몬에 대한 포인터인지 fsync 호출 스레드에 대한 포인터인지 확인하기
	 */
	struct task_struct	*j_task;

	/**
	 * @j_max_transaction_buffers:
	 *
	 * Maximum number of metadata buffers to allow in a single compound
	 * commit transaction.
	 * 단일 트랜잭션에서 처리할 수 있는 메타데이터 버퍼의 최대 수량.
	 * TODO: compound transaction이라는게 running만 말하는 건지 running + commit까지 인지 알아보기. 다른 주석에서는 문맥상 running만을 이야기하는 어조였음.
	 */
	int			j_max_transaction_buffers;

	/**
	 * @j_revoke_records_per_block:
	 *
	 * Number of revoke records that fit in one descriptor block.
	 * 디스크립터 블럭 하나에 저장될 수 있는 리보크 레코드의 수
	 */
	int			j_revoke_records_per_block;

	/**
	 * @j_transaction_overhead_buffers:
	 *
	 * Number of blocks each transaction needs for its own bookkeeping
	 */
	int			j_transaction_overhead_buffers;

	/**
	 * @j_commit_interval:
	 *
	 * What is the maximum transaction lifetime before we begin a commit?
	 * 트랜잭션 생성부터 커밋 요청까지 가장 오랜 걸렸던 경우의 시간?
	 * TODO: 설명 추가
	 */
	unsigned long		j_commit_interval;

	/**
	 * @j_commit_timer: The timer used to wakeup the commit thread.
	 * 주기적으로 JBD 데몬 깨우기 위한 타이머
	 */
	struct timer_list	j_commit_timer;

	/**
	 * @j_revoke_lock: Protect the revoke table.
	 * 리보크 테이블 보호하는 락
	 */
	spinlock_t		j_revoke_lock;

	/**
	 * @j_revoke:
	 *
	 * The revoke table - maintains the list of revoked blocks in the
	 * current transaction.
	 * 현재 트랜잭션의 리보크 레코드 리스트
	 */
	struct jbd2_revoke_table_s *j_revoke;

	/**
	 * @j_revoke_table: Alternate revoke tables for j_revoke.
	 * TODO: 설명 추가
	 */
	struct jbd2_revoke_table_s *j_revoke_table[2];

	/**
	 * @j_wbuf: Array of bhs for jbd2_journal_commit_transaction.
	 * 버퍼 여러개 저장해서 몇개 마다 끊어서 io를 내려보내는데 그 때 버퍼 헤드를 저장하는 리스트
	 * TODO: 커밋 분석할 때 설명 추가
	 */
	struct buffer_head	**j_wbuf;

	/**
	 * @j_fc_wbuf: Array of fast commit bhs for fast commit. Accessed only
	 * during a fast commit. Currently only process can do fast commit, so
	 * this field is not protected by any lock.
	 * TODO: j_wbuf의 FastCommit버전인듯. 설명 추가하기
	 */
	struct buffer_head	**j_fc_wbuf;

	/**
	 * @j_wbufsize:
	 *
	 * Size of @j_wbuf array.
	 * j_wubf가 저장할 수 있는 포인터의 수
	 */
	int			j_wbufsize;

	/**
	 * @j_fc_wbufsize:
	 *
	 * Size of @j_fc_wbuf array.
	 * j_wbufsize의 fastcommit버전
	 */
	int			j_fc_wbufsize;

	/**
	 * @j_last_sync_writer:
	 *
	 * The pid of the last person to run a synchronous operation
	 * through the journal.
	 * 마지막으로 fsync 호출한 스레드의 pid
	 */
	pid_t			j_last_sync_writer;

	/**
	 * @j_average_commit_time:
	 *
	 * The average amount of time in nanoseconds it takes to commit a
	 * transaction to disk. [j_state_lock]
	 * 커밋에 걸린 평균적인 시간
	 */
	u64			j_average_commit_time;

	/**
	 * @j_min_batch_time:
	 *
	 * Minimum time that we should wait for additional filesystem operations
	 * to get batched into a synchronous handle in microseconds.
	 * 아마 lock되고 나서 모든 파일 연산 끝날때까지 걸린 최소 시간인 듯?
	 * TODO: 사용처, 설명 추가
	 */
	u32			j_min_batch_time;

	/**
	 * @j_max_batch_time:
	 *
	 * Maximum time that we should wait for additional filesystem operations
	 * to get batched into a synchronous handle in microseconds.
	 * 바로 위에꺼 최대 버전.
	 * TODO: 사용처, 설명 추가
	 */
	u32			j_max_batch_time;

	/**
	 * @j_commit_callback:
	 *
	 * This function is called when a transaction is closed.
	 * TODO: 어떤 함수가 설정되고 언제 호출되는지 설명 추가
	 */
	void			(*j_commit_callback)(journal_t *,
						     transaction_t *);

	/**
	 * @j_submit_inode_data_buffers:
	 *
	 * This function is called for all inodes associated with the
	 * committing transaction marked with JI_WRITE_DATA flag
	 * before we start to write out the transaction to the journal.
	 * TODO: 어떤 함수가 설정되고 언제 호출되는지 설명 추가
	 */
	int			(*j_submit_inode_data_buffers)
					(struct jbd2_inode *);

	/**
	 * @j_finish_inode_data_buffers:
	 *
	 * This function is called for all inodes associated with the
	 * committing transaction marked with JI_WAIT_DATA flag
	 * after we have written the transaction to the journal
	 * but before we write out the commit block.
	 * TODO: 어떤 함수가 설정되고 언제 호출되는지 설명 추가
	 */
	int			(*j_finish_inode_data_buffers)
					(struct jbd2_inode *);

	/* 프로파일링용 필드들. procfs entry도 여기에 있다.*/
	/*
	 * Journal statistics
	 */

	/**
	 * @j_history_lock: Protect the transactions statistics history.
	 */
	spinlock_t		j_history_lock;

	/**
	 * @j_proc_entry: procfs entry for the jbd statistics directory.
	 */
	struct proc_dir_entry	*j_proc_entry;

	/**
	 * @j_stats: Overall statistics.
	 */
	struct transaction_stats_s j_stats;

	/**
	 * @j_failed_commit: Failed journal commit ID.
	 * 커밋이 실패할 경우 실패한 커밋의 id
	 * TODO: 트랜잭션 ID가 맞는지
	 */
	unsigned int		j_failed_commit;

	/**
	 * @j_private:
	 *
	 * An opaque pointer to fs-private information.  ext3 puts its
	 * superblock pointer here.
	 *
	 * ext3의 경우 수퍼블럭 주소를 여기에 저장한다.
	 * TODO: ext4의 경우도 그런지 알아보기
	 */
	void *j_private;

	/**
	 * @j_chksum_driver:
	 *
	 * Reference to checksum algorithm driver via cryptoapi.
	 */
	struct crypto_shash *j_chksum_driver;

	/**
	 * @j_csum_seed:
	 *
	 * Precomputed journal UUID checksum for seeding other checksums.
	 */
	__u32 j_csum_seed;

#ifdef CONFIG_DEBUG_LOCK_ALLOC
	/* lockdep: 커널이 사용하는 여러 락 들의 의존성을 검사하는 디버깅 루틴
	 * Lock inversion, circular dependency 등의 런타임에 탐지해줌.
	 * 일단 넘어가자.
	 */
	/**
	 * @j_trans_commit_map:
	 *
	 * Lockdep entity to track transaction commit dependencies. Handles
	 * hold this "lock" for read, when we wait for commit, we acquire the
	 * "lock" for writing. This matches the properties of jbd2 journalling
	 * where the running transaction has to wait for all handles to be
	 * dropped to commit that transaction and also acquiring a handle may
	 * require transaction commit to finish.
	 */
	struct lockdep_map	j_trans_commit_map;
#endif

	/**
	 * @j_fc_cleanup_callback:
	 *
	 * Clean-up after fast commit or full commit. JBD2 calls this function
	 * after every commit operation.
	 * TODO: fastcommit 분석 시 어떤 함수가 설정되고 언제 호출되는지 설명 추가
	 */
	void (*j_fc_cleanup_callback)(struct journal_s *journal, int full, tid_t tid);

	/**
	 * @j_fc_replay_callback:
	 *
	 * File-system specific function that performs replay of a fast
	 * commit. JBD2 calls this function for each fast commit block found in
	 * the journal. This function should return JBD2_FC_REPLAY_CONTINUE
	 * to indicate that the block was processed correctly and more fast
	 * commit replay should continue. Return value of JBD2_FC_REPLAY_STOP
	 * indicates the end of replay (no more blocks remaining). A negative
	 * return value indicates error.
	 * TODO: fastcommit 분석 시 어떤 함수가 설정되고 언제 호출되는지 설명 추가
	 */
	int (*j_fc_replay_callback)(struct journal_s *journal,
				    struct buffer_head *bh,
				    enum passtype pass, int off,
				    tid_t expected_commit_id);

	/**
	 * @j_bmap:
	 *
	 * Bmap function that should be used instead of the generic
	 * VFS bmap function.
	 * TODO: 사용처, 설명 추가. 아마 저널 영역에 대한 맵핑을 말하는 것 같다.
	 */
	int (*j_bmap)(struct journal_s *journal, sector_t *block);
};

/* TODO: 설명 추가 */
#define jbd2_might_wait_for_commit(j) \
	do { \
		rwsem_acquire(&j->j_trans_commit_map, 0, 0, _THIS_IP_); \
		rwsem_release(&j->j_trans_commit_map, _THIS_IP_); \
	} while (0)

/*
 * We can support any known requested features iff the
 * superblock is not in version 1.  Otherwise we fail to support any
 * extended sb features.
 * 
 * 수퍼블럭 버전이 2라면 특정 feature가 호환되는 커널인지 아닌지를
 * 수퍼블럭을 통해 알 수 있음.
 * 단순히 수퍼블럭 버전을 체크해서 이 기능을 검사하는 함수
 */
static inline bool jbd2_format_support_feature(journal_t *j)
{
	return j->j_superblock->s_header.h_blocktype !=
					cpu_to_be32(JBD2_SUPERBLOCK_V1);
}

/* flags별로 has, set, clear의 세가지 함수를 정의해주는 매크로 
 * 매크로 정의 아래에 현재 커널에서 지원, 미지원하는 feature들에 대한 함수를 정의한다.
 */
/* journal feature predicate functions */
#define JBD2_FEATURE_COMPAT_FUNCS(name, flagname) \
static inline bool jbd2_has_feature_##name(journal_t *j) \
{ \
	return (jbd2_format_support_feature(j) && \
		((j)->j_superblock->s_feature_compat & \
		 cpu_to_be32(JBD2_FEATURE_COMPAT_##flagname)) != 0); \
} \
static inline void jbd2_set_feature_##name(journal_t *j) \
{ \
	(j)->j_superblock->s_feature_compat |= \
		cpu_to_be32(JBD2_FEATURE_COMPAT_##flagname); \
} \
static inline void jbd2_clear_feature_##name(journal_t *j) \
{ \
	(j)->j_superblock->s_feature_compat &= \
		~cpu_to_be32(JBD2_FEATURE_COMPAT_##flagname); \
}

#define JBD2_FEATURE_RO_COMPAT_FUNCS(name, flagname) \
static inline bool jbd2_has_feature_##name(journal_t *j) \
{ \
	return (jbd2_format_support_feature(j) && \
		((j)->j_superblock->s_feature_ro_compat & \
		 cpu_to_be32(JBD2_FEATURE_RO_COMPAT_##flagname)) != 0); \
} \
static inline void jbd2_set_feature_##name(journal_t *j) \
{ \
	(j)->j_superblock->s_feature_ro_compat |= \
		cpu_to_be32(JBD2_FEATURE_RO_COMPAT_##flagname); \
} \
static inline void jbd2_clear_feature_##name(journal_t *j) \
{ \
	(j)->j_superblock->s_feature_ro_compat &= \
		~cpu_to_be32(JBD2_FEATURE_RO_COMPAT_##flagname); \
}

#define JBD2_FEATURE_INCOMPAT_FUNCS(name, flagname) \
static inline bool jbd2_has_feature_##name(journal_t *j) \
{ \
	return (jbd2_format_support_feature(j) && \
		((j)->j_superblock->s_feature_incompat & \
		 cpu_to_be32(JBD2_FEATURE_INCOMPAT_##flagname)) != 0); \
} \
static inline void jbd2_set_feature_##name(journal_t *j) \
{ \
	(j)->j_superblock->s_feature_incompat |= \
		cpu_to_be32(JBD2_FEATURE_INCOMPAT_##flagname); \
} \
static inline void jbd2_clear_feature_##name(journal_t *j) \
{ \
	(j)->j_superblock->s_feature_incompat &= \
		~cpu_to_be32(JBD2_FEATURE_INCOMPAT_##flagname); \
}

JBD2_FEATURE_COMPAT_FUNCS(checksum,		CHECKSUM)

JBD2_FEATURE_INCOMPAT_FUNCS(revoke,		REVOKE)
JBD2_FEATURE_INCOMPAT_FUNCS(64bit,		64BIT)
JBD2_FEATURE_INCOMPAT_FUNCS(async_commit,	ASYNC_COMMIT)
JBD2_FEATURE_INCOMPAT_FUNCS(csum2,		CSUM_V2)
JBD2_FEATURE_INCOMPAT_FUNCS(csum3,		CSUM_V3)
JBD2_FEATURE_INCOMPAT_FUNCS(fast_commit,	FAST_COMMIT)

/* Journal high priority write IO operation flags 
 * 저널링에서 발생하는 IO 요청에 사용되는 request.
 * TODO: IO 우선순위 어떤 플래그에 의해 올라가는 건지 알아보고 설명 추가
 */
#define JBD2_JOURNAL_REQ_FLAGS		(REQ_META | REQ_SYNC | REQ_IDLE)

/*
 * Journal flag definitions
 * j_flags에 저장되는 비트 플래그들
 */
#define JBD2_UNMOUNT	0x001	/* Journal thread is being destroyed */
#define JBD2_ABORT	0x002	/* Journaling has been aborted for errors. */
#define JBD2_ACK_ERR	0x004	/* The errno in the sb has been acked */
#define JBD2_FLUSHED	0x008	/* The journal superblock has been flushed */
#define JBD2_LOADED	0x010	/* The journal superblock has been loaded */
#define JBD2_BARRIER	0x020	/* Use IDE barriers */
#define JBD2_ABORT_ON_SYNCDATA_ERR	0x040	/* Abort the journal on file
						 * data write error in ordered
						 * mode */
#define JBD2_CYCLE_RECORD		0x080	/* Journal cycled record log on
						 * clean and empty filesystem
						 * logging area */
#define JBD2_FAST_COMMIT_ONGOING	0x100	/* Fast commit is ongoing */
#define JBD2_FULL_COMMIT_ONGOING	0x200	/* Full commit is ongoing */
#define JBD2_JOURNAL_FLUSH_DISCARD	0x0001
#define JBD2_JOURNAL_FLUSH_ZEROOUT	0x0002
#define JBD2_JOURNAL_FLUSH_VALID	(JBD2_JOURNAL_FLUSH_DISCARD | \
					JBD2_JOURNAL_FLUSH_ZEROOUT)

/*
 * Function declarations for the journaling transaction and buffer
 * management
 */

/* Filing buffers 
 * TODO: 커밋 분석할 때 리스트 간 버퍼 이동할 때 사용했던 걸로 기억함. 설명 추가하기
 */
extern void jbd2_journal_unfile_buffer(journal_t *, struct journal_head *);
extern bool __jbd2_journal_refile_buffer(struct journal_head *);
extern void jbd2_journal_refile_buffer(journal_t *, struct journal_head *);
extern void __jbd2_journal_file_buffer(struct journal_head *, transaction_t *, int);
extern void jbd2_journal_file_buffer(struct journal_head *, transaction_t *, int);
/* TODO: 커밋 분석 할 때 설명 추가하기 */
static inline void jbd2_file_log_bh(struct list_head *head, struct buffer_head *bh)
{
	list_add_tail(&bh->b_assoc_buffers, head);
}
static inline void jbd2_unfile_log_bh(struct buffer_head *bh)
{
	list_del_init(&bh->b_assoc_buffers);
}

/* Log buffer allocation 
 * TODO: 커밋 분석 할 때 설 명 추가하기.
 */
struct buffer_head *jbd2_journal_get_descriptor_buffer(transaction_t *, int);
void jbd2_descriptor_block_csum_set(journal_t *, struct buffer_head *);
int jbd2_journal_next_log_block(journal_t *, unsigned long long *);
int jbd2_journal_get_log_tail(journal_t *journal, tid_t *tid,
			      unsigned long *block);
int __jbd2_update_log_tail(journal_t *journal, tid_t tid, unsigned long block);
void jbd2_update_log_tail(journal_t *journal, tid_t tid, unsigned long block);

/* Commit management 
 * JBD2의 메인 루틴. 실제 커밋이 이뤄지는 함수.
 */
extern void jbd2_journal_commit_transaction(journal_t *);

/* Checkpoint list management 
 * TODO: 체크포인트 분석할 때 설명 추가하기
 */
enum jbd2_shrink_type {JBD2_SHRINK_DESTROY, JBD2_SHRINK_BUSY_STOP, JBD2_SHRINK_BUSY_SKIP};
void __jbd2_journal_clean_checkpoint_list(journal_t *journal, enum jbd2_shrink_type type);
unsigned long jbd2_journal_shrink_checkpoint_list(journal_t *journal, unsigned long *nr_to_scan);
int __jbd2_journal_remove_checkpoint(struct journal_head *);
int jbd2_journal_try_remove_checkpoint(struct journal_head *jh);
void jbd2_journal_destroy_checkpoint(journal_t *journal);
void __jbd2_journal_insert_checkpoint(struct journal_head *, transaction_t *);


/*
 * Triggers
 * TODO: trigger 호출하는 코드 보면 설명 추가하기
 */

struct jbd2_buffer_trigger_type {
	/*
	 * Fired a the moment data to write to the journal are known to be
	 * stable - so either at the moment b_frozen_data is created or just
	 * before a buffer is written to the journal.  mapped_data is a mapped
	 * buffer that is the frozen data for commit.
	 */
	void (*t_frozen)(struct jbd2_buffer_trigger_type *type,
			 struct buffer_head *bh, void *mapped_data,
			 size_t size);

	/*
	 * Fired during journal abort for dirty buffers that will not be
	 * committed.
	 */
	void (*t_abort)(struct jbd2_buffer_trigger_type *type,
			struct buffer_head *bh);
};

extern void jbd2_buffer_frozen_trigger(struct journal_head *jh,
				       void *mapped_data,
				       struct jbd2_buffer_trigger_type *triggers);
extern void jbd2_buffer_abort_trigger(struct journal_head *jh,
				      struct jbd2_buffer_trigger_type *triggers);

/* Buffer IO 
 * submit_bh()해서 메타데이터 실제로 io 요청하는 코드로 기억함.
 * TODO: 팩트 체크
 */
extern int jbd2_journal_write_metadata_buffer(transaction_t *transaction,
					      struct journal_head *jh_in,
					      struct buffer_head **bh_out,
					      sector_t blocknr);

/* Transaction cache support 
 * TODO: 설명 추가
 */
extern void jbd2_journal_destroy_transaction_cache(void);
extern int __init jbd2_journal_init_transaction_cache(void);
extern void jbd2_journal_free_transaction(transaction_t *);

/*
 * Journal locking.
 *
 * We need to lock the journal during transaction state changes so that nobody
 * ever tries to take a handle on the running transaction while we are in the
 * middle of moving it to the commit phase.  j_state_lock does this.
 *
 * Note that the locking is completely interrupt unsafe.  We never touch
 * journal structures from interrupts.
 */

/* 파일연산 스레드의 task struct의 journal infor라는 void 포인터에 핸들을 저장하는 듯*/
static inline handle_t *journal_current_handle(void)
{
	return current->journal_info;
}

/* The journaling code user interface:
 *
 * Create and destroy handles
 * Register buffer modifications against the current transaction.
 * 
 * 트랜잭션로의 수정사항 삽입과 관련된 것을 보임.
 * 파일 시스템(jbd2의 user)에서 jbd를 사용하기 위한 api 로 보인다.
 * TODO: ext4 시스템 콜 분석 시 설명 추가
 */

extern handle_t *jbd2_journal_start(journal_t *, int nblocks);
extern handle_t *jbd2__journal_start(journal_t *, int blocks, int rsv_blocks,
				     int revoke_records, gfp_t gfp_mask,
				     unsigned int type, unsigned int line_no);
extern int	 jbd2_journal_restart(handle_t *, int nblocks);
extern int	 jbd2__journal_restart(handle_t *, int nblocks,
				       int revoke_records, gfp_t gfp_mask);
extern int	 jbd2_journal_start_reserved(handle_t *handle,
				unsigned int type, unsigned int line_no);
extern void	 jbd2_journal_free_reserved(handle_t *handle);
extern int	 jbd2_journal_extend(handle_t *handle, int nblocks,
				     int revoke_records);
extern int	 jbd2_journal_get_write_access(handle_t *, struct buffer_head *);
extern int	 jbd2_journal_get_create_access (handle_t *, struct buffer_head *);
extern int	 jbd2_journal_get_undo_access(handle_t *, struct buffer_head *);
void		 jbd2_journal_set_triggers(struct buffer_head *,
					   struct jbd2_buffer_trigger_type *type);
extern int	 jbd2_journal_dirty_metadata (handle_t *, struct buffer_head *);
extern int	 jbd2_journal_forget (handle_t *, struct buffer_head *);
int jbd2_journal_invalidate_folio(journal_t *, struct folio *,
					size_t offset, size_t length);
bool jbd2_journal_try_to_free_buffers(journal_t *journal, struct folio *folio);
extern int	 jbd2_journal_stop(handle_t *);
extern int	 jbd2_journal_flush(journal_t *journal, unsigned int flags);
extern void	 jbd2_journal_lock_updates (journal_t *);
extern void	 jbd2_journal_unlock_updates (journal_t *);

void jbd2_journal_wait_updates(journal_t *);

extern journal_t * jbd2_journal_init_dev(struct block_device *bdev,
				struct block_device *fs_dev,
				unsigned long long start, int len, int bsize);
extern journal_t * jbd2_journal_init_inode (struct inode *);
extern int	   jbd2_journal_update_format (journal_t *);
extern int	   jbd2_journal_check_used_features
		   (journal_t *, unsigned long, unsigned long, unsigned long);
extern int	   jbd2_journal_check_available_features
		   (journal_t *, unsigned long, unsigned long, unsigned long);
extern int	   jbd2_journal_set_features
		   (journal_t *, unsigned long, unsigned long, unsigned long);
extern void	   jbd2_journal_clear_features
		   (journal_t *, unsigned long, unsigned long, unsigned long);
extern int	   jbd2_journal_load       (journal_t *journal);
extern int	   jbd2_journal_destroy    (journal_t *);
extern int	   jbd2_journal_recover    (journal_t *journal);
extern int	   jbd2_journal_wipe       (journal_t *, int);
extern int	   jbd2_journal_skip_recovery	(journal_t *);
extern void	   jbd2_journal_update_sb_errno(journal_t *);
extern int	   jbd2_journal_update_sb_log_tail	(journal_t *, tid_t,
				unsigned long, blk_opf_t);
extern void	   jbd2_journal_abort      (journal_t *, int);
extern int	   jbd2_journal_errno      (journal_t *);
extern void	   jbd2_journal_ack_err    (journal_t *);
extern int	   jbd2_journal_clear_err  (journal_t *);
extern int	   jbd2_journal_bmap(journal_t *, unsigned long, unsigned long long *);
extern int	   jbd2_journal_force_commit(journal_t *);
extern int	   jbd2_journal_force_commit_nested(journal_t *);
extern int	   jbd2_journal_inode_ranged_write(handle_t *handle,
			struct jbd2_inode *inode, loff_t start_byte,
			loff_t length);
extern int	   jbd2_journal_inode_ranged_wait(handle_t *handle,
			struct jbd2_inode *inode, loff_t start_byte,
			loff_t length);
extern int	   jbd2_journal_finish_inode_data_buffers(
			struct jbd2_inode *jinode);
extern int	   jbd2_journal_begin_ordered_truncate(journal_t *journal,
				struct jbd2_inode *inode, loff_t new_size);
extern void	   jbd2_journal_init_jbd_inode(struct jbd2_inode *jinode, struct inode *inode);
extern void	   jbd2_journal_release_jbd_inode(journal_t *journal, struct jbd2_inode *jinode);

/*
 * journal_head management
 * 버퍼헤드에 저널헤드를 연결, 해제하는 api로 보임.
 */
struct journal_head *jbd2_journal_add_journal_head(struct buffer_head *bh);
struct journal_head *jbd2_journal_grab_journal_head(struct buffer_head *bh);
void jbd2_journal_put_journal_head(struct journal_head *jh);

/*
 * handle management
 * 핸들에 필요한 메모리를 동적으로 할당, 해제하는 api로 보임.
 */
extern struct kmem_cache *jbd2_handle_cache;

/*
 * This specialized allocator has to be a macro for its allocations to be
 * accounted separately (to have a separate alloc_tag). The typecast is
 * intentional to enforce typesafety.
 */
#define jbd2_alloc_handle(_gfp_flags) \
		((handle_t *)kmem_cache_zalloc(jbd2_handle_cache, _gfp_flags))

static inline void jbd2_free_handle(handle_t *handle)
{
	kmem_cache_free(jbd2_handle_cache, handle);
}

/*
 * jbd2_inode management (optional, for those file systems that want to use
 * dynamically allocated jbd2_inode structures)
 *
 * jbd_inode 개체를 위한 메모리를 동적으로 할당, 해제하는 api로 보임.
 */
extern struct kmem_cache *jbd2_inode_cache;

/*
 * This specialized allocator has to be a macro for its allocations to be
 * accounted separately (to have a separate alloc_tag). The typecast is
 * intentional to enforce typesafety.
 */
#define jbd2_alloc_inode(_gfp_flags) \
		((struct jbd2_inode *)kmem_cache_zalloc(jbd2_inode_cache, _gfp_flags))


static inline void jbd2_free_inode(struct jbd2_inode *jinode)
{
	kmem_cache_free(jbd2_inode_cache, jinode);
}

/* Primary revoke support 
 * revoke 관련 api인듯.
 * TODO: revoke.c 분석 시 설명 추가
 */
#define JOURNAL_REVOKE_DEFAULT_HASH 256
extern int	   jbd2_journal_init_revoke(journal_t *, int);
extern void	   jbd2_journal_destroy_revoke_record_cache(void);
extern void	   jbd2_journal_destroy_revoke_table_cache(void);
extern int __init jbd2_journal_init_revoke_record_cache(void);
extern int __init jbd2_journal_init_revoke_table_cache(void);

extern void	   jbd2_journal_destroy_revoke(journal_t *);
extern int	   jbd2_journal_revoke (handle_t *, unsigned long long, struct buffer_head *);
extern int	   jbd2_journal_cancel_revoke(handle_t *, struct journal_head *);
extern void	   jbd2_journal_write_revoke_records(transaction_t *transaction,
						     struct list_head *log_bufs);

/* Recovery revoke support 
 * TODO: recovery.c 분석 시 설명 추가
 */
extern int	jbd2_journal_set_revoke(journal_t *, unsigned long long, tid_t);
extern int	jbd2_journal_test_revoke(journal_t *, unsigned long long, tid_t);
extern void	jbd2_journal_clear_revoke(journal_t *);
extern void	jbd2_journal_switch_revoke_table(journal_t *journal);
extern void	jbd2_clear_buffer_revoked_flags(journal_t *journal);

/*
 * The log thread user interface:
 *
 * Request space in the current transaction, and force transaction commit
 * transitions on demand.
 *
 * 파일연산 스레드가 사용하는 api들 같음.
 * TODO: ext4 시스템 콜 분석 시 추가
 */

int jbd2_log_start_commit(journal_t *journal, tid_t tid);
int jbd2_journal_start_commit(journal_t *journal, tid_t *tid);
int jbd2_log_wait_commit(journal_t *journal, tid_t tid);
int jbd2_transaction_committed(journal_t *journal, tid_t tid);
int jbd2_complete_transaction(journal_t *journal, tid_t tid);
int jbd2_log_do_checkpoint(journal_t *journal);
int jbd2_trans_will_send_data_barrier(journal_t *journal, tid_t tid);

void __jbd2_log_wait_for_space(journal_t *journal);
extern void __jbd2_journal_drop_transaction(journal_t *, transaction_t *);
extern int jbd2_cleanup_journal_tail(journal_t *);

/* Fast commit related APIs 
 * TODO: fastcommit 분석시 설명 추가
 */
int jbd2_fc_begin_commit(journal_t *journal, tid_t tid);
int jbd2_fc_end_commit(journal_t *journal);
int jbd2_fc_end_commit_fallback(journal_t *journal);
int jbd2_fc_get_buf(journal_t *journal, struct buffer_head **bh_out);
int jbd2_submit_inode_data(journal_t *journal, struct jbd2_inode *jinode);
int jbd2_wait_inode_data(journal_t *journal, struct jbd2_inode *jinode);
int jbd2_fc_wait_bufs(journal_t *journal, int num_blks);
void jbd2_fc_release_bufs(journal_t *journal);

/*
 * is_journal_abort
 *
 * Simple test wrapper function to test the JBD2_ABORT state flag.  This
 * bit, when set, indicates that we have had a fatal error somewhere,
 * either inside the journaling layer or indicated to us by the client
 * (eg. ext3), and that we and should not commit any further
 * transactions.
 *
 * abort, error 관련 함수들
 * 저널이나 핸들의 플래그를 검사하거나 설정한다.
 */

static inline int is_journal_aborted(journal_t *journal)
{
	return journal->j_flags & JBD2_ABORT;
}

static inline int is_handle_aborted(handle_t *handle)
{
	if (handle->h_aborted || !handle->h_transaction)
		return 1;
	return is_journal_aborted(handle->h_transaction->t_journal);
}

static inline void jbd2_journal_abort_handle(handle_t *handle)
{
	handle->h_aborted = 1;
}

static inline void jbd2_init_fs_dev_write_error(journal_t *journal)
{
	struct address_space *mapping = journal->j_fs_dev->bd_mapping;

	/*
	 * Save the original wb_err value of client fs's bdev mapping which
	 * could be used to detect the client fs's metadata async write error.
	 */
	errseq_check_and_advance(&mapping->wb_err, &journal->j_fs_dev_wb_err);
}

static inline int jbd2_check_fs_dev_write_error(journal_t *journal)
{
	struct address_space *mapping = journal->j_fs_dev->bd_mapping;

	return errseq_check(&mapping->wb_err,
			    READ_ONCE(journal->j_fs_dev_wb_err));
}

#endif /* __KERNEL__   */

/* Comparison functions for transaction IDs: perform comparisons using
 * modulo arithmetic so that they work over sequence number wraps. */

/* 중복 커밋을 막거나 커밋 요청이 있는지 확인하기 위해서 tid를 비교한다. 
 * tid 비교를 위해 사용되는 함수
 */
static inline int tid_gt(tid_t x, tid_t y)
{
	int difference = (x - y);
	return (difference > 0);
}

static inline int tid_geq(tid_t x, tid_t y)
{
	int difference = (x - y);
	return (difference >= 0);
}

extern int jbd2_journal_blocks_per_page(struct inode *inode);
extern size_t journal_tag_bytes(journal_t *journal);

static inline bool jbd2_journal_has_csum_v2or3_feature(journal_t *j)
{
	return jbd2_has_feature_csum2(j) || jbd2_has_feature_csum3(j);
}

static inline int jbd2_journal_has_csum_v2or3(journal_t *journal)
{
	WARN_ON_ONCE(jbd2_journal_has_csum_v2or3_feature(journal) &&
		     journal->j_chksum_driver == NULL);

	return journal->j_chksum_driver != NULL;
}

/* fc 블럭 개수, 수퍼블럭 버전1이면 num_fc_blocks가 0으로 저장되어서 기본값 리턴하는 듯*/
static inline int jbd2_journal_get_num_fc_blks(journal_superblock_t *jsb)
{
	int num_fc_blocks = be32_to_cpu(jsb->s_num_fc_blks);

	return num_fc_blocks ? num_fc_blocks : JBD2_DEFAULT_FAST_COMMIT_BLOCKS;
}

/*
 * Return number of free blocks in the log. Must be called under j_state_lock.
 * 로그 영역에 자유 영역에 속하는 블럭 수를 리턴한다.
 * 우선 현재 free블럭 수를 세고 커밋중인 트랜잭션이 있으면 파일 연산들에 할당된
 * 크레딧을 뺀다.
 * TODO: 왜 32를 빼는 것인 rounding error랑 관련있는 건지?
 */
static inline unsigned long jbd2_log_space_left(journal_t *journal)
{
	/* Allow for rounding errors */
	long free = journal->j_free - 32;

	if (journal->j_committing_transaction) {
		free -= atomic_read(&journal->
                        j_committing_transaction->t_outstanding_credits);
	}
	return max_t(long, free, 0);
}

/*
 * Definitions which augment the buffer_head layer
 */

/* journaling buffer types 
 * 버퍼가 저널링 단계중 어떤 단계인지를 나타내는 타입으로 기억함.
 * TODO: 커밋 분석시 설명 추가
 */
#define BJ_None		0	/* Not journaled */
#define BJ_Metadata	1	/* Normal journaled metadata */
#define BJ_Forget	2	/* Buffer superseded by this transaction */
#define BJ_Shadow	3	/* Buffer contents being shadowed to the log */
#define BJ_Reserved	4	/* Buffer is reserved for access by journal */
#define BJ_Types	5

/* JBD uses a CRC32 checksum */
#define JBD_MAX_CHECKSUM_SIZE 4

static inline u32 jbd2_chksum(journal_t *journal, u32 crc,
			      const void *address, unsigned int length)
{
	struct {
		struct shash_desc shash;
		char ctx[JBD_MAX_CHECKSUM_SIZE];
	} desc;
	int err;

	BUG_ON(crypto_shash_descsize(journal->j_chksum_driver) >
		JBD_MAX_CHECKSUM_SIZE);

	desc.shash.tfm = journal->j_chksum_driver;
	*(u32 *)desc.ctx = crc;

	err = crypto_shash_update(&desc.shash, address, length);
	BUG_ON(err);

	return *(u32 *)desc.ctx;
}

/* Return most recent uncommitted transaction 
 * 러닝 트랜잭션이 있으면 그 tid를 리턴하고 아니면 가장 최근 커밋 요청된 트랜잭션의
 * tid를 리턴한다.
 */
static inline tid_t  jbd2_get_latest_transaction(journal_t *journal)
{
	tid_t tid;

	read_lock(&journal->j_state_lock);
	tid = journal->j_commit_request;
	if (journal->j_running_transaction)
		tid = journal->j_running_transaction->t_tid;
	read_unlock(&journal->j_state_lock);
	return tid;
}

/*
 * TODO: 함수 호출하는 부분 발견 시 분석 추가
 */
static inline int jbd2_handle_buffer_credits(handle_t *handle)
{
	journal_t *journal;

	if (!handle->h_reserved)
		journal = handle->h_transaction->t_journal;
	else
		journal = handle->h_journal;

	return handle->h_total_credits -
		DIV_ROUND_UP(handle->h_revoke_credits_requested,
			     journal->j_revoke_records_per_block);
}

#ifdef __KERNEL__

#define buffer_trace_init(bh)	do {} while (0)
#define print_buffer_fields(bh)	do {} while (0)
#define print_buffer_trace(bh)	do {} while (0)
#define BUFFER_TRACE(bh, info)	do {} while (0)
#define BUFFER_TRACE2(bh, bh2, info)	do {} while (0)
#define JBUFFER_TRACE(jh, info)	do {} while (0)

#endif	/* __KERNEL__ */

#define EFSBADCRC	EBADMSG		/* Bad CRC detected */
#define EFSCORRUPTED	EUCLEAN		/* Filesystem is corrupted */

#endif	/* _LINUX_JBD2_H */
