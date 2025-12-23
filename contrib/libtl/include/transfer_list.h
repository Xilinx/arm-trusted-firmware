/*
 * Copyright The Transfer List Library Contributors
 *
 * SPDX-License-Identifier: MIT OR GPL-2.0-or-later
 */

#ifndef TRANSFER_LIST_H
#define TRANSFER_LIST_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TRANSFER_LIST_SIGNATURE 0x4a0fb10b
#define TRANSFER_LIST_VERSION 0x0001

/*
 * Init value of maximum alignment required by any TE data in the TL
 * specified as a power of two
 */
#define TRANSFER_LIST_INIT_MAX_ALIGN 3U

/* Alignment required by TE header start address, in bytes */
#define TRANSFER_LIST_GRANULE 8U

/*
 * Version of the register convention used.
 * Set to 1 for both AArch64 and AArch32 according to fw handoff spec v0.9
 */
#define REGISTER_CONVENTION_VERSION_SHIFT_64 32UL
#define REGISTER_CONVENTION_VERSION_SHIFT_32 24UL
#define REGISTER_CONVENTION_VERSION_MASK 0xffUL
#define REGISTER_CONVENTION_VERSION 1UL

#define TRANSFER_LIST_HANDOFF_X1_VALUE(__version)                \
	((TRANSFER_LIST_SIGNATURE &                              \
	  ((1UL << REGISTER_CONVENTION_VERSION_SHIFT_64) - 1)) | \
	 (((__version)&REGISTER_CONVENTION_VERSION_MASK)         \
	  << REGISTER_CONVENTION_VERSION_SHIFT_64))

#define TRANSFER_LIST_HANDOFF_R1_VALUE(__version)                \
	((TRANSFER_LIST_SIGNATURE &                              \
	  ((1UL << REGISTER_CONVENTION_VERSION_SHIFT_32) - 1)) | \
	 (((__version)&REGISTER_CONVENTION_VERSION_MASK)         \
	  << REGISTER_CONVENTION_VERSION_SHIFT_32))

#ifndef __ASSEMBLER__

#define TL_FLAGS_HAS_CHECKSUM (1U << 0U)

enum transfer_list_tag_id {
	TL_TAG_EMPTY = 0,
	TL_TAG_FDT = 1,
	TL_TAG_HOB_BLOCK = 2,
	TL_TAG_HOB_LIST = 3,
	TL_TAG_ACPI_TABLE_AGGREGATE = 4,
	TL_TAG_TPM_EVLOG = 5,
	TL_TAG_OPTEE_PAGABLE_PART = 0x100,
	TL_TAG_DT_SPMC_MANIFEST = 0x101,
	TL_TAG_EXEC_EP_INFO64 = 0x102,
	TL_TAG_SRAM_LAYOUT64 = 0x104,
	TL_TAG_MBEDTLS_HEAP_INFO = 0x105,
	TL_TAG_DT_FFA_MANIFEST = 0x106,
	TL_TAG_SRAM_LAYOUT32 = 0x107,
	TL_TAG_EXEC_EP_INFO32 = 0x108,
};

enum transfer_list_ops {
	TL_OPS_NON, /* invalid for any operation */
	TL_OPS_ALL, /* valid for all operations */
	TL_OPS_RO, /* valid for read only */
	TL_OPS_CUS, /* abort or switch to special code to interpret */
};

struct transfer_list_header {
	uint32_t signature;
	uint8_t checksum;
	uint8_t version;
	uint8_t hdr_size;
	uint8_t alignment; /* max alignment of TE data */
	uint32_t size; /* TL header + all TEs */
	uint32_t max_size;
	uint32_t flags;
	uint32_t reserved; /* spare bytes */
	/*
	 * Commented out element used to visualize dynamic part of the
	 * data structure.
	 *
	 * Note that struct transfer_list_entry also is dynamic in size
	 * so the elements can't be indexed directly but instead must be
	 * traversed in order
	 *
	 * struct transfer_list_entry entries[];
	 */
};

