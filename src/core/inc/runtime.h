////////////////////////////////////////////////////////////////////////////////
//
// The University of Illinois/NCSA
// Open Source License (NCSA)
//
// Copyright (c) 2014-2015, Advanced Micro Devices, Inc. All rights reserved.
//
// Developed by:
//
//                 AMD Research and AMD HSA Software Development
//
//                 Advanced Micro Devices, Inc.
//
//                 www.amd.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal with the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
//  - Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimers.
//  - Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimers in
//    the documentation and/or other materials provided with the distribution.
//  - Neither the names of Advanced Micro Devices, Inc,
//    nor the names of its contributors may be used to endorse or promote
//    products derived from this Software without specific prior written
//    permission.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS WITH THE SOFTWARE.
//
////////////////////////////////////////////////////////////////////////////////

// HSA runtime C++ interface file.

#ifndef HSA_RUNTME_CORE_INC_RUNTIME_H_
#define HSA_RUNTME_CORE_INC_RUNTIME_H_

#include <vector>
#include <map>

#include "core/inc/hsa_ext_interface.h"
#include "core/inc/hsa_internal.h"

#include "core/inc/agent.h"
#include "core/inc/memory_region.h"
#include "core/inc/signal.h"
#include "core/util/utils.h"
#include "core/util/locks.h"
#include "core/util/os.h"

#include "core/inc/amd_loader_context.hpp"
#include "amd_hsa_code.hpp"

//---------------------------------------------------------------------------//
//    Constants                                                              //
//---------------------------------------------------------------------------//

#define HSA_ARGUMENT_ALIGN_BYTES 16
#define HSA_QUEUE_ALIGN_BYTES 64
#define HSA_PACKET_ALIGN_BYTES 64

namespace core {
extern bool g_use_interrupt_wait;

/// @brief  Runtime class provides the following functions:
/// - open and close connection to kernel driver.
/// - load supported extension library (image and finalizer).
/// - load tools library.
/// - expose supported agents.
/// - allocate and free memory.
/// - memory copy and fill.
/// - grant access to memory (dgpu memory pool extension).
/// - maintain loader state.
/// - monitor asynchronous event from agent.
class Runtime {
 public:
  /// @brief Open connection to kernel driver and increment reference count.
  /// @retval True if the connection to kernel driver is successfully opened.
  static bool Acquire();

  /// @brief Checks if connection to kernel driver is opened.
  /// @retval True if the connection to kernel driver is opened.
  static bool IsOpen();

  /// @brief Singleton object of the runtime.
  static Runtime* runtime_singleton_;

  /// @brief Decrement reference count and close connection to kernel driver.
  /// @retval True if reference count is larger than 0.
  bool Release();

  /// @brief Insert agent into agent list ::agents_.
  /// @param [in] agent Pointer to the agent object.
  void RegisterAgent(Agent* agent);

  /// @brief Delete all agent objects from ::agents_.
  void DestroyAgents();

  /// @brief Insert memory region into memory region list ::regions_.
  /// @param [in] region Pointer to region object.
  void RegisterMemoryRegion(MemoryRegion* region);

  /// @brief Delete all region objects in ::regions_.
  void DestroyMemoryRegions();

  /// @brief Call the user provided call back for each agent in the agent list.
  ///
  /// @param [in] callback User provided callback function.
  /// @param [in] data User provided pointer as input for @p callback.
  ///
  /// @retval ::HSA_STATUS_SUCCESS if the callback function for each traversed
  /// agent returns ::HSA_STATUS_SUCCESS.
  hsa_status_t IterateAgent(hsa_status_t (*callback)(hsa_agent_t agent,
                                                     void* data),
                            void* data);

  /// @brief Allocate memory on a particular region.
  ///
  /// @param [in] region Pointer to region object.
  /// @param [in] size Allocation size in bytes.
  /// @param [out] address Pointer to store the allocation result.
  ///
  /// @retval ::HSA_STATUS_SUCCESS If allocation is successful.
  hsa_status_t AllocateMemory(const MemoryRegion* region, size_t size,
                              void** address);

  /// @brief Allocate memory on a particular region with option to restrict
  /// access to the owning agent.
  ///
  /// @param [in] restrict_access If true, the allocation result would only be
  /// accessible to the agent(s) that own the region object.
  /// @param [in] region Pointer to region object.
  /// @param [in] size Allocation size in bytes.
  /// @param [out] address Pointer to store the allocation result.
  ///
  /// @retval ::HSA_STATUS_SUCCESS If allocation is successful.
  hsa_status_t AllocateMemory(bool restrict_access, const MemoryRegion* region,
                              size_t size, void** address);

  /// @brief Free memory previously allocated with AllocateMemory.
  ///
  /// @param [in] ptr Address of the memory to be freed.
  ///
  /// @retval ::HSA_STATUS_ERROR If @p ptr is not the address of previous
  /// allocation via ::core::Runtime::AllocateMemory
  /// @retval ::HSA_STATUS_SUCCESS if @p ptr is successfully released.
  hsa_status_t FreeMemory(void* ptr);

