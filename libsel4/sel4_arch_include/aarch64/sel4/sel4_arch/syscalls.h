/*
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <sel4/config.h>
#include <sel4/functions.h>
#include <sel4/types.h>
#include <sel4/invocation.h>

#ifdef CONFIG_KERNEL_MCS
#define MCS_PARAM_DECL(r)    register seL4_Word reply_reg asm(r) = reply
#define MCS_PARAM    , "r"(reply_reg)
#else
#define MCS_PARAM_DECL(r)
#define MCS_PARAM
#endif

/*
 * To simplify the definition of the various seL4 syscalls/syscall-wrappers we define
 * some helper assembly functions. These functions are designed to cover the different
 * cases of sending/receiving data in registers to/from the kernel. The most 'complex'
 * version is arm_sys_send_recv, and all other functions are limited versions that allow
 * for registers to not be unnecessarily clobbered
 *
 * arm_sys_send: Fills all registers into the kernel, expects nothing to be sent back
 *      by the kernel. Used for direction one way sends that contain data (e.g. seL4_Send,
 *      seL4_NBSend)
 *
 * arm_sys_send_null: Only fills metadata registers into the kernel (skips message
 *      registers). Expects nothing to be sent back by the kernel. Used by directional
 *      one way sends that do not contain data (e.g. seL4_Notify)
 *
 * arm_sys_reply: Similar to arm_sys_send except it does not take a word for the
 *      destination register. Used for undirected one way sends that contain data
 *      (e.g. seL4_Reply)
 *
 * arm_sys_recv: Sends one register (destination) to the kernel and expects all
 *      registers to be returned by the kernel. Used for directed receives that return
 *      data (e.g. seL4_Recv)
 *
 * arm_sys_send_recv: Fills all registers into the kernel and expects all of them
 *      to be filled on return by the kernel. Used for directed send+receives
 *      where data flows both directions (e.g. seL4_Call, seL4_ReplyWait)
 *
 * arm_sys_nbsend_recv: Fills all registers into the kernel and expects all of them
 *      to be filled on return by the kernel. Used for directed send+receives
 *      where data flows both directions on separate caps (e.g. seL4_NBSendRecv)
 *
 * arm_sys_null: Does not send any registers to the kernel or expect anything to
 *      be returned from the kernel. Used to trigger implicit kernel actions without
 *      any data (e.g. seL4_Yield)
 */

static inline void arm_sys_send(seL4_Word sys, seL4_Word dest, seL4_Word info_arg, seL4_Word mr0, seL4_Word mr1,
                                seL4_Word mr2, seL4_Word mr3)
{
    register seL4_Word destptr asm("x0") = dest;
    register seL4_Word info asm("x1") = info_arg;

    /* Load beginning of the message into registers. */
    register seL4_Word msg0 asm("x2") = mr0;
    register seL4_Word msg1 asm("x3") = mr1;
    register seL4_Word msg2 asm("x4") = mr2;
    register seL4_Word msg3 asm("x5") = mr3;

    /* Perform the system call. */
    register seL4_Word scno asm("x7") = sys;
    asm volatile(
        "svc #0"
        : "+r"(destptr), "+r"(msg0), "+r"(msg1), "+r"(msg2),
        "+r"(msg3), "+r"(info)
        : "r"(scno)
    );
}

#ifndef CONFIG_KERNEL_MCS
static inline void arm_sys_reply(seL4_Word sys, seL4_Word info_arg, seL4_Word mr0, seL4_Word mr1, seL4_Word mr2,
                                 seL4_Word mr3)
{
    register seL4_Word info asm("x1") = info_arg;

    /* Load beginning of the message into registers. */
    register seL4_Word msg0 asm("x2") = mr0;
    register seL4_Word msg1 asm("x3") = mr1;
    register seL4_Word msg2 asm("x4") = mr2;
    register seL4_Word msg3 asm("x5") = mr3;

    /* Perform the system call. */
    register seL4_Word scno asm("x7") = sys;
    asm volatile(
        "svc #0"
        : "+r"(msg0), "+r"(msg1), "+r"(msg2), "+r"(msg3),
        "+r"(info)
        : "r"(scno)
    );
}
#endif

static inline void arm_sys_send_null(seL4_Word sys, seL4_Word src, seL4_Word info_arg)
{
    register seL4_Word destptr asm("x0") = src;
    register seL4_Word info asm("x1") = info_arg;

    /* Perform the system call. */
    register seL4_Word scno asm("x7") = sys;
    asm volatile(
        "svc #0"
        : "+r"(destptr), "+r"(info)
        : "r"(scno)
    );
}

static inline void arm_sys_recv(seL4_Word sys, seL4_Word src, seL4_Word *out_badge, seL4_Word *out_info,
                                seL4_Word *out_mr0, seL4_Word *out_mr1, seL4_Word *out_mr2, seL4_Word *out_mr3, LIBSEL4_UNUSED seL4_Word reply)
{
    register seL4_Word src_and_badge asm("x0") = src;
    register seL4_Word info asm("x1");

    /* Incoming message registers. */
    register seL4_Word msg0 asm("x2");
    register seL4_Word msg1 asm("x3");
    register seL4_Word msg2 asm("x4");
    register seL4_Word msg3 asm("x5");
    MCS_PARAM_DECL("x6");

    /* Perform the system call. */
    register seL4_Word scno asm("x7") = sys;
    asm volatile(
        "svc #0"
        : "=r"(msg0), "=r"(msg1), "=r"(msg2), "=r"(msg3),
        "=r"(info), "+r"(src_and_badge)
        : "r"(scno) MCS_PARAM
        : "memory"
    );
    *out_badge = src_and_badge;
    *out_info = info;
    *out_mr0 = msg0;
    *out_mr1 = msg1;
    *out_mr2 = msg2;
    *out_mr3 = msg3;
}

