/*
 * Copyright 2018 The Project Oak Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef OAK_SERVER_OAK_RUNTIME_H_
#define OAK_SERVER_OAK_RUNTIME_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "include/grpcpp/security/server_credentials.h"
#include "include/grpcpp/server.h"
#include "oak/proto/application.pb.h"
#include "oak/proto/oak_api.pb.h"
#include "oak/server/base_runtime.h"
#include "oak/server/oak_grpc_node.h"
#include "oak/server/storage/storage_node.h"

namespace oak {
// OakRuntime contains the common runtime needed for an Oak System. The Runtime is responsible for
// Initializing and Running a gRPC server, creating the nodes and channels and keeping track of
// the connectivity. For now, it only supports one node.
//
// It can run in its own enclave, but this is optional.

class OakRuntime : public BaseRuntime {
 public:
  OakRuntime()
      : grpc_node_(nullptr),
        grpc_handle_(kInvalidHandle),
        app_node_(nullptr),
        app_handle_(kInvalidHandle),
        termination_pending_(false) {}
  virtual ~OakRuntime() = default;

  // Initializes an OakRuntime with a user-provided ApplicationConfiguration. This
  // method should be called exactly once, before Start().
  grpc::Status Initialize(const ApplicationConfiguration& config,
                          std::shared_ptr<grpc::ServerCredentials> grpc_credentials)
      LOCKS_EXCLUDED(mu_);
  grpc::Status Start();
  grpc::Status Stop();

  bool CreateAndRunNode(const std::string& config_name, const std::string& entrypoint_name,
                        std::unique_ptr<ChannelHalf> half, std::string* node_name) override;

  bool TerminationPending() override { return termination_pending_.load(); }

  int32_t GetPort();

 private:
  OakRuntime& operator=(const OakRuntime& other) = delete;

  std::string NextNodeName(const std::string& config_name, const std::string& entrypoint_name)
      EXCLUSIVE_LOCKS_REQUIRED(mu_);

  OakNode* CreateNode(const std::string& config_name, const std::string& entrypoint_name,
                      std::string* node_name) EXCLUSIVE_LOCKS_REQUIRED(mu_);

  // Information derived from ApplicationConfiguration; const after Initialize() called:

  // Collection of Wasm configuration info indexed by config name.
  std::map<std::string, std::unique_ptr<const WebAssemblyConfiguration>> wasm_config_;
  // Config names that refer to a logging node.
  std::set<std::string> log_config_;
  // Config names that refer to a storage proxy node.
  std::map<std::string, std::unique_ptr<std::string>> storage_config_;
  // Config names that refer to a gRPC client node.
  std::map<std::string, std::unique_ptr<std::string>> grpc_client_config_;

  // Convenience (non-owning) reference to gRPC pseudo-node.
  OakGrpcNode* grpc_node_;
  // Handle for the write half of the gRPC server notification channel, relative
  // to the gRPC server pseudo-Node
  Handle grpc_handle_;
  // Convenience (non-owning) reference to initial Application Wasm node;
  OakNode* app_node_;
  // Handle for the read half of the gRPC server notification channel, relative
  // to the initial Application Wasm Node.
  Handle app_handle_;

  // Next index for node name generation.
  mutable absl::Mutex mu_;  // protects nodes_, next_index_;
  std::map<std::string, int> next_index_ GUARDED_BY(mu_);

  std::atomic_bool termination_pending_;

  // Collection of running Nodes indexed by Node name.  Note that Node name is
  // unique but is not visible to the running Application in any way.
  std::map<std::string, std::unique_ptr<OakNode>> nodes_ GUARDED_BY(mu_);

};  // class OakRuntime

}  // namespace oak

#endif  // OAK_SERVER_OAK_RUNTIME_H_