  /// @brief Blocking memory copy from src to dst.
  ///
  /// @param [in] dst Memory address of the destination.
  /// @param [in] src Memory address of the source.
  /// @param [in] size Copy size in bytes.
  ///
  /// @retval ::HSA_STATUS_SUCCESS if memory copy is successful and completed.
  hsa_status_t CopyMemory(void* dst, const void* src, size_t size);

  /// @brief Non-blocking memory copy from src to dst.
  ///
  /// @details The memory copy will be performed after all signals in
  /// @p dep_signals have value of 0. On completion @p completion_signal
  /// will be decremented.
  ///
  /// @param [in] dst Memory address of the destination.
  /// @param [in] dst_agent Agent object associated with the destination. This
  /// agent should be able to access the destination and source.
  /// @param [in] src Memory address of the source.
  /// @param [in] src_agent Agent object associated with the source. This
  /// agent should be able to access the destination and source.
  /// @param [in] size Copy size in bytes.
  /// @param [in] dep_signals Array of signal dependency.
  /// @param [in] completion_signal Completion signal object.
  ///
  /// @retval ::HSA_STATUS_SUCCESS if copy command has been submitted
  /// successfully to the agent DMA queue.
  hsa_status_t CopyMemory(void* dst, core::Agent& dst_agent, const void* src,
                          core::Agent& src_agent, size_t size,
                          std::vector<core::Signal*>& dep_signals,
                          core::Signal& completion_signal);

  /// @brief Fill the first @p count of uint32_t in ptr with value.
  ///
  /// @param [in] ptr Memory address to be filled.
  /// @param [in] value The value/pattern that will be used to see @p ptr.
  /// @param [in] count Number of uint32_t element to be set to the value.
  ///
  /// @retval ::HSA_STATUS_SUCCESS if memory fill is successful and completed.
  hsa_status_t FillMemory(void* ptr, uint32_t value, size_t count);

  /// @brief Set agents as the whitelist to access ptr.
  ///
  /// @param [in] num_agents The number of agent handles in @p agents array.
  /// @param [in] agents Agent handle array.
  /// @param [in] ptr Pointer of memory previously allocated via
  /// core::Runtime::AllocateMemory.
  ///
  /// @retval ::HSA_STATUS_SUCCESS The whitelist has been configured
  /// successfully and all agents in the @p agents could start accessing @p ptr.
  hsa_status_t AllowAccess(uint32_t num_agents, const hsa_agent_t* agents,
                           const void* ptr);

  /// @brief Query system information.
  ///
  /// @param [in] attribute System info attribute to query.
  /// @param [out] value Pointer to store the attribute value.
  ///
  /// @retval HSA_STATUS_SUCCESS The attribute is valid and the @p value is
  /// set.
  hsa_status_t GetSystemInfo(hsa_system_info_t attribute, void* value);

  /// @brief Query next available queue id.
  ///
  /// @retval Next available queue id.
  uint32_t GetQueueId();

  /// @brief Register a callback function @p handler that is associated with
  /// @p signal to asynchronous event monitor thread.
  ///
  /// @param [in] signal Signal handle associated with @p handler.
  /// @param [in] cond The condition to execute the @p handler.
  /// @param [in] value The value to compare with @p signal value. If the
  /// comparison satisfy @p cond, the @p handler will be called.
  /// @param [in] arg Pointer to the argument that will be provided to @p
  /// handler.
  ///
  /// @retval ::HSA_STATUS_SUCCESS Registration is successful.
  hsa_status_t SetAsyncSignalHandler(hsa_signal_t signal,
                                     hsa_signal_condition_t cond,
                                     hsa_signal_value_t value,
                                     hsa_amd_signal_handler handler, void* arg);

  const std::vector<Agent*>& agents() { return agents_; }

  const std::vector<uint32_t>& gpu_ids() { return gpu_ids_; }

  Agent* blit_agent() { return blit_agent_; }

  Agent* host_agent() { return host_agent_; }

  hsa_region_t system_region() { return system_region_; }

  hsa_region_t system_region_coarse() { return system_region_coarse_; }

  amd::hsa::loader::Loader* loader() { return loader_; }

  amd::LoaderContext* loader_context() { return &loader_context_; }

  amd::hsa::code::AmdHsaCodeManager* code_manager() { return &code_manager_; }

  std::function<void*(size_t, size_t)>& system_allocator() {
    return system_allocator_;
  }

  std::function<void(void*)>& system_deallocator() {
    return system_deallocator_;
  }

  ExtensionEntryPoints extensions_;

 protected:
  static void AsyncEventsLoop(void*);

  struct AllocationRegion {
    AllocationRegion() : region(NULL), assigned_agent_(NULL), size(0) {}
    AllocationRegion(const MemoryRegion* region_arg, size_t size_arg)
        : region(region_arg), assigned_agent_(NULL), size(size_arg) {}

    const MemoryRegion* region;
    const Agent* assigned_agent_;
    size_t size;
  };

