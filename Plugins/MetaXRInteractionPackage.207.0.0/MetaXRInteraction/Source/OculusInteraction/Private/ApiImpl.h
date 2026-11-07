/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * Licensed under the Oculus SDK License Agreement (the "License");
 * you may not use the Oculus SDK except in compliance with the License,
 * which is provided at the time of installation or download, or which
 * otherwise accompanies this software in either electronic or hard copy form.
 *
 * You may obtain a copy of the License at
 *
 * https://developer.oculus.com/licenses/oculussdk/
 *
 * Unless required by applicable law or agreed to in writing, the Oculus SDK
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <functional>

/**
 * A template class implementing the lazy initialization (deferred creation) pattern for API
 * instances.
 *
 * FApiImpl manages the complete lifecycle of API objects by creating them on first access via
 * GetOrCreateInstance() and handling cleanup automatically in the destructor. This pattern is used
 * extensively throughout the OculusInteraction plugin to wrap native Interaction SDK (ISDK) API
 * types, ensuring that expensive API objects are only created when actually needed and that
 * creation failures are handled gracefully without repeated retry attempts.
 *
 * The class provides a virtual hook OnInstanceCreated() that derived classes can override to
 * perform additional initialization after the API instance is successfully created. This is useful
 * for setting up event handlers, configuring default values, or establishing connections to other
 * API objects.
 *
 * @tparam TApiType The underlying API type being managed (e.g., ExternalHandSource,
 * PokeInteractor). This is the type returned by reference or pointer from accessor methods.
 * @tparam TApiTypePtr A smart pointer type that owns the API instance (e.g.,
 * ExternalHandSourcePtr). Must support IsValid(), Get(), and Reset() methods compatible with
 * Unreal's smart pointer conventions.
 *
 * @see FEventQueueApiImpl For an example of a derived class that adds event queue handling
 * functionality.
 * @see FSelectorImpl For an example of a simple derived class that creates a Selector API instance.
 */
template <typename TApiType, typename TApiTypePtr>
class FApiImpl
{
 public:
  /**
   * Constructs an FApiImpl with a factory function for creating the API instance.
   *
   * The factory function is stored and will be invoked lazily when GetOrCreateInstance() is first
   * called. This allows the actual API object creation to be deferred until it is needed, which can
   * improve startup performance and avoid creating objects that may never be used.
   *
   * @param InCreateFn A callable that creates and returns a new TApiTypePtr instance. This function
   *                   should return a valid smart pointer on success, or an invalid/null pointer on
   *                   failure. The function may capture context needed for creation (e.g., 'this'
   *                   pointer, configuration parameters).
   */
  FApiImpl(std::function<TApiTypePtr()> InCreateFn) : CreateFn(std::move(InCreateFn)) {}

  /**
   * Virtual destructor that ensures proper cleanup of the managed API instance.
   *
   * Calls DestroyInstance() to release the underlying API object before the FApiImpl is destroyed.
   * This guarantees that API resources are properly released even if DestroyInstance() was not
   * explicitly called by the owner.
   */
  virtual ~FApiImpl()
  {
    DestroyInstance();
  }

  /**
   * Returns the managed API instance, creating it if necessary using lazy initialization.
   *
   * This is the primary access method for obtaining the API instance. On first call, it invokes
   * the factory function provided in the constructor to create the instance. Subsequent calls
   * return the cached instance without re-creation. If creation fails, the failure is recorded
   * and no further creation attempts are made until DestroyInstance() is called to reset the state.
   *
   * After successful creation, OnInstanceCreated() is called to allow derived classes to perform
   * additional initialization on the new instance.
   *
   * @return A pointer to the API instance if valid, or nullptr if the instance could not be created
   *         or if a previous creation attempt failed. Callers should check for nullptr before use.
   *
   * @see GetInstanceChecked For access when the instance is guaranteed to exist.
   * @see OnInstanceCreated For performing post-creation initialization in derived classes.
   */
  TApiType* GetOrCreateInstance()
  {
    if (Instance.IsValid())
    {
      return &Instance.Get();
    }
    if (!bCreateInstanceFailed)
    {
      Instance = CreateFn();
      if (Instance.IsValid())
      {
        OnInstanceCreated(&Instance.Get());
        return &Instance.Get();
      }
      bCreateInstanceFailed = true;
    }
    return nullptr;
  }