struct __attribute__((packed)) transfer_list_entry {
	uint32_t tag_id : 24;
	uint8_t hdr_size;
	uint32_t data_size;
	/*
	 * Commented out element used to visualize dynamic part of the
	 * data structure.
	 *
	 * Note that padding is added at the end of @data to make to reach
	 * a 8-byte boundary.
	 *
	 * uint8_t	data[ROUNDUP(data_size, 8)];
	 */
};

/*
 * Provide a backward-compatible implementation of static_assert.
 * This keyword was introduced in C11, so it may be unavailable in
 * projects targeting older C standards.
 */
#if __STDC_VERSION__ >= 201112L
#define LIBTL_STATIC_ASSERT(cond, msg) _Static_assert(cond, #msg)
#else
#define LIBTL_STATIC_ASSERT(cond, msg) \
	typedef char static_assertion_##msg[(cond) ? 1 : -1]
#endif

LIBTL_STATIC_ASSERT(sizeof(struct transfer_list_entry) == 0x8U,
		    assert_transfer_list_entry_size);

/**
 * Initialize a transfer list in a reserved memory region.
 *
 * Sets up an empty transfer list at the specified address. Ensures alignment
 * and zeroes the region. Compliant to 2.4.5 of Firmware Handoff specification
 * (v0.9).
 *
 * @param[in]  addr      Pointer to memory to initialize as a transfer list.
 * @param[in]  max_size  Total size of the memory region in bytes.
 *
 * @return Pointer to the initialized transfer list, or NULL on error.
 */
struct transfer_list_header *transfer_list_init(void *addr, size_t max_size);

/**
 * Relocate a transfer list to a new memory region.
 *
 * Moves an existing transfer list to a new location while preserving alignment
 * and contents. Updates internal pointers and checksum accordingly. Compliant
 * to 2.4.6 of Firmware Handoff specification (v0.9).
 *
 * @param[in]  tl        Pointer to the existing transfer list.
 * @param[in]  addr      Target address for relocation.
 * @param[in]  max_size  Size of the target memory region in bytes.
 *
 * @return Pointer to the relocated transfer list, or NULL on error.
 */
struct transfer_list_header *
transfer_list_relocate(struct transfer_list_header *tl, void *addr,
		       size_t max_size);

/**
 * Check the validity of a transfer list header.
 *
 * Validates signature, size, version, and checksum of a transfer list.
 * Compliant to 2.4.1 of Firmware Handoff specification (v0.9)
 *
 * @param[in] tl  Pointer to the transfer list to verify.
 *
 * @return A transfer_list_ops code indicating valid operations.
 */
enum transfer_list_ops
transfer_list_check_header(const struct transfer_list_header *tl);

/**
 * Get the next transfer entry in the list.
 *
 * Enumerates transfer entries starting from the given entry or from the beginning.
 *
 * @param[in]  tl    Pointer to the transfer list.
 * @param[in]  last  Pointer to the previous entry, or NULL to start at the beginning.
 *
 * @return Pointer to the next valid entry, or NULL on error or end of list.
 */
struct transfer_list_entry *
transfer_list_next(struct transfer_list_header *tl,
		   struct transfer_list_entry *last);

/**
 * Get the previous transfer entry in the list.
 *
 * Enumerates transfer entries backward from the given entry.
 *
 * @param[in]  tl    Pointer to the transfer list.
 * @param[in]  last  Pointer to the current entry.
 *
 * @return Pointer to the previous valid entry, or NULL on error or start of list.
 */
struct transfer_list_entry *transfer_list_prev(struct transfer_list_header *tl,
	struct transfer_list_entry *last);

/**
 * Update the checksum of a transfer list.
 *
 * Recomputes and updates the checksum field based on current contents.
 *
 * @param[in,out] tl  Pointer to the transfer list to update.
 */
void transfer_list_update_checksum(struct transfer_list_header *tl);

/**
 * Verify the checksum of a transfer list.
 *
 * Computes and checks the checksum against the stored value.
 *
 * @param[in] tl  Pointer to the transfer list to verify.
 *
 * @return true if checksum is valid, false otherwise.
 */
bool transfer_list_verify_checksum(const struct transfer_list_header *tl);

/**
 * Set the data size of a transfer entry.
 *
 * Modifies the data size field of a given entry and adjusts subsequent entries.
 *
 * @param[in,out] tl            Pointer to the parent transfer list.
 * @param[in,out] te            Pointer to the entry to modify.
 * @param[in]     new_data_size New size of the entry data in bytes.
 *
 * @return true on success, false on error.
 */