  struct AsyncEventsControl {
    AsyncEventsControl() : async_events_thread_(NULL) {}
    void Shutdown();

    hsa_signal_t wake;
    os::Thread async_events_thread_;
    KernelMutex lock;
    bool exit;
  };

  struct AsyncEvents {
    void PushBack(hsa_signal_t signal, hsa_signal_condition_t cond,
                  hsa_signal_value_t value, hsa_amd_signal_handler handler,
                  void* arg);

    void CopyIndex(size_t dst, size_t src);

    size_t Size();

    void PopBack();

    void Clear();

    std::vector<hsa_signal_t> signal_;
    std::vector<hsa_signal_condition_t> cond_;
    std::vector<hsa_signal_value_t> value_;
    std::vector<hsa_amd_signal_handler> handler_;
    std::vector<void*> arg_;
  };

  // Will be created before any user could call hsa_init but also could be
  // destroyed before incorrectly written programs call hsa_shutdown.
  static KernelMutex bootstrap_lock_;

  Runtime();

  Runtime(const Runtime&);

  Runtime& operator=(const Runtime&);

  ~Runtime() {}

  /// @brief Open connection to kernel driver.
  void Load();

  /// @brief Close connection to kernel driver and cleanup resources.
  void Unload();

  /// @brief Dynamically load extension libraries (images, finalizer) and
  /// call OnLoad method on each loaded library.
  void LoadExtensions();

  /// @brief Call OnUnload method on each extension library then close it.
  void UnloadExtensions();

  /// @brief Dynamically load tool libraries and call OnUnload method on each
  /// loaded library.
  void LoadTools();

  /// @brief Call OnUnload method of each tool library.
  void UnloadTools();

  /// @brief Close tool libraries.
  void CloseTools();

  /// @brief Blocking memory copy from src to dst. One of the src or dst
  /// is user pointer. A particular setup need to be made if the DMA queue
  /// for the memory copy belongs to a dGPU agent. E.g: pin the user pointer
  /// before copying, or using a staging buffer.
  ///
  /// @param [in] dst Memory address of the destination.
  /// @param [in] src Memory address of the source.
  /// @param [in] size Copy size in bytes.
  /// @param [in] dst_malloc If true, then @p dst is the user pointer. Otherwise
  /// @p src is the user pointer.
  ///
  /// @retval ::HSA_STATUS_SUCCESS if memory copy is successful and completed.
  hsa_status_t CopyMemoryHostAlloc(void* dst, const void* src, size_t size,
                                   bool dst_malloc);

  // Mutex object to protect multithreaded access to ::Acquire and ::Release.
  KernelMutex kernel_lock_;

  // Mutex object to protect multithreaded access to ::allocation_map_.
  KernelMutex memory_lock_;

  // Array containing tools library handles.
  std::vector<os::LibHandle> tool_libs_;

  // Agent list containing all compatible agents in the platform.
  std::vector<Agent*> agents_;

  // Agent list containing all compatible gpu agent ids in the platform.
  std::vector<uint32_t> gpu_ids_;

  // Region list containing all physical memory region in the platform.
  std::vector<MemoryRegion*> regions_;

  // Shared fine grain system memory region
  hsa_region_t system_region_;

  // Shared coarse grain system memory region
  hsa_region_t system_region_coarse_;

  // Loader instance.
  amd::hsa::loader::Loader* loader_;

  // Loader context.
  amd::LoaderContext loader_context_;

  // Code object manager.
  amd::hsa::code::AmdHsaCodeManager code_manager_;

  // Contains the region, address, and size of previously allocated memory.
  std::map<const void*, AllocationRegion> allocation_map_;

  // Allocator using ::system_region_
  std::function<void*(size_t, size_t)> system_allocator_;

  // Deallocator using ::system_region_
  std::function<void(void*)> system_deallocator_;

  // Pointer to a host/cpu agent object.
  Agent* host_agent_;

  // Pointer to DMA agent.
  Agent* blit_agent_;

  AsyncEventsControl async_events_control_;

  AsyncEvents async_events_;

  AsyncEvents new_async_events_;

  // Queue id counter.
  uint32_t queue_count_;

  // Starting address of SVM address space.
  // On APU the cpu and gpu could access the area inside starting and end of
  // the SVM address space.
  // On dGPU, only the gpu is guaranteed to have access to the area inside the
  // SVM address space, since it maybe backed by private gpu VRAM.
  uintptr_t start_svm_address_;

  // End address of SVM address space.
  // start_svm_address_ + size
  uintptr_t end_svm_address_;

  // System clock frequency.
  uint64_t sys_clock_freq_;

  // Holds reference count to runtime object.
  volatile uint32_t ref_count_;

  // Frees runtime memory when the runtime library is unloaded if safe to do so.
  // Failure to release the runtime indicates an incorrect application but is
  // common (example: calls library routines at process exit).
  friend class RuntimeCleanup;
};

}  // namespace core
#endif  // header guard