static inline void arm_sys_send_recv(seL4_Word sys, seL4_Word dest, seL4_Word *out_badge, seL4_Word info_arg,
                                     seL4_Word *out_info, seL4_Word *in_out_mr0, seL4_Word *in_out_mr1, seL4_Word *in_out_mr2, seL4_Word *in_out_mr3,
                                     LIBSEL4_UNUSED seL4_Word reply)
{
    register seL4_Word destptr asm("x0") = dest;
    register seL4_Word info asm("x1") = info_arg;

    /* Load beginning of the message into registers. */
    register seL4_Word msg0 asm("x2") = *in_out_mr0;
    register seL4_Word msg1 asm("x3") = *in_out_mr1;
    register seL4_Word msg2 asm("x4") = *in_out_mr2;
    register seL4_Word msg3 asm("x5") = *in_out_mr3;
    MCS_PARAM_DECL("x6");

    /* Perform the system call. */
    register seL4_Word scno asm("x7") = sys;
    asm volatile(
        "svc #0"
        : "+r"(msg0), "+r"(msg1), "+r"(msg2), "+r"(msg3),
        "+r"(info), "+r"(destptr)
        : "r"(scno) MCS_PARAM
        : "memory"
    );
    *out_info = info;
    *out_badge = destptr;
    *in_out_mr0 = msg0;
    *in_out_mr1 = msg1;
    *in_out_mr2 = msg2;
    *in_out_mr3 = msg3;
}

#ifdef CONFIG_KERNEL_MCS
static inline void arm_sys_nbsend_recv(seL4_Word sys, seL4_Word dest, seL4_Word src, seL4_Word *out_badge,
                                       seL4_Word info_arg,
                                       seL4_Word *out_info, seL4_Word *in_out_mr0, seL4_Word *in_out_mr1, seL4_Word *in_out_mr2,
                                       seL4_Word *in_out_mr3, seL4_Word reply)
{
    register seL4_Word src_and_badge asm("x0") = src;
    register seL4_Word info asm("x1") = info_arg;

    /* Load the beginning of the message info registers */
    register seL4_Word msg0 asm("x2") = *in_out_mr0;
    register seL4_Word msg1 asm("x3") = *in_out_mr1;
    register seL4_Word msg2 asm("x4") = *in_out_mr2;
    register seL4_Word msg3 asm("x5") = *in_out_mr3;

    register seL4_Word reply_reg asm("x6") = reply;
    register seL4_Word dest_reg asm("x8") = dest;

    /* Perform the system call. */
    register seL4_Word scno asm("x7") = sys;
    asm volatile(
        "svc #0"
        : "+r"(msg0), "+r"(msg1), "+r"(msg2), "+r"(msg3),
        "+r"(src_and_badge), "+r"(info)
        : "r"(scno), "r"(reply_reg), "r"(dest_reg)
        : "memory"
    );

    *out_badge = src_and_badge;
    *out_info = info;
    *in_out_mr0 = msg0;
    *in_out_mr1 = msg1;
    *in_out_mr2 = msg2;
    *in_out_mr3 = msg3;
}
#endif

static inline void arm_sys_null(seL4_Word sys)
{
    register seL4_Word scno asm("x7") = sys;
    asm volatile(
        "svc #0"
        : /* no outputs */
        : "r"(scno)
    );
}

// temporary constant definitions:
#define seL4_MaxBatchedMap      ((seL4_MsgMaxLength - 3) / 2)
#define seL4_MaxBatchedUnmap    (seL4_MsgMaxLength - 1)

typedef struct seL4_ARM_BatchedMapping_ {
    seL4_CPtr frame;
    seL4_Word vaddr;
} seL4_ARM_BatchedMapping;

LIBSEL4_INLINE_FUNC seL4_Error seL4_ARM_VSpace_Map_Batched(seL4_ARM_VSpace _service,
                                                           seL4_Word count,
                                                           const seL4_ARM_BatchedMapping *mappings,
                                                           seL4_CapRights_t rights,
                                                           seL4_ARM_VMAttributes attr)
{
    if (count > seL4_MaxBatchedMap) {
        return seL4_RangeError;
    }

    seL4_SetMR(0, count);
    seL4_SetMR(1, rights.words[0]);
    seL4_SetMR(2, attr);
    for (seL4_Word i = 0; i < count; i++) {
        seL4_SetMR(3 + 2 * i, mappings[i].frame);
        seL4_SetMR(4 + 2 * i, mappings[i].vaddr);
    }

    seL4_MessageInfo_t tag = seL4_MessageInfo_new(ARMVSpaceMapBatched, 0, 0, 3 + 2 * count);
    seL4_MessageInfo_t output_tag = seL4_Call(_service, tag);

    return (seL4_Error) seL4_MessageInfo_get_label(output_tag);
}

LIBSEL4_INLINE_FUNC seL4_Error seL4_ARM_VSpace_Unmap_Batched(seL4_ARM_VSpace _service,
                                                             seL4_Word count,
                                                             const seL4_CPtr *frames)
{
    if (count > seL4_MaxBatchedUnmap) {
        return seL4_RangeError;
    }

    seL4_SetMR(0, count);
    for (seL4_Word i = 0; i < count; i++) {
        seL4_SetMR(1 + i, frames[i]);
    }

    seL4_MessageInfo_t tag = seL4_MessageInfo_new(ARMVSpaceUnmapBatched, 0, 0, 1 + count);
    seL4_MessageInfo_t output_tag = seL4_Call(_service, tag);

    return (seL4_Error) seL4_MessageInfo_get_label(output_tag);
}