bool transfer_list_set_data_size(struct transfer_list_header *tl,
				 struct transfer_list_entry *te,
				 uint32_t new_data_size);

/**
 * Remove a transfer entry by marking it as empty.
 *
 * Clears the tag of a given entry and updates the list checksum.
 *
 * @param[in,out] tl  Pointer to the transfer list.
 * @param[in,out] te  Pointer to the entry to remove.
 *
 * @return true on success, false on error.
 */
bool transfer_list_rem(struct transfer_list_header *tl,
		       struct transfer_list_entry *te);

/**
 * Add a new transfer entry to the list.
 *
 * Appends a new entry with specified tag and data to the list tail.
 *
 * @param[in,out] tl         Pointer to the transfer list.
 * @param[in]     tag_id     Tag identifier for the new entry.
 * @param[in]     data_size  Size of the entry data in bytes.
 * @param[in]     data       Pointer to the data to copy (can be NULL).
 *
 * @return Pointer to the added entry, or NULL on error.
 */
struct transfer_list_entry *transfer_list_add(struct transfer_list_header *tl,
					      uint32_t tag_id,
					      uint32_t data_size,
					      const void *data);

/**
 * Add a new transfer entry with specific alignment requirements.
 *
 * Inserts padding if needed to meet the alignment of the entry data.
 *
 * @param[in,out] tl         Pointer to the transfer list.
 * @param[in]     tag_id     Tag identifier for the new entry.
 * @param[in]     data_size  Size of the entry data in bytes.
 * @param[in]     data       Pointer to the data to copy (can be NULL).
 * @param[in]     alignment  Desired alignment (as log2 value).
 *
 * @return Pointer to the added entry, or NULL on error.
 */
struct transfer_list_entry *
transfer_list_add_with_align(struct transfer_list_header *tl, uint32_t tag_id,
			     uint32_t data_size, const void *data,
			     uint8_t alignment);

/**
 * Find an entry in the transfer list by tag.
 *
 * Search for an existing transfer entry with the specified tag id from a
 * transfer list
 *
 * @param[in] tl      Pointer to the transfer list.
 * @param[in] tag_id  Tag identifier to search for.
 *
 * @return Pointer to the found entry, or NULL if not found.
 */
struct transfer_list_entry *transfer_list_find(struct transfer_list_header *tl,
					       uint32_t tag_id);

/**
 * Set handoff arguments in the entry point info structure.
 *
 * This function populates the provided entry point info structure with data
 * from the transfer list.
 *
 * @param[in] tl        Pointer to the transfer list.
 * @param[out] ep_info  Pointer to the entry point info structure to populate.
 *
 * @return Pointer to the populated entry point info structure.
 */
struct entry_point_info *
transfer_list_set_handoff_args(struct transfer_list_header *tl,
			       struct entry_point_info *ep_info);

/**
 * Get the data pointer of a transfer entry.
 *
 * Returns a pointer to the start of the entry's data buffer.
 *
 * @param[in] entry  Pointer to the transfer entry.
 *
 * @return Pointer to the data section, or NULL on error.
 */
void *transfer_list_entry_data(struct transfer_list_entry *entry);

/**
 * Ensure the transfer list is initialized.
 *
 * Verifies that the transfer list has not already been initialized, then
 * initializes it at the specified memory location.
 *
 * @param[in]  addr  Pointer to the memory to verify or initialize.
 * @param[in]  size  Size of the memory in bytes.
 *
 * @return Pointer to a valid transfer list, or NULL on error.
 */
struct transfer_list_header *transfer_list_ensure(void *addr, size_t size);

/**
 * Dump the contents of a transfer list.
 *
 * @param[in] tl   Pointer to the transfer list.
 */
void transfer_list_dump(struct transfer_list_header *tl);

/**
 * Dump the contents of a transfer entry.
 *
 * @param[in] te  Pointer to the transfer entry.
 */
void transfer_entry_dump(struct transfer_list_entry *te);

#endif /* __ASSEMBLER__ */
#endif /* TRANSFER_LIST_H */
