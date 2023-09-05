/*
 *
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "gtest/gtest.h"

#include "utils/utils.hpp"
#include "test_harness/test_harness.hpp"
#include "logging/logging.hpp"

#include <level_zero/ze_api.h>
#include <thread>

namespace {

class zeDeviceCreateEventPoolTests : public lzt::zeEventPoolTests {};

TEST_F(
    zeDeviceCreateEventPoolTests,
    GivenDefaultDeviceWhenCreatingEventPoolWithDefaultFlagsThenNotNullEventPoolIsReturned) {
  ep.InitEventPool();
}

TEST_F(
    zeDeviceCreateEventPoolTests,
    GivenDefaultDeviceWhenCreatingEventPoolWithHostVisibleFlagsThenNotNullEventPoolIsReturned) {
  ep.InitEventPool(32, ZE_EVENT_POOL_FLAG_HOST_VISIBLE);
}

TEST_F(
    zeDeviceCreateEventPoolTests,
    GivenDefaultDeviceWhenCreatingEventPoolWithIPCFlagsThenNotNullEventPoolIsReturned) {
  ep.InitEventPool(32, ZE_EVENT_POOL_FLAG_IPC);
}

class zeDeviceCreateEventPermuteEventsTests
    : public lzt::zeEventPoolTests,
      public ::testing::WithParamInterface<
          std::tuple<ze_event_scope_flag_t, ze_event_scope_flag_t>> {};

TEST_P(
    zeDeviceCreateEventPermuteEventsTests,
    GivenDefaultDeviceAndEventPoolWhenCreatingEventsWithSignalAndWaitEventsThenNotNullEventIsReturned) {

  ze_event_handle_t event = nullptr;

  ep.create_event(event, std::get<0>(GetParam()), std::get<1>(GetParam()));
  ep.destroy_event(event);
}

INSTANTIATE_TEST_SUITE_P(
    ImplictEventCreateParameterizedTest, zeDeviceCreateEventPermuteEventsTests,
    ::testing::Combine(::testing::Values(0, ZE_EVENT_SCOPE_FLAG_SUBDEVICE,
                                         ZE_EVENT_SCOPE_FLAG_DEVICE,
                                         ZE_EVENT_SCOPE_FLAG_HOST),
                       ::testing::Values(0, ZE_EVENT_SCOPE_FLAG_SUBDEVICE,
                                         ZE_EVENT_SCOPE_FLAG_DEVICE,
                                         ZE_EVENT_SCOPE_FLAG_HOST)));

class zeEventSignalScopeTests
    : public lzt::zeEventPoolTests,
      public ::testing::WithParamInterface<ze_event_scope_flag_t> {
public:
  void RunGivenDifferentEventSignalScopeFlagsThenAppendSignalEventTest(
      bool is_immediate) {
    const ze_device_handle_t device =
        lzt::zeDevice::get_instance()->get_device();
    size_t num_event = 3;
    ze_context_handle_t context = lzt::create_context();
    ep.InitEventPool(context, 10, ZE_EVENT_POOL_FLAG_HOST_VISIBLE);
    std::vector<ze_event_handle_t> events(num_event, nullptr);
    auto cmd_bundle = lzt::create_command_bundle(
        context, device, 0, ZE_COMMAND_QUEUE_MODE_DEFAULT,
        ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, 0, 0, is_immediate);
    ze_event_scope_flag_t signalScope = GetParam();
    ep.create_events(events, num_event, signalScope, 0);

    for (uint32_t i = 0; i < num_event; i++) {
      lzt::append_signal_event(cmd_bundle.list, events[i]);
    }
    lzt::append_barrier(cmd_bundle.list, nullptr, 0, nullptr);
    lzt::close_command_list(cmd_bundle.list);
    lzt::execute_and_sync_command_bundle(cmd_bundle, UINT64_MAX);

    for (uint32_t i = 0; i < num_event; i++) {
      EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventQueryStatus(events[i]));
    }
    lzt::destroy_command_bundle(cmd_bundle);
    ep.destroy_events(events);
    lzt::destroy_context(context);
  }
};

TEST_P(zeEventSignalScopeTests,
       GivenDifferentEventSignalScopeFlagsThenAppendSignalEventIsSuccessful) {
  RunGivenDifferentEventSignalScopeFlagsThenAppendSignalEventTest(false);
}

TEST_P(
    zeEventSignalScopeTests,
    GivenDifferentEventSignalScopeFlagsThenAppendSignalEventToImmediateCmdListIsSuccessful) {
  RunGivenDifferentEventSignalScopeFlagsThenAppendSignalEventTest(true);
}

INSTANTIATE_TEST_SUITE_P(zeEventSignalScopeParameterizedTest,
                         zeEventSignalScopeTests,
                         ::testing::Values(0, ZE_EVENT_SCOPE_FLAG_SUBDEVICE,
                                           ZE_EVENT_SCOPE_FLAG_DEVICE,
                                           ZE_EVENT_SCOPE_FLAG_HOST));

TEST_F(zeDeviceCreateEventPoolTests,
       GivenDefaultDeviceWhenGettingIpcEventHandleThenNotNullisReturned) {
  ze_ipc_event_pool_handle_t hIpc;
  ep.InitEventPool(32, ZE_EVENT_POOL_FLAG_IPC);

  ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventPoolGetIpcHandle(ep.event_pool_, &hIpc));
}

class zeSubDeviceCreateEventPoolTests : public lzt::zeEventPoolTests {};

TEST_F(zeSubDeviceCreateEventPoolTests,
       GivenSubDeviceWhenGettingIpcEventHandleThenNotNullReturned) {
  ze_ipc_event_pool_handle_t hIpc;
  auto devices = lzt::get_ze_devices();
  ze_event_pool_desc_t ep_desc = {};
  ep_desc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;
  ep_desc.flags = ZE_EVENT_POOL_FLAG_IPC;
  ep_desc.count = 10;
  std::vector<ze_device_handle_t> devs;
  for (auto device : devices) {
    auto sub_devices = lzt::get_ze_sub_devices(device);
    if (sub_devices.size() < 2) {
      continue;
    }
    for (auto subdev : sub_devices) {
      devs.push_back(subdev);
      ep.InitEventPool(ep_desc, devs);
      ASSERT_EQ(ZE_RESULT_SUCCESS,
                zeEventPoolGetIpcHandle(ep.event_pool_, &hIpc));
      devs.clear();
    }
  }
}

class zeDeviceCreateEventAndCommandListTests : public ::testing::Test {};

TEST_F(
    zeDeviceCreateEventAndCommandListTests,
    GivenDefaultDeviceWhenAppendingSignalEventToComandListThenSuccessIsReturned) {
  lzt::zeEventPool ep;
  const ze_device_handle_t device = lzt::zeDevice::get_instance()->get_device();
  ze_context_handle_t context = lzt::create_context();
  ep.InitEventPool(context, 1);
  ze_command_list_handle_t cmd_list =
      lzt::create_command_list(context, device, 0);
  ze_event_handle_t event = nullptr;

  ep.create_event(event);
  EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendSignalEvent(cmd_list, event));
  ep.destroy_event(event);
  lzt::destroy_command_list(cmd_list);
  lzt::destroy_context(context);
}

TEST_F(
    zeDeviceCreateEventAndCommandListTests,
    GivenDefaultDeviceWhenAppendingWaitEventsToComandListThenSuccessIsReturned) {
  const size_t event_count = 2;
  lzt::zeEventPool ep;
  const ze_device_handle_t device = lzt::zeDevice::get_instance()->get_device();
  ze_context_handle_t context = lzt::create_context();
  ep.InitEventPool(context, 2);
  ze_command_list_handle_t cmd_list =
      lzt::create_command_list(context, device, 0);
  std::vector<ze_event_handle_t> events(event_count, nullptr);

  ep.create_events(events, event_count, 0, 0);
  auto events_initial = events;
  EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendWaitOnEvents(
                                   cmd_list, event_count, events.data()));
  for (int i = 0; i < events.size(); i++) {
    ASSERT_EQ(events[i], events_initial[i]);
  }
  ep.destroy_events(events);
  lzt::destroy_command_list(cmd_list);
  lzt::destroy_context(context);
}

class zeSubDeviceCreateEventAndCommandListTests
    : public lzt::zeEventPoolTests,
      public ::testing::WithParamInterface<
          std::tuple<ze_command_queue_mode_t, bool>> {
protected:
  void SetUp() override {
    ep_desc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;
    ep_desc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    ep_desc.count = num_events;
    mode = std::get<0>(GetParam());
    use_immediate = std::get<1>(GetParam());

    auto devices = lzt::get_ze_devices();
    for (auto device : devices) {
      auto sub_devices = lzt::get_ze_sub_devices(device);
      if (sub_devices.size() < 2) {
        continue;
      }
      for (auto subdev : sub_devices) {
        dev_handles.push_back(subdev);
      }
    }

    context = lzt::create_context();
    for (auto idx = 0; idx < dev_handles.size(); idx++) {
      cmdlist_immediate_handles.resize(dev_handles.size());
      cmd_lists.resize(dev_handles.size());
      cmd_qs.resize(dev_handles.size());
      if (use_immediate) {
        cmdlist_immediate_handles[idx] = lzt::create_immediate_command_list(
            dev_handles[idx], 0, mode, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0);
      } else {
        cmd_lists[idx] = lzt::create_command_list(context, dev_handles[idx], 0);
        cmd_qs[idx] =
            lzt::create_command_queue(context, dev_handles[idx], 0, mode,
                                      ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0);
      }
    }

    if (mode == ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS) {
      timeout = 0;
    } else {
      timeout = UINT64_MAX - 1;
    }
  }

  void TearDown() override {
    auto num_handles = cmdlist_immediate_handles.size();
    for (auto i = 0; i < num_handles; i++) {
      if (use_immediate) {
        lzt::destroy_command_list(cmdlist_immediate_handles[i]);
      } else {
        lzt::destroy_command_list(cmd_lists[i]);
        lzt::destroy_command_queue(cmd_qs[i]);
      }
    }
    lzt::destroy_context(context);
  }
  ze_command_queue_mode_t mode;
  uint64_t timeout = UINT64_MAX - 1;
  uint64_t num_events = 10;
  ze_event_pool_desc_t ep_desc = {};
  bool use_immediate = false;
  std::vector<ze_command_list_handle_t> cmdlist_immediate_handles;
  std::vector<ze_device_handle_t> dev_handles;
  std::vector<ze_command_list_handle_t> cmd_lists;
  std::vector<ze_command_queue_handle_t> cmd_qs;
  ze_context_handle_t context;
};

TEST_P(
    zeSubDeviceCreateEventAndCommandListTests,
    GivenSubDeviceWhenAppendingSignalEventToCommandListThenSuccessIsReturned) {
  ze_event_handle_t event = nullptr;

  ep.InitEventPool(ep_desc, dev_handles);
  for (auto i = 0; i < dev_handles.size(); i++) {
    ep.create_event(event);
    if (use_immediate) {
      EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendSignalEvent(
                                       cmdlist_immediate_handles[i], event));
    } else {
      EXPECT_EQ(ZE_RESULT_SUCCESS,
                zeCommandListAppendSignalEvent(cmd_lists[i], event));
      lzt::close_command_list(cmd_lists[i]);
      lzt::execute_command_lists(cmd_qs[i], 1, &cmd_lists[i], nullptr);
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventHostSynchronize(event, timeout));
    ep.destroy_event(event);
  }
}

TEST_P(
    zeSubDeviceCreateEventAndCommandListTests,
    GivenSubDeviceWhenChainingSignalEventToCommandListThenSuccessIsReturned) {
  std::vector<ze_event_handle_t> events(num_events);
  for (auto i = 0; i < num_events; i++) {
    ep.create_event(events[i]);
  }

  auto num_iters =
      (num_events > dev_handles.size()) ? dev_handles.size() : num_events;
  if (num_iters < 1) {
    GTEST_SKIP() << "No subdevices found, skipping test";
  }
  uint32_t i = 0;
  for (i = 0; i < num_iters - 1; i++) {
    if (use_immediate) {
      EXPECT_EQ(ZE_RESULT_SUCCESS,
                zeCommandListAppendSignalEvent(cmdlist_immediate_handles[i],
                                               events[i]));
      EXPECT_EQ(ZE_RESULT_SUCCESS,
                zeCommandListAppendWaitOnEvents(
                    cmdlist_immediate_handles[i + 1], 1, &events[i]));
    } else {
      EXPECT_EQ(ZE_RESULT_SUCCESS,
                zeCommandListAppendSignalEvent(cmd_lists[i], events[i]));
      EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendWaitOnEvents(
                                       cmd_lists[i + 1], 1, &events[i]));
    }
  }

  // For regular command lists, execute as batch
  if (!use_immediate) {
    for (auto idx = 0; idx < num_iters; idx++) {
      lzt::close_command_list(cmd_lists[idx]);
      lzt::execute_command_lists(cmd_qs[idx], 1, &cmd_lists[idx], nullptr);
      lzt::synchronize(cmd_qs[idx], timeout);
    }
  } else {
    // Signal last event in queue and wait on host
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendSignalEvent(
                                     cmdlist_immediate_handles[i], events[i]));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventHostSynchronize(events[i], timeout));
  }

  for (auto i = 0; i < num_events; i++) {
    ep.destroy_event(events[i]);
  }
}

TEST_P(zeSubDeviceCreateEventAndCommandListTests,
       GivenSubDeviceAndEventPoolWhenAppendingEventResetThenSuccessIsReturned) {
  ze_event_handle_t event = nullptr;
  ze_event_handle_t event_imm = nullptr;

  ep.InitEventPool(ep_desc, dev_handles);
  for (auto i = 0; i < dev_handles.size(); i++) {
    ep.create_event(event);
    ep.create_event(event_imm);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventHostSignal(event));
    if (use_immediate) {
      EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendEventReset(
                                       cmdlist_immediate_handles[i], event));
      EXPECT_EQ(ZE_RESULT_SUCCESS,
                zeCommandListAppendSignalEvent(cmdlist_immediate_handles[i],
                                               event_imm));
      EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventHostSynchronize(event_imm, timeout));
      EXPECT_EQ(ZE_RESULT_NOT_READY, zeEventQueryStatus(event));
    } else {
      EXPECT_EQ(ZE_RESULT_SUCCESS,
                zeCommandListAppendEventReset(cmd_lists[i], event));
      lzt::close_command_list(cmd_lists[i]);
      lzt::execute_command_lists(cmd_qs[i], 1, &cmd_lists[i], nullptr);
      lzt::synchronize(cmd_qs[i], UINT64_MAX);
      EXPECT_EQ(ZE_RESULT_NOT_READY, zeEventQueryStatus(event));
    }
    ep.destroy_event(event);
    ep.destroy_event(event_imm);
  }
}

INSTANTIATE_TEST_SUITE_P(
    TestCasesForSubDeviceEvents, zeSubDeviceCreateEventAndCommandListTests,
    testing::Combine(testing::Values(ZE_COMMAND_QUEUE_MODE_DEFAULT,
                                     ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS,
                                     ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS),
                     testing::Values(true, false)));

class zeDeviceCreateEventTests : public lzt::zeEventPoolTests {};

TEST_F(
    zeDeviceCreateEventTests,
    GivenDefaultDeviceAndEventPoolWhenSignalingHostEventThenSuccessIsReturned) {
  ze_event_handle_t event = nullptr;

  ep.create_event(event);
  EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventHostSignal(event));
  ep.destroy_event(event);
}

class zeHostEventSyncPermuteTimeoutTests
    : public lzt::zeEventPoolTests,
      public ::testing::WithParamInterface<uint32_t> {};

void child_thread_function(ze_event_handle_t event, uint32_t timeout) {
  int retries = 10000;
  ze_result_t result = ZE_RESULT_NOT_READY;
  if (timeout == 0) {
    while (retries > 0) {
      result = zeEventHostSynchronize(event, timeout);
      if (result != ZE_RESULT_NOT_READY) {
        break;
      }
      retries--;
      std::this_thread::yield();
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
  } else {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventHostSynchronize(event, timeout));
  }
}

TEST_P(zeHostEventSyncPermuteTimeoutTests,
       GivenDefaultDeviceAndEventPoolWhenSyncingEventThenSuccessIsReturned) {
  ze_event_handle_t event = nullptr;

  ep.create_event(event, ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST);

  std::thread child_thread(child_thread_function, event, GetParam());
  EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventHostSignal(event));
  child_thread.join();
  ep.destroy_event(event);
}

INSTANTIATE_TEST_SUITE_P(ImplictHostSynchronizeEventParameterizedTest,
                         zeHostEventSyncPermuteTimeoutTests,
                         ::testing::Values(0, 10000000, UINT64_MAX));

TEST_F(
    zeDeviceCreateEventTests,
    GivenDefaultDeviceAndEventPoolWhenCreatingAnEventAndQueryingItsStatusThenNotReadyIsReturned) {
  ze_event_handle_t event = nullptr;

  ep.create_event(event);
  EXPECT_EQ(ZE_RESULT_NOT_READY, zeEventQueryStatus(event));
  ep.destroy_event(event);
}

TEST_F(
    zeDeviceCreateEventAndCommandListTests,
    GivenDefaultDeviceAndEventPoolWhenAppendingEventResetThenSuccessIsReturned) {
  lzt::zeEventPool ep;
  const ze_device_handle_t device = lzt::zeDevice::get_instance()->get_device();
  ze_context_handle_t context = lzt::create_context();
  ep.InitEventPool(context, 1);
  ze_command_list_handle_t cmd_list =
      lzt::create_command_list(context, device, 0);
  ze_event_handle_t event = nullptr;

  ep.create_event(event);
  EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendEventReset(cmd_list, event));
  ep.destroy_event(event);
  lzt::destroy_command_list(cmd_list);
  lzt::destroy_context(context);
}

TEST_F(zeDeviceCreateEventTests,
       GivenDefaultDeviceAndEventPoolWhenResettingEventThenSuccessIsReturned) {
  ze_event_handle_t event = nullptr;

  ep.create_event(event);
  EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventHostReset(event));
  ep.destroy_event(event);
}

class zeEventSignalingTests : public lzt::zeEventPoolTests {};

TEST_F(
    zeEventSignalingTests,
    GivenOneEventSignaledbyHostWhenQueryStatusThenVerifyOnlyOneEventDetected) {
  size_t num_event = 10;
  std::vector<ze_event_handle_t> host_event(num_event, nullptr);

  ep.InitEventPool(num_event);
  ep.create_events(host_event, num_event, 0, 0);
  for (uint32_t i = 0; i < num_event; i++) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventHostSignal(host_event[i]));
    for (uint32_t j = 0; j < num_event; j++) {
      if (j == i) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventQueryStatus(host_event[j]));
      } else {
        EXPECT_EQ(ZE_RESULT_NOT_READY, zeEventQueryStatus(host_event[j]));
      }
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventHostReset(host_event[i]));
  }
  ep.destroy_events(host_event);
}

void RunGivenOneEventSignaledbyCommandListWhenQueryStatusOnHostTest(
    zeEventSignalingTests &test, bool is_immediate) {
  const ze_device_handle_t device = lzt::zeDevice::get_instance()->get_device();
  size_t num_event = 10;
  ASSERT_GE(num_event, 3);
  ze_context_handle_t context = lzt::create_context();
  test.ep.InitEventPool(context, 10);
  std::vector<ze_event_handle_t> device_event(num_event, nullptr);
  auto cmd_bundle = lzt::create_command_bundle(
      context, device, 0, ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS,
      ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, 0, 0, is_immediate);
  size_t copy_size = 4096;
  void *src_buff = lzt::allocate_host_memory(copy_size, 1, context);
  void *dst_buff =
      lzt::allocate_shared_memory(copy_size, 1, 0, 0, device, context);
  test.ep.create_events(device_event, num_event, ZE_EVENT_SCOPE_FLAG_HOST, 0);

  lzt::write_data_pattern(src_buff, copy_size, 1);
  lzt::write_data_pattern(dst_buff, copy_size, 0);

  for (uint32_t i = 0; i < num_event - 1; i++) {
    lzt::append_signal_event(cmd_bundle.list, device_event[i]);
    lzt::append_barrier(cmd_bundle.list, nullptr, 0, nullptr);
    lzt::append_wait_on_events(cmd_bundle.list, 1,
                               &device_event[num_event - 1]);
    lzt::append_memory_copy(cmd_bundle.list, dst_buff, src_buff, copy_size,
                            nullptr);
    lzt::close_command_list(cmd_bundle.list);
    if (!is_immediate) {
      lzt::execute_command_lists(cmd_bundle.queue, 1, &cmd_bundle.list,
                                 nullptr);
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS,
              zeEventHostSynchronize(device_event[i], UINT32_MAX - 1));
    EXPECT_EQ(ZE_RESULT_SUCCESS,
              zeEventHostSignal(device_event[num_event - 1]));
    if (is_immediate) {
      lzt::synchronize_command_list_host(cmd_bundle.list, UINT64_MAX);
    } else {
      lzt::synchronize(cmd_bundle.queue, UINT64_MAX);
    }

    for (uint32_t j = 0; j < num_event - 1; j++) {
      if (j == i) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventQueryStatus(device_event[j]));
      } else {
        EXPECT_EQ(ZE_RESULT_NOT_READY, zeEventQueryStatus(device_event[j]));
      }
    }
    lzt::validate_data_pattern(dst_buff, copy_size, 1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventHostReset(device_event[i]));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventHostReset(device_event[num_event - 1]));
    lzt::reset_command_list(cmd_bundle.list);
  }

  lzt::destroy_command_bundle(cmd_bundle);
  lzt::free_memory(context, src_buff);
  lzt::free_memory(context, dst_buff);
  test.ep.destroy_events(device_event);
  lzt::destroy_context(context);
}

TEST_F(
    zeEventSignalingTests,
    GivenOneEventSignaledbyCommandListWhenQueryStatusOnHostThenVerifyOnlyOneEventDetected) {
  RunGivenOneEventSignaledbyCommandListWhenQueryStatusOnHostTest(*this, false);
}

TEST_F(
    zeEventSignalingTests,
    GivenOneEventSignaledbyImmediateCommandListWhenQueryStatusOnHostThenVerifyOnlyOneEventDetected) {
  RunGivenOneEventSignaledbyCommandListWhenQueryStatusOnHostTest(*this, true);
}

void RunGivenCommandListWaitsForEventsWhenHostAndCommandListSendsSignalsTest(
    zeEventSignalingTests &test, bool is_immediate) {
  const ze_device_handle_t device = lzt::zeDevice::get_instance()->get_device();
  size_t num_event = 10;
  ASSERT_GE(num_event, 3);
  ze_context_handle_t context = lzt::create_context();
  test.ep.InitEventPool(context, 10);
  std::vector<ze_event_handle_t> device_event(num_event, nullptr);
  auto cmd_bundle = lzt::create_command_bundle(
      context, device, 0, ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS,
      ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, 0, 0, is_immediate);
  size_t copy_size = 4096;
  void *src_buff = lzt::allocate_host_memory(copy_size, 1, context);
  void *dev_buff =
      lzt::allocate_device_memory(copy_size, 1, 0, 0, device, context);
  void *dst_buff =
      lzt::allocate_shared_memory(copy_size, 1, 0, 0, device, context);

  test.ep.create_events(device_event, num_event, ZE_EVENT_SCOPE_FLAG_HOST, 0);

  lzt::write_data_pattern(src_buff, copy_size, 1);
  lzt::write_data_pattern(dst_buff, copy_size, 0);

  lzt::append_signal_event(cmd_bundle.list, device_event[0]);
  lzt::append_memory_copy(cmd_bundle.list, dev_buff, src_buff, copy_size,
                          nullptr);
  lzt::append_signal_event(cmd_bundle.list, device_event[1]);
  lzt::append_barrier(cmd_bundle.list, nullptr, 0, nullptr);
  lzt::append_wait_on_events(cmd_bundle.list, num_event, device_event.data());
  lzt::append_memory_copy(cmd_bundle.list, dst_buff, dev_buff, copy_size,
                          nullptr);
  lzt::close_command_list(cmd_bundle.list);
  if (!is_immediate) {
    lzt::execute_command_lists(cmd_bundle.queue, 1, &cmd_bundle.list, nullptr);
  }
  for (uint32_t i = 2; i < num_event; i++) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventHostSignal(device_event[i]));
  }
  if (is_immediate) {
    lzt::synchronize_command_list_host(cmd_bundle.list, UINT64_MAX);
  } else {
    lzt::synchronize(cmd_bundle.queue, UINT64_MAX);
  }

  for (uint32_t i = 0; i < num_event; i++) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventQueryStatus(device_event[i]));
  }

  lzt::validate_data_pattern(dst_buff, copy_size, 1);

  lzt::destroy_command_bundle(cmd_bundle);
  lzt::free_memory(context, src_buff);
  lzt::free_memory(context, dev_buff);
  lzt::free_memory(context, dst_buff);
  test.ep.destroy_events(device_event);
  lzt::destroy_context(context);
}

TEST_F(
    zeEventSignalingTests,
    GivenCommandListWaitsForEventsWhenHostAndCommandListSendsSignalsThenCommandListExecutesSuccessfully) {
  RunGivenCommandListWaitsForEventsWhenHostAndCommandListSendsSignalsTest(
      *this, false);
}

TEST_F(
    zeEventSignalingTests,
    GivenImmediateCommandListWaitsForEventsWhenHostAndImmediateCommandListSendsSignalsThenImmediateCommandListExecutesSuccessfully) {
  RunGivenCommandListWaitsForEventsWhenHostAndCommandListSendsSignalsTest(*this,
                                                                          true);
}

void RunGivenEventsSignaledWhenResetTest(zeEventSignalingTests &test,
                                         bool is_immediate) {
  const ze_device_handle_t device = lzt::zeDevice::get_instance()->get_device();
  size_t num_event = 10;
  ASSERT_GE(num_event, 4);
  size_t num_loop = 4;
  ASSERT_GE(num_loop, 4);
  ze_context_handle_t context = lzt::create_context();
  test.ep.InitEventPool(context, 10);
  std::vector<ze_event_handle_t> device_event(num_event, nullptr);
  auto cmd_bundle = lzt::create_command_bundle(
      context, device, 0, ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS,
      ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, 0, 0, is_immediate);
  size_t loop_data_size = 300; // do not make this N*256
  ASSERT_TRUE(loop_data_size % 256);
  size_t copy_size = num_loop * loop_data_size;
  void *src_buff = lzt::allocate_host_memory(copy_size, 1, context);
  ze_device_mem_alloc_flags_t dev_flags{};
  ze_host_mem_alloc_flags_t host_flags{};
  void *dst_buff = lzt::allocate_shared_memory(copy_size, 1, dev_flags,
                                               host_flags, device, context);
  uint8_t *src_char = static_cast<uint8_t *>(src_buff);
  uint8_t *dst_char = static_cast<uint8_t *>(dst_buff);
  test.ep.create_events(device_event, num_event, ZE_EVENT_SCOPE_FLAG_HOST, 0);

  lzt::write_data_pattern(src_buff, copy_size, 1);
  lzt::write_data_pattern(dst_buff, copy_size, 0);
  for (size_t i = 0; i < num_loop; i++) {
    for (size_t j = 0; j < num_event; j++) {
      EXPECT_EQ(ZE_RESULT_NOT_READY, zeEventQueryStatus(device_event[j]));
    }
    lzt::append_signal_event(cmd_bundle.list, device_event[i]);
    lzt::append_wait_on_events(cmd_bundle.list, num_event, device_event.data());
    lzt::append_memory_copy(cmd_bundle.list, static_cast<void *>(dst_char),
                            static_cast<void *>(src_char), loop_data_size,
                            nullptr);
    lzt::append_reset_event(cmd_bundle.list, device_event[i]);
    lzt::close_command_list(cmd_bundle.list);
    if (!is_immediate) {
      lzt::execute_command_lists(cmd_bundle.queue, 1, &cmd_bundle.list,
                                 nullptr);
    }
    for (size_t j = 0; j < num_event; j++) {
      if (i != j) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventHostSignal(device_event[j]));
      }
    }
    if (is_immediate) {
      lzt::synchronize_command_list_host(cmd_bundle.list, UINT64_MAX);
    } else {
      lzt::synchronize(cmd_bundle.queue, UINT64_MAX);
    }

    for (size_t j = 0; j < num_event; j++) {
      if (i == j) {
        EXPECT_EQ(ZE_RESULT_NOT_READY, zeEventQueryStatus(device_event[j]));
      } else {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventQueryStatus(device_event[j]));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventHostReset(device_event[j]));
        EXPECT_EQ(ZE_RESULT_NOT_READY, zeEventQueryStatus(device_event[j]));
      }
    }
    src_char += loop_data_size;
    dst_char += loop_data_size;
    lzt::reset_command_list(cmd_bundle.list);
  }
  lzt::validate_data_pattern(dst_buff, copy_size, 1);

  lzt::destroy_command_bundle(cmd_bundle);
  lzt::free_memory(context, src_buff);
  lzt::free_memory(context, dst_buff);
  test.ep.destroy_events(device_event);
  lzt::destroy_context(context);
}

TEST_F(zeEventSignalingTests,
       GivenEventsSignaledWhenResetThenQueryStatusReturnsNotReady) {
  RunGivenEventsSignaledWhenResetTest(*this, false);
}

TEST_F(
    zeEventSignalingTests,
    GivenEventsSignaledWhenResetOnImmediateCmdListThenQueryStatusReturnsNotReady) {
  RunGivenEventsSignaledWhenResetTest(*this, true);
}

static void
multi_device_event_signal_read(std::vector<ze_device_handle_t> devices,
                               bool is_immediate) {
  ze_event_pool_desc_t ep_desc = {};
  ep_desc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;
  ep_desc.flags = 0;
  ep_desc.count = 10;
  ze_context_handle_t context = lzt::create_context();
  auto ep = lzt::create_event_pool(context, ep_desc, devices);
  ze_event_scope_flag_t flag = (ze_event_scope_flag_t)(
      ZE_EVENT_SCOPE_FLAG_DEVICE | ZE_EVENT_SCOPE_FLAG_SUBDEVICE);
  ze_event_desc_t event_desc = {};
  event_desc.stype = ZE_STRUCTURE_TYPE_EVENT_DESC;
  event_desc.index = 0;
  event_desc.signal = flag;
  event_desc.wait = flag;

  auto event = lzt::create_event(ep, event_desc);

  // dev0 signals
  {
    auto cmd_bundle = lzt::create_command_bundle(
        context, devices[0], 0, ZE_COMMAND_QUEUE_MODE_DEFAULT,
        ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, 0, 0, is_immediate);

    lzt::append_signal_event(cmd_bundle.list, event);
    lzt::close_command_list(cmd_bundle.list);
    lzt::execute_and_sync_command_bundle(cmd_bundle, UINT64_MAX);

    // cleanup
    lzt::destroy_command_bundle(cmd_bundle);
  }

  // all devices can read it.
  for (auto device : devices) {
    auto cmd_bundle = lzt::create_command_bundle(
        context, device, 0, ZE_COMMAND_QUEUE_MODE_DEFAULT,
        ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, 0, 0, is_immediate);

    lzt::append_wait_on_events(cmd_bundle.list, 1, &event);
    lzt::close_command_list(cmd_bundle.list);
    lzt::execute_and_sync_command_bundle(cmd_bundle, UINT64_MAX);

    // cleanup
    lzt::destroy_command_bundle(cmd_bundle);
  }

  lzt::destroy_event(event);
  event = lzt::create_event(ep, event_desc);

  // dev0 signals at kernel completion
  {
    auto cmd_bundle = lzt::create_command_bundle(
        context, devices[0], 0, ZE_COMMAND_QUEUE_MODE_DEFAULT,
        ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, 0, 0, is_immediate);
    size_t size = 10000;
    size_t buff_size = size * sizeof(int);
    auto src_buffer = lzt::allocate_host_memory(buff_size, 1, context);
    auto dst_buffer = lzt::allocate_host_memory(buff_size, 1, context);
    const int addval = 0x11223344;
    memset(src_buffer, 0, buff_size);
    auto module =
        lzt::create_module(context, devices[0], "profile_add.spv",
                           ZE_MODULE_FORMAT_IL_SPIRV, nullptr, nullptr);
    auto kernel = lzt::create_function(module, "profile_add_constant");
    lzt::set_group_size(kernel, 1, 1, 1);
    ze_group_count_t args = {static_cast<uint32_t>(size), 1, 1};
    lzt::set_argument_value(kernel, 0, sizeof(src_buffer), &src_buffer);
    lzt::set_argument_value(kernel, 1, sizeof(dst_buffer), &dst_buffer);
    lzt::set_argument_value(kernel, 2, sizeof(addval), &addval);
    lzt::append_launch_function(cmd_bundle.list, kernel, &args, event, 0,
                                nullptr);
    lzt::close_command_list(cmd_bundle.list);
    lzt::execute_and_sync_command_bundle(cmd_bundle, UINT64_MAX);

    // cleanup
    lzt::destroy_command_bundle(cmd_bundle);
    lzt::destroy_module(module);
    lzt::destroy_function(kernel);
    lzt::free_memory(context, src_buffer);
    lzt::free_memory(context, dst_buffer);
  }

  // all devices can read it.
  for (auto device : devices) {
    auto cmd_bundle = lzt::create_command_bundle(
        context, device, 0, ZE_COMMAND_QUEUE_MODE_DEFAULT,
        ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, 0, 0, is_immediate);

    lzt::append_wait_on_events(cmd_bundle.list, 1, &event);
    lzt::close_command_list(cmd_bundle.list);
    lzt::execute_and_sync_command_bundle(cmd_bundle, UINT64_MAX);

    // cleanup
    lzt::destroy_command_bundle(cmd_bundle);
  }

  lzt::destroy_event(event);
  lzt::destroy_event_pool(ep);
  lzt::destroy_context(context);
}

TEST(MultiDeviceEventTests,
     GivenMultipleDeviceEventPoolWhenSignalledFromOneDeviceThenAllDevicesRead) {
  auto devices = lzt::get_ze_devices();
  if (devices.size() < 2) {
    LOG_WARNING << "Less than two devices, skipping test";
  } else {
    multi_device_event_signal_read(devices, false);
  }
}

TEST(
    MultiDeviceEventTests,
    GivenMultipleDeviceEventPoolWhenSignalledFromOneDeviceOnImmediateCmdListThenAllDevicesRead) {
  auto devices = lzt::get_ze_devices();
  if (devices.size() < 2) {
    LOG_WARNING << "Less than two devices, skipping test";
  } else {
    multi_device_event_signal_read(devices, true);
  }
}

TEST(
    MultiDeviceEventTests,
    GivenMultipleSubDevicesEventPoolWhenSignalledFromOneSubDeviceThenAllSubDevicesRead) {
  auto devices = lzt::get_ze_devices();
  bool test_run = false;
  for (auto device : devices) {
    auto sub_devices = lzt::get_ze_sub_devices(device);
    if (sub_devices.size() < 2) {
      continue;
    }
    test_run = true;
    multi_device_event_signal_read(sub_devices, false);
  }
  if (!test_run)
    LOG_WARNING << "Less than two sub devices, skipping test";
}

TEST(
    MultiDeviceEventTests,
    GivenMultipleSubDevicesEventPoolWhenSignalledFromOneSubDeviceOnImmediateCmdListThenAllSubDevicesRead) {
  auto devices = lzt::get_ze_devices();
  bool test_run = false;
  for (auto device : devices) {
    auto sub_devices = lzt::get_ze_sub_devices(device);
    if (sub_devices.size() < 2) {
      continue;
    }
    test_run = true;
    multi_device_event_signal_read(sub_devices, true);
  }
  if (!test_run)
    LOG_WARNING << "Less than two sub devices, skipping test";
}

} // namespace
