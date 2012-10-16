/*
 *
 * (C) COPYRIGHT 2011 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 * 
 */



#ifndef _BASE_HWCONFIG_H_
#define _BASE_HWCONFIG_H_

/** Clarifications requested to First Vertex Index.
 *  Note this reference needs to be replaced with a proper issue raised against the HW Beta.
 */
#define BASE_HW_ISSUE_999   0

/** Tiler triggers a fault if the scissor rectangle is empty. */
#define BASE_HW_ISSUE_5699  1

/** Soft-stopped jobs should cause the job slot to stall until the software has cleared the IRQ. */
#define BASE_HW_ISSUE_5713  0

/* The current version of the model doesn't support Soft-Stop */
#define BASE_HW_ISSUE_5736  0

/** Framebuffer output smaller than 6 pixels causes hang. */
#define BASE_HW_ISSUE_5753  0

/* Transaction Elimination doesn't work correctly. */
#define BASE_HW_ISSUE_5907  1

/* Multisample write mask must be set to all 1s. */
#define BASE_HW_ISSUE_5936  0

/* Jobs can get stuck after page fault */
#define BASE_HW_ISSUE_6035 0

/* Hierarchical tiling doesn't work properly. */
#define BASE_HW_ISSUE_6097  0

/* Depth texture read of D24S8 hangs the FPGA */
#define BASE_HW_ISSUE_6156  0

 /* GPU_COMMAND completion is not visible */
#define BASE_HW_ISSUE_6315  0

/* Readback with negative stride doesn't work properly. */
#define BASE_HW_ISSUE_6325  0

/* Using 8xMSAA surfaces produces incorrect output */
#define BASE_HW_ISSUE_6352  0

/* Need way to guarantee that all previously-translated memory accesses are commited */
#define BASE_HW_ISSUE_6367  1

/* Pixel format 95 doesn't work properly (HW writes to memory) */
#define BASE_HW_ISSUE_6405  0

/* On job complete with non-done the cache is not flushed */
#define BASE_HW_ISSUE_6787  1

/* There is no interrupt when a Performance Counters dump is completed */
#define BASE_HW_ISSUE_7115  0

/* Descriptor Cache usage-counter issue */
#define BASE_HW_ISSUE_7347  0

/* Writing to averaging mode MULTISAMPLE might hang */
#define BASE_HW_ISSUE_7516 0

/* Nested page faults not visible to SW */
#define BASE_HW_ISSUE_7660  1

/* Write of PRFCNT_CONFIG_MODE_MANUAL to PRFCNT_CONFIG causes a instrumentation dump if
   PRFCNT_TILER_EN is enabled */
#define BASE_HW_ISSUE_8186  1

/** Hierz doesn't work when stenciling is enabled */
#define BASE_HW_ISSUE_8260  1

/** uTLB deadlock could occur when writing to an invalid page at the same time as
 * access to a valid page in the same uTLB cache line ( == 4 PTEs == 16K block of mapping) */
#define BASE_HW_ISSUE_8316  1

/* Livelock in L0 icache */
#define BASE_HW_ISSUE_8280  1

/* TIB: Reports faults from a vtile which has not yet been allocated */
#define BASE_HW_ISSUE_8245  1

/* Repeatedly Soft-stopping a job chain consisting of (Vertex Shader, Cache Flush, Tiler)
 * jobs causes 0x58 error on tiler job. */
#define BASE_HW_ISSUE_8408 1

/** Tiler heap issue using FBOs or multiple processes using the tiler simultaneously
 */
#define BASE_HW_ISSUE_8564 1

/* Jobs with relaxed dependencies are not supporting soft-stop */
#define BASE_HW_ISSUE_8803 1

#endif /* _BASE_HWCONFIG_H_ */