  /**
   * Returns a reference to the managed API instance with an assertion check.
   *
   * Use this method when you are certain the instance has already been created and is valid.
   * This is more efficient than GetOrCreateInstance() when the instance is guaranteed to exist,
   * as it avoids the creation logic overhead. However, it will trigger a check assertion failure
   * if the instance is not valid, which helps catch programming errors during development.
   *
   * Prefer GetOrCreateInstance() when the instance may not exist yet or when graceful handling
   * of missing instances is required. Use GetInstanceChecked() in code paths that are only
   * executed after successful instance creation has been verified.
   *
   * @return A reference to the valid API instance.
   *
   * @see GetOrCreateInstance For safe access that handles instance creation.
   * @see IsInstanceValid For checking instance validity before calling this method.
   */
  TApiType& GetInstanceChecked()
  {
    check(Instance.IsValid());
    return Instance.Get();
  }

  /**
   * Virtual hook called immediately after a new API instance is successfully created.
   *
   * Derived classes can override this method to perform additional initialization on the newly
   * created instance, such as configuring default settings, registering event handlers, or
   * establishing connections to other API objects. The base implementation does nothing.
   *
   * This method is only called when creation succeeds; it is not called if the factory function
   * returns an invalid instance.
   *
   * @param NewInstance A pointer to the newly created API instance. Guaranteed to be valid
   *                    (non-null and pointing to a valid object) when this method is called.
   *
   * @see FInteractorStateEventQueueImpl::OnInstanceCreated For an example that sets event type
   * hints.
   */
  virtual void OnInstanceCreated(TApiType* NewInstance) {}

  /**
   * Destroys the current API instance and resets the creation failure tracking state.
   *
   * After calling this method, the next call to GetOrCreateInstance() will attempt to create
   * a new instance using the factory function, even if a previous creation attempt had failed.
   * This allows for retry logic when the conditions that caused the initial failure may have
   * been resolved (e.g., required dependencies are now available).
   *
   * This method is automatically called by the destructor, but can also be called explicitly
   * to release resources early or to force re-creation of the API instance.
   *
   * @see GetOrCreateInstance For the method that will re-create the instance after destruction.
   */
  void DestroyInstance()
  {
    Instance.Reset();
    bCreateInstanceFailed = false;
  }

  /**
   * Checks whether a valid API instance currently exists.
   *
   * This method can be used to determine if the instance has been created and is still valid
   * without triggering creation. It is useful for conditional logic that should only execute
   * when an instance exists, or for validation before calling GetInstanceChecked().
   *
   * @return True if the managed instance exists and is valid, false otherwise. A false return
   *         does not necessarily mean creation would fail; it may simply mean GetOrCreateInstance()
   *         has not been called yet.
   *
   * @see GetOrCreateInstance For obtaining the instance (creating if necessary).
   * @see GetInstanceChecked For access when validity has been confirmed.
   */
  bool IsInstanceValid() const
  {
    return Instance.IsValid();
  }

 private:
  /// The factory function used to create new API instances. Called by GetOrCreateInstance()
  /// when no valid instance exists and no previous creation attempt has failed.
  std::function<TApiTypePtr()> CreateFn;

  /// The managed API instance smart pointer. Holds the created instance after successful
  /// creation via GetOrCreateInstance(), or remains invalid if creation has not occurred
  /// or has failed.
  TApiTypePtr Instance{};

  /// Tracks whether a previous creation attempt has failed. When true, GetOrCreateInstance()
  /// will not attempt to create a new instance and will return nullptr immediately. Reset
  /// to false by DestroyInstance() to allow retry attempts.
  bool bCreateInstanceFailed{};
};
