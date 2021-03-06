// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_MESSAGE_PIPE_ENDPOINT_H_
#define MOJO_SYSTEM_MESSAGE_PIPE_ENDPOINT_H_

#include "base/basictypes.h"
#include "mojo/public/system/core.h"

namespace mojo {
namespace system {

class Waiter;

// This is an interface to one of the ends of a message pipe, and is used by
// |MessagePipe|. Its most important role is to provide a sink for messages
// (i.e., a place where messages can be sent). It has a secondary role: When the
// endpoint is local (i.e., in the current process), there'll be a dispatcher
// corresponding to the endpoint. In that case, the implementation of
// |MessagePipeEndpoint| also implements the functionality required by the
// dispatcher, e.g., to read messages and to wait. Implementations of this class
// are not thread-safe; instances are protected by |MesssagePipe|'s lock.
class MessagePipeEndpoint {
 public:
  virtual ~MessagePipeEndpoint() {}

  // All implementations must implement these.
  virtual void OnPeerClose() = 0;
  virtual MojoResult EnqueueMessage(
      const void* bytes, uint32_t num_bytes,
      const MojoHandle* handles, uint32_t num_handles,
      MojoWriteMessageFlags flags) = 0;

  // Implementations must override these if they represent a local endpoint,
  // i.e., one for which there's a |MessagePipeDispatcher| (and thus a handle).
  // An implementation for a remote endpoint (for which there's no dispatcher)
  // needs not override these methods, since they should never be called.
  //
  // These methods implement the methods of the same name in |MessagePipe|,
  // though |MessagePipe|'s implementation may have to do a little more if the
  // operation involves both endpoints.
  virtual void CancelAllWaiters();
  virtual void Close();
  virtual MojoResult ReadMessage(void* bytes, uint32_t* num_bytes,
                                 MojoHandle* handles, uint32_t* num_handles,
                                 MojoReadMessageFlags flags);
  virtual MojoResult AddWaiter(Waiter* waiter,
                               MojoWaitFlags flags,
                               MojoResult wake_result);
  virtual void RemoveWaiter(Waiter* waiter);

 protected:
  MessagePipeEndpoint() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MessagePipeEndpoint);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_MESSAGE_PIPE_ENDPOINT_H_
